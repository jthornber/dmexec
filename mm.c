#include "mm.h"

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


// I'm using a slab allocator combined with mark and lazy sweep collector.
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
// Blocks are fixed size chunks of memory that get divided up into objects.
// Blocks are assigned to Slabs as they need the memory.
//
// Allocation from a block uses a free bitset, rather than a free list, which
// will hopefully show better cache coherency.
//
// Blocks need to different offsets for the first object to avoid cache
// collisions.

static void fail_(const char *msg)
{
	fprintf(stderr, "%s", msg);
	exit(1);
}

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
			     MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (ca->mem_begin == MAP_FAILED)
		fail_("mmap failed, can't do anything without memory");
	ca->mem_end = ca->mem_begin + mem_size;

	INIT_LIST_HEAD(&ca->free);
	for (ptr = ca->mem_begin; ptr != ca->mem_end; ptr++) {
		struct list_head *tmp = ptr;
		list_add(tmp, &ca->free);
	}
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

#if 0

#define BLOCK_SIZE (16 * 1024)

typedef struct {
	struct list_head list;
	ObjectType type;
	uint32_t first_obj;
	uint32_t alloc_bits[];
} Block;

typedef struct {
	struct list_head blocks;
	ObjectType type;
} Slab;

static struct list_head *unused_blocks;

#endif

// FIXME: this is going since type and size will be in the block
typedef struct {
       ObjectType type;
       unsigned size;          /* in bytes, we always round to a 4 byte boundary */
} Header;

static MemoryStats memory_stats_;

void mm_init()
{
	// Grab a big chunk of memory, Block aligned.
}

void mm_exit()
{
	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) get_memory_stats()->total_allocated);
}

static void out_of_memory(void)
{
	error("out of memory");
}

static void *untyped_alloc_(size_t s)
{
	memory_stats_.total_allocated += s;
	return malloc(s);
}

void *mm_alloc(ObjectType type, size_t s)
{
	size_t len = s + sizeof(Header);

	// Also zeroes memory
	Header *h = untyped_alloc_(len);

	if (!h)
		out_of_memory();

	h->type = type;
	h->size = s;

	return h + 1;
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
	Header *h = obj_to_header(obj);
	void *new = mm_alloc(h->type, h->size);

	memcpy(new, obj, h->size);
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
	return obj_to_header(obj)->type;
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
		"cons",
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
