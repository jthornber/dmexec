#ifndef DMEXEC_SLAB_H
#define DMEXEC_SLAB_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "list.h"

//----------------------------------------------------------------

#define CHUNK_SIZE (16 * 1024)
typedef struct chunk_allocator__ ChunkAllocator;

void ca_init(ChunkAllocator *ca, size_t chunk_size, size_t mem_size);
void ca_exit(ChunkAllocator *ca);
void *ca_alloc(ChunkAllocator *ca);
void ca_free(ChunkAllocator *ca, void *ptr);

extern ChunkAllocator global_allocator_;

//----------------------------------------------------------------

typedef struct slist__ {
	struct slist__ *next;
} SList;


#define MAX_FREE 32

typedef struct {
	const char *name;
	struct list_head full_chunks;
	struct list_head chunks;
	void *free[MAX_FREE];
	unsigned free_end;

	uint16_t type;
	uint16_t obj_size;
	size_t bitset_size; // in bytes
	unsigned objs_per_chunk;

	unsigned nr_chunks;
	unsigned nr_allocs;
} Slab;

// This works for interior pointers too, so that we can cope with generic slabs
// that have objects prepended with a header.
void slab_init(Slab *s, const char *name, uint16_t type, unsigned obj_size);
void slab_exit(Slab *s);
void slab_populate_free_list(Slab *s);
void slab_clear_marks(Slab *s);
void slab_return_unused_chunks(Slab *s);

typedef struct chunk__ {
	struct list_head list;
	Slab *owner;
	void *objects;
	uint32_t search_start;
	bool unused;
	uint32_t marks[0];
} Chunk;

typedef struct {
	Chunk *c;
	unsigned index;
} ChunkAddress;

ChunkAddress ca_address(void *obj);
void ca_mark(ChunkAddress addr);
bool ca_marked(ChunkAddress addr);

static inline void *slab_alloc(Slab *s)
{
	if (!s->free_end)
		slab_populate_free_list(s);

	return s->free[--s->free_end];
}

static inline void *slab_clone(Slab *s, void *ptr, size_t len)
{
	void *new = slab_alloc(s);
	memcpy(new, ptr, len);
	return new;
}

//----------------------------------------------------------------

#endif
