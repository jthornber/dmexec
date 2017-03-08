#include "mm.h"

#include <assert.h>
#include <gc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

#include "error.h"
#include "list.h"
#include "types.h"
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

struct chunk_allocator__ {
	size_t chunk_size;
	void *mem_begin, *mem_end;
	struct list_head free;

	// this is the total nr of times alloc has been called, frees are not
	// taken into account.  Used to see if we need a GC.
	unsigned nr_allocs;
};

void ca_init(ChunkAllocator *ca, size_t chunk_size, size_t mem_size)
{
	void *ptr;

	// Adjust mem_size to be a multiple of the chunk size
	mem_size = chunk_size * (mem_size / chunk_size);

	ca->chunk_size = chunk_size;
	ca->mem_begin = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
			     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (ca->mem_begin == MAP_FAILED)
		fail_("mmap failed, can't do anything without memory\n");
	ca->mem_end = ca->mem_begin + mem_size;
	ca->mem_begin = mem_align(ca->mem_begin, chunk_size);

	INIT_LIST_HEAD(&ca->free);
	for (ptr = ca->mem_begin; (ptr + chunk_size) <= ca->mem_end; ptr += chunk_size)
		list_add((struct list_head *) ptr, &ca->free);

	ca->nr_allocs = 0;
}

void ca_exit(ChunkAllocator *ca)
{
	// FIXME: finish
}

void *ca_alloc(ChunkAllocator *ca)
{
	void *ptr;

	if (list_empty(&ca->free))
		fail_("out of memory");

	ca->nr_allocs++;
	ptr = ca->free.next;
	list_del(ptr);

	memset(ptr, 0xba, ca->chunk_size);
	return ptr;
}

void ca_free(ChunkAllocator *ca, void *ptr)
{
	struct list_head *tmp = ptr;
	memset(ptr, 0xde, ca->chunk_size);
	list_add(tmp, &ca->free);
}

ChunkAllocator global_allocator_;

//----------------------------------------------------------------
// Slab
//----------------------------------------------------------------

struct slab__ {
	const char *name;
	struct list_head full_chunks;
	struct list_head chunks;

	uint16_t type;
	uint16_t obj_size;
	size_t bitset_size; // in bytes
	unsigned objs_per_chunk;

	unsigned nr_chunks;
	unsigned nr_allocs;
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
	c->search_start = 0;
	c->unused = true;
}

// This works for interior pointers too, so that we can cope with generic slabs
// that have objects prepended with a header.
ChunkAddress ca_address(void *obj)
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

void ca_mark(ChunkAddress addr)
{
	set_bit_(addr.c->marks, addr.index);
	addr.c->unused = false;
}

bool ca_marked(ChunkAddress addr)
{
	return test_bit_(addr.c->marks, addr.index);
}

static void new_chunk_(Slab *s)
{
	Chunk *c = ca_alloc(&global_allocator_);

	c->owner = s;
	c->objects = ((void *) (c + 1)) + s->bitset_size;
	clear_marks_(c);
	s->nr_chunks++;
	list_add(&c->list, &s->chunks);
}

void slab_init(Slab *s, const char *name, uint16_t type, unsigned obj_size)
{
	s->name = name;
	INIT_LIST_HEAD(&s->full_chunks);
	INIT_LIST_HEAD(&s->chunks);
	s->free_list = NULL;
	s->type = type;
	s->obj_size = obj_size;
	s->objs_per_chunk = calc_nr_objects_(obj_size);
	s->bitset_size = calc_bitset_words_(s->objs_per_chunk) * sizeof(uint32_t);
	s->nr_chunks = 0;
	s->nr_allocs = 0;
}

void slab_exit(Slab *s)
{
	struct list_head *entry, *tmp;
	fprintf(stderr, "%s: chunks allocated = %u (%um), nr allocated = %u\n",
		s->name, s->nr_chunks, (s->nr_chunks * CHUNK_SIZE) / (1024 * 1024),
		s->nr_allocs);
	list_splice_init(&s->full_chunks, &s->chunks);
	list_for_each_safe (entry, tmp, &s->chunks)
		ca_free(&global_allocator_, entry);
}

static unsigned find_first_clear(uint32_t *words, unsigned b, unsigned e)
{
	unsigned wb = b / 32;
	unsigned we = div_up(e, 32);
	unsigned bit;

	while (wb != we) {
		bit = __builtin_ffs(~words[wb]);
		if (bit)
			return (wb * 32) + bit - 1;
		wb++;
	}

	return e;
}

static void *alloc_one_(Slab *s)
{
	ChunkAddress addr;

	if (list_empty(&s->chunks))
		new_chunk_(s);

	addr.c = (Chunk *) s->chunks.next;

	addr.index = find_first_clear(addr.c->marks,
				      addr.c->search_start, s->objs_per_chunk);

	if (addr.index >= s->objs_per_chunk) {
		list_move(&addr.c->list, &s->full_chunks);
		return NULL;
	}

	assert(!ca_marked(addr));
	ca_mark(addr);
	addr.c->search_start = addr.index + 1;
	s->nr_allocs++;
	return addr.c->objects + (addr.index * s->obj_size);
}

#define POP_TARGET 32

void slab_populate_free_list(Slab *s)
{
	SList *ptr;
	unsigned count = POP_TARGET;

	while (count) {
		ptr = alloc_one_(s);
		if (ptr) {
			ptr->next = s->free_list;
			s->free_list = ptr;
			count--;

		} else if (s->free_list)
			// We don't want to span more than one chunk.
			break;
	}
}

void slab_clear_marks(Slab *s)
{
	Chunk *c;

	list_splice_init(&s->full_chunks, &s->chunks);
	list_for_each_entry (c, &s->chunks, list)
		clear_marks_(c);

	s->free_list = NULL;
}

void slab_return_unused_chunks(Slab *s)
{
	Chunk *c, *tmp;

	list_for_each_entry_safe (c, tmp, &s->chunks, list)
		if (c->unused) {
			unsigned i;
			ChunkAddress addr;

			addr.c = c;
			for (i = 0; i < s->objs_per_chunk; i++) {
				addr.index = i;
				assert(!ca_marked(addr));
			}
			s->nr_chunks--;
			list_del(&c->list);
			ca_free(&global_allocator_, c);
		}
}

//----------------------------------------------------------------
