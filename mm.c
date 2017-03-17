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

//----------------------------------------------------------------

// We use this data structure to manage the order that we walk data for the
// mark phase.  If we use a stack, then we'll traverse in depth first order,
// and a queue will be breadth first.  Since we're not copying (yet) the
// traversal order doesn't really have any effect on future data locality.  So
// our primary concern is cache locality of the mark bits.  Using a stack for
// now.  We can't use standard data structures like Vector because this is used
// whilst we're running a garbage collection.

typedef struct {
	struct list_head list;
	Value *b, *e, *current;
} ValueChunk;

typedef struct {
	struct list_head chunks;
} Traversal;

static void new_vc_(Traversal *tv)
{
	ValueChunk *vc = ca_alloc(&global_allocator_);
	list_add(&vc->list, &tv->chunks);
	vc->b = (Value *) (vc + 1);
	vc->e = vc->b + ((CHUNK_SIZE - sizeof(ValueChunk)) / sizeof(Value));
	vc->current = vc->b;
}

static void trav_init_(Traversal *tv)
{
	INIT_LIST_HEAD(&tv->chunks);
}

static bool trav_empty_(Traversal *tv)
{
	return list_empty(&tv->chunks);
}

static ValueChunk *trav_current_chunk(Traversal *tv)
{
	return (ValueChunk *) tv->chunks.next;
}

static void trav_push_(Traversal *tv, Value v)
{
	ValueChunk *vc;

	if (trav_empty_(tv))
		new_vc_(tv);

	vc = trav_current_chunk(tv);
	if (vc->current == vc->e) {
		new_vc_(tv);
		vc = trav_current_chunk(tv);
	}

	*vc->current = v;
	vc->current++;
}

static Value trav_pop_(Traversal *tv)
{
	Value v;
	ValueChunk *vc;

	if (trav_empty_(tv))
		fail_("empty traversal");

	vc = trav_current_chunk(tv);
	vc->current--;
	v = *vc->current;

	if (vc->current == vc->b) {
		list_del(&vc->list);
		ca_free(&global_allocator_, vc);
	}

	return v;
}

//----------------------------------------------------------------

static bool is_immediate_(Value v)
{
	return get_tag(v) != TAG_REF;
}

static void mark_value_(Traversal *tv, Value v)
{
	if (is_immediate_(v))
		return;

	else if (!v.ptr) {
		fprintf(stderr, "walked a null ptr\n");
		return;

	} else {
		ChunkAddress addr = ca_address(v.ptr);
		if (!ca_marked(addr)) {
			ca_mark(addr);
			trav_push_(tv, v);
		}
	}
}

static void walk_he_(Traversal *tv, HashEntry *he)
{
	if (get_type(he->val) != HBLOCK)
		mark_value_(tv, he->key);
	mark_value_(tv, he->val);
}

static void walk_one_(Traversal *tv, Value v)
{
	switch (get_type(v)) {
	case PRIMITIVE:
	case CLOSURE:
	case STRING:
	case SYMBOL:
		break;

	case CONS: {
		Cons *cell = v.ptr;
		mark_value_(tv, cell->car);
		mark_value_(tv, cell->cdr);
		break;
	}

	case NIL:
		break;

	case VECTOR: {
		Vector *vec = v.ptr;
		mark_value_(tv, mk_ref(vec->root));
		mark_value_(tv, mk_ref(vec->cursor));
		break;
	}

	case VBLOCK: {
		unsigned i;
		VBlock vb = v.ptr;
		for (i = 0; i < ENTRIES_PER_VBLOCK; i++)
			mark_value_(tv, vb[i]);
		break;
	}

	case HTABLE: {
		HashTable *ht = v.ptr;
		if (ht->nr_entries)
			walk_he_(tv, &ht->root);
		break;
	}

	case HBLOCK: {
		unsigned i;
		HBlock hb = v.ptr;
		unsigned nr_entries = get_obj_size(hb) / sizeof(HashEntry);
		for (i = 0; i < nr_entries; i++)
			walk_he_(tv, hb + i);
		break;
	}

	case FRAME:
	case STATIC_FRAME:
	case STATIC_ENV:
	case THUNK:
	case RAW:
	case FIXNUM:
	     break;
	}
}

static void walk_all_(Traversal *tv)
{
	while (!trav_empty_(tv))
		walk_one_(tv, trav_pop_(tv));
}

//----------------------------------------------------------------

typedef struct {
       uint16_t type;
       uint16_t size;
} Header;

MemoryStats memory_stats_;

#define GENERIC_TYPE 0xff

Slab generic_8_slab_;
Slab generic_16_slab_;
Slab generic_32_slab_;
Slab generic_64_slab_;
Slab generic_128_slab_;
Slab generic_256_slab_;
Slab generic_512_slab_;
Slab generic_1024_slab_;

Slab cons_slab_;
Slab vblock_slab_;

void mm_init(size_t mem_size)
{
	ca_init(&global_allocator_, CHUNK_SIZE, mem_size);

	slab_init(&generic_8_slab_, "generic-8", GENERIC_TYPE, 8);
	slab_init(&generic_16_slab_, "generic-16", GENERIC_TYPE, 16);
	slab_init(&generic_32_slab_, "generic-32", GENERIC_TYPE, 32);
	slab_init(&generic_64_slab_, "generic-64", GENERIC_TYPE, 64);
	slab_init(&generic_128_slab_, "generic-128", GENERIC_TYPE, 128);
	slab_init(&generic_256_slab_, "generic-256", GENERIC_TYPE, 256);
	slab_init(&generic_512_slab_, "generic-512", GENERIC_TYPE, 512);
	slab_init(&generic_1024_slab_, "generic-1024", GENERIC_TYPE, 1024);

	slab_init(&cons_slab_, "cons", CONS, sizeof(Cons));
	slab_init(&vblock_slab_, "vblock", VBLOCK, sizeof(Value) * ENTRIES_PER_VBLOCK);
}

void mm_exit()
{
	slab_exit(&generic_8_slab_);
	slab_exit(&generic_16_slab_);
	slab_exit(&generic_32_slab_);
	slab_exit(&generic_64_slab_);
	slab_exit(&generic_128_slab_);
	slab_exit(&generic_256_slab_);
	slab_exit(&generic_512_slab_);
	slab_exit(&generic_1024_slab_);

	slab_exit(&cons_slab_);
	slab_exit(&vblock_slab_);

	ca_exit(&global_allocator_);
	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) memory_stats_.total_allocated);
}

static Slab *choose_slab_(size_t s)
{
	static Slab *slabs_[] = {
		&generic_8_slab_,
		&generic_16_slab_,
		&generic_32_slab_,
		&generic_64_slab_,
		&generic_128_slab_,
		&generic_256_slab_,
		&generic_512_slab_,
		&generic_1024_slab_,
	};

	unsigned i, n;

	// FIXME: slow, use ffs
	assert(s <= 1024);
	for (i = 0, n = 8; ; i++, n *= 2) {
		if (s <= n)
			return slabs_[i];
	}

	assert(false);
	return NULL;
}

static inline void *alloc_(ObjectType type, size_t s)
{
	Header *h;
	size_t len;

	// FIXME: assume generic
	switch (type) {
	case CONS:
		return slab_alloc(&cons_slab_);

	case VBLOCK:
		return slab_alloc(&vblock_slab_);

	default:
		len = s + sizeof(Header);
		h = slab_alloc(choose_slab_(len));
		h->type = type;
		h->size = s;
		return h + 1;
	}
}

void *mm_alloc(ObjectType type, size_t s)
{
	void *ptr = alloc_(type, s);
	if (!ptr)
		error("out of memory");

	memory_stats_.total_allocated += s;
	return ptr;
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

static void *header_to_obj(Header *h)
{
	return h + 1;
}

void *mm_clone(void *obj)
{
	Slab *s = ca_address(obj).c->owner;

	if (s->type == GENERIC_TYPE) {
		Header *h = obj_to_header(obj);
		return header_to_obj(slab_clone(s, h, sizeof(*h) + h->size));
	} else
		return slab_clone(s, obj, s->obj_size);
}

void mm_garbage_collect(Value *roots, unsigned count)
{
	Traversal tv;

	slab_clear_marks(&generic_8_slab_);
	slab_clear_marks(&generic_16_slab_);
	slab_clear_marks(&generic_32_slab_);
	slab_clear_marks(&generic_64_slab_);
	slab_clear_marks(&generic_128_slab_);
	slab_clear_marks(&generic_256_slab_);
	slab_clear_marks(&generic_512_slab_);
	slab_clear_marks(&generic_1024_slab_);

	slab_clear_marks(&cons_slab_);
	slab_clear_marks(&vblock_slab_);

	trav_init_(&tv);
	while (count--)
		mark_value_(&tv, roots[count]);

	walk_all_(&tv);

	slab_return_unused_chunks(&generic_8_slab_);
	slab_return_unused_chunks(&generic_16_slab_);
	slab_return_unused_chunks(&generic_32_slab_);
	slab_return_unused_chunks(&generic_64_slab_);
	slab_return_unused_chunks(&generic_128_slab_);
	slab_return_unused_chunks(&generic_256_slab_);
	slab_return_unused_chunks(&generic_512_slab_);
	slab_return_unused_chunks(&generic_1024_slab_);

	slab_return_unused_chunks(&cons_slab_);
	slab_return_unused_chunks(&vblock_slab_);
}

void *as_ref(Value v)
{
	if (get_tag(v) != TAG_REF)
		error("type error: value is not a reference.");
	return v.ptr;
}

ObjectType get_obj_type(void *obj)
{
	uint16_t t = ca_address(obj).c->owner->type;

	if (t == GENERIC_TYPE)
		return obj_to_header(obj)->type;
	else
		return t;
}

size_t get_obj_size(void *obj)
{
	uint16_t t = ca_address(obj).c->owner->type;

	if (t == GENERIC_TYPE)
		return obj_to_header(obj)->size;
	else
		return ca_address(obj).c->owner->obj_size;
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
