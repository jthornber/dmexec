#include "mm.h"

#include <assert.h>
#include <gc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

#define HEADER_MAGIC 846219U

#include "error.h"
#include "list.h"
#include "string_type.h"

//----------------------------------------------------------------
// Memory manager


// I'm using slab allocators combined with mark and lazy sweep collector.
//
// There are separate slabs for each object type.  This lets us avoid storing
// a header for every object, saving a lot of memory and simplifying the
// granularity for the mark bitsets.
//
// Mark and sweep avoids copying, but may suffer from fragmentation.  I'm
// hoping that since we use so much immutable data the fragmentation wont be so
// bad.
//
// Marking is performed in a bitset allocated on the side.  Better cache
// locality.
//
// Chunks are fixed size chunks of memory that get divided up into objects.
// Chunks are assigned to Slabs as they need the memory.
//
// Allocation from a chunk uses a free bitset, rather than a free list, which
// will hopefully show better cache coherency.
//
// Chunks need to allocate from different offsets for the first object to avoid
// cache collisions.

static void fail_(const char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(1);
}

static size_t div_up(size_t n, size_t d)
{
	return (n + (d - 1)) / d;
}

// align must be a power of 2
static void *mem_align(void *ptr, size_t align)
{
	intptr_t offset = align - 1;
	intptr_t mask = ~offset;
	return (void *) ((((intptr_t) ptr) + offset) & mask);
}

//----------------------------------------------------------------
// Chunk allocator
//----------------------------------------------------------------

typedef struct {
	void *mem_begin, *mem_end;
	struct list_head free;
} ChunkAllocator;

static void ca_init_(ChunkAllocator *ca, size_t chunk_size, size_t mem_size)
{
	void *ptr;

	// Adjust mem_size to be a multiple of the chunk size
	mem_size = chunk_size * (mem_size / chunk_size);

	ca->mem_begin = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ca->mem_begin == MAP_FAILED)
		fail_("mmap failed, can't do anything without memory\n");
	ca->mem_end = ca->mem_begin + mem_size;
	ca->mem_begin = mem_align(ca->mem_begin, chunk_size);

	INIT_LIST_HEAD(&ca->free);
	for (ptr = ca->mem_begin; (ptr + chunk_size) <= ca->mem_end; ptr += chunk_size)
		list_add((struct list_head *) ptr, &ca->free);
}

static void ca_exit_(ChunkAllocator *ca)
{
	// FIXME: finish
}

static void *ca_alloc_(ChunkAllocator *ca)
{
	void *ptr;

	if (list_empty(&ca->free))
		fail_("out of memory");

	ptr = ca->free.next;
	list_del(ptr);

	return ptr;
}

static void ca_free_(ChunkAllocator *ca, void *ptr)
{
	struct list_head *tmp = ptr;
	list_add(tmp, &ca->free);
}

static bool ca_within_heap_(ChunkAllocator *ca, void *ptr)
{
	return (ptr >= ca->mem_begin) && (ptr < ca->mem_end);
}

static ChunkAllocator global_allocator_;

//----------------------------------------------------------------
// Slab
//----------------------------------------------------------------

#define CHUNK_SIZE (16 * 1024)

typedef struct slab__ Slab;

typedef struct {
	struct list_head list;
	Slab *owner;
	void *objects;
	uint32_t last_alloc;
	uint32_t marks[0];
} Chunk;

typedef struct {
	Chunk *c;
	unsigned index;
} ChunkAddress;

struct slab__ {
	struct list_head full_chunks;
	Chunk *current;
	ObjectType type;
	size_t obj_size;
	size_t bitset_size; // in bytes
	unsigned objs_per_chunk;
};

static unsigned calc_bitset_words_(unsigned nr_bits)
{
	return div_up(nr_bits, 32);
}

static uint32_t calc_nr_objects_(size_t obj_size)
{
	size_t nr_objs = (CHUNK_SIZE - sizeof(Chunk)) / obj_size;
	size_t nr_lost_to_bitset = div_up(sizeof(uint32_t) * calc_bitset_words_(nr_objs),
					  obj_size);
	return nr_objs - nr_lost_to_bitset;
}

static void clear_marks_(Chunk *c)
{
	memset(c->marks, 0, c->owner->bitset_size);
}

static ChunkAddress obj_address(void *obj)
{
	intptr_t mask = ~(((intptr_t) CHUNK_SIZE) - 1);
	Chunk *c = (Chunk *) (((intptr_t) obj) & mask);
	unsigned index = ((unsigned) (obj - c->objects)) / c->owner->obj_size;

	return (ChunkAddress) {c, index};
}

static void set_bit_(uint32_t *words, unsigned index)
{
	words[index / 32] |= (1 << (index & 31));
}

static bool test_bit_(uint32_t *words, unsigned index)
{
	return words[index / 32] & (1 << (index & 31));
}

static void mark_(ChunkAddress addr)
{
	set_bit_(addr.c->marks, addr.index);
}

static bool marked_(ChunkAddress addr)
{
	return test_bit_(addr.c->marks, addr.index);
}

static void new_chunk_(Slab *s)
{
	if (s->current)
		list_add(&s->current->list, &s->full_chunks);

	s->current = ca_alloc_(&global_allocator_);
	s->current->owner = s;
	s->current->objects = ((void *) (s->current + 1)) + s->bitset_size;
	s->current->last_alloc = 0;
	clear_marks_(s->current);
}

static void slab_init_(Slab *s, ObjectType type, unsigned obj_size)
{
	INIT_LIST_HEAD(&s->full_chunks);
	s->current = NULL;
	s->type = type;
	s->obj_size = obj_size;
	s->objs_per_chunk = calc_nr_objects_(obj_size);
	s->bitset_size = calc_bitset_words_(s->objs_per_chunk) * sizeof(uint32_t);

	new_chunk_(s);
}

static void slab_exit_(Slab *s)
{
	struct list_head *entry, *tmp;
	list_for_each_safe (entry, tmp, &s->full_chunks)
		ca_free_(&global_allocator_, entry);
	ca_free_(&global_allocator_, s->current);
}

static void *slab_alloc_(Slab *s, size_t len)
{
	void *r;
	ChunkAddress addr;

	addr.c = s->current;

	assert(len == s->obj_size);

	// FIXME: clearly slow
	for (addr.index = addr.c->last_alloc; addr.index < addr.c->owner->objs_per_chunk; addr.index++) {
		if (!marked_(addr)) {
			mark_(addr);
			r = addr.c->objects + (addr.index * addr.c->owner->obj_size);
			addr.c->last_alloc = addr.index;
			break;
		}
	}

	if (addr.index == (addr.c->owner->objs_per_chunk - 1))
		new_chunk_(s);

	assert(ca_within_heap_(&global_allocator_, r));
	return r;
}

//----------------------------------------------------------------

// FIXME: this is going since type and size will be in the block
typedef struct {
       ObjectType type;
       unsigned size;          /* in bytes, we always round to a 4 byte boundary */
} Header;

static Slab vblock_slab_;
static MemoryStats memory_stats_;

void mm_init(size_t mem_size)
{
	ca_init_(&global_allocator_, CHUNK_SIZE, mem_size);

	// FIXME: where are we going to get the object sizes from?
	slab_init_(&vblock_slab_, VBLOCK, sizeof(Value) * 16);
}

void mm_exit()
{
	slab_exit_(&vblock_slab_);
	ca_exit_(&global_allocator_);
	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) get_memory_stats()->total_allocated);
}

static void out_of_memory(void)
{
	error("out of memory");
}

void *mm_alloc(ObjectType type, size_t s)
{
	memory_stats_.total_allocated += s;
	if (type == VBLOCK)
		return slab_alloc_(&vblock_slab_, s);

	else {
		size_t len = s + sizeof(Header);

		// Also zeroes memory
		Header *h = malloc(len);

		if (!h)
			out_of_memory();

		h->type = type;
		h->size = s;

		return h + 1;
	}
}

void *mm_zalloc(ObjectType type, size_t s)
{
	void *ptr = mm_alloc(type, s);
	memset(ptr, 0, s);
	return ptr;
}

static Header *obj_to_header(void *obj)
{
	return ((Header *) obj) - 1;
}

void *mm_clone(void *obj)
{
	// FIXME: once everything is allocated from a slab get the slab rather
	// than calling get_obj*
	void *new = mm_alloc(get_obj_type(obj), get_obj_size(obj));
	memcpy(new, obj, get_obj_size(obj));

	return new;
}

void *as_ref(Value v)
{
	if (get_tag(v) != TAG_REF)
		error("type error: value is not a reference.");
	return v.ptr;
}

ObjectType get_obj_type(void *obj)
{
	if (ca_within_heap_(&global_allocator_, obj))
		return obj_address(obj).c->owner->type;
	else
		return obj_to_header(obj)->type;
}

size_t get_obj_size(void *obj)
{
	if (ca_within_heap_(&global_allocator_, obj))
		return obj_address(obj).c->owner->obj_size;
	else
		return obj_to_header(obj)->size;
}

ObjectType get_type(Value v)
{
	if (get_tag(v) == TAG_FIXNUM)
		return FIXNUM;

	if (get_tag(v) == TAG_NIL)
		return NIL;

	return get_obj_type(as_ref(v));
}

//----------------------------------------------------------------
// Values - immediate or reference
//
// The bottom 2 bits are used for tagging.

Tag get_tag(Value v)
{
	return v.i & 0x3;
}

Value mk_fixnum(int i)
{
	Value v;
	v.i = (i << 2) | TAG_FIXNUM;
	return v;
}

int as_fixnum(Value v)
{
	if (get_tag(v) != TAG_FIXNUM)
		error("type error: expected fixnum.");
	return v.i >> 2;
}

Value mk_ref(void *ptr)
{
	Value v;
	v.ptr = ptr;
	return v;
}

Value clone_value(Value v)
{
	return mk_ref(mm_clone(as_ref(v)));
}

Value mk_nil()
{
	Value v;
	v.i = TAG_NIL;
	return v;
}

bool is_false(Value v)
{
	return v.i == NIL;
}

static const char *type_desc(ObjectType t)
{
	// FIXME: out of date
	static const char *strs[] = {
		"forward",
		"primitive",
		"closure",
		"string",
		"symbol",
		"nil",
		"fixnum"
	};

	return strs[t];
}

void *as_type(ObjectType t, Value v)
{
	if (get_type(v) != t)
		error("type error: expected type '%s'.", type_desc(t));

	return as_ref(v);
}

//----------------------------------------------------------------

MemoryStats *get_memory_stats()
{
	return &memory_stats_;
}

//----------------------------------------------------------------
