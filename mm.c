#include "mm.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HEADER_MAGIC 846219U

//----------------------------------------------------------------
// Memory manager

// FIXME: use the valgrind api to allow it to check for accesses to garbage

static struct memory_stats memory_stats_;

static void out_of_memory()
{
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

void *alloc(enum object_type type, size_t s)
{
	size_t len = s + sizeof(struct header);
	struct header *h = malloc(len);

	if (!h)
		out_of_memory();

	h->type = type;
	h->size = s;
	h->magic = HEADER_MAGIC;

	memory_stats_.total_allocated += len;
	return ((char *) (h + 1));
}

void *zalloc(enum object_type type, size_t s)
{
	void *ptr = alloc(type, s);
	memset(ptr, 0, s);
	return ptr;
}

static struct header *obj_to_header(void *obj)
{
	return ((struct header *) obj) - 1;
}

static void *follow_fwd_ptrs(void *obj)
{
	struct header *h = obj_to_header(obj);

	while (h->type == FORWARD) {
		obj = *((void **) (h + 1));
		h = obj_to_header(obj);
	}

	return obj;
}

void *clone(void *obj_)
{
	void *obj = follow_fwd_ptrs(obj_);
	struct header *h = obj_to_header(obj);
	void *new = alloc(h->type, h->size);

	memcpy(new, obj, h->size);
	return new;
}

void *as_ref(value_t v)
{
	void *obj = v.ptr;
	assert(get_tag(v) == TAG_REF);
	return follow_fwd_ptrs(obj);
}

void replace_obj(void *old_obj, void *new_obj)
{
	set_obj_type(old_obj, FORWARD);
	*((void **) old_obj) = new_obj;
}

void set_obj_type(void *obj, enum object_type t)
{
	struct header *h = obj_to_header(obj);
	h->type = t;
}

enum object_type get_obj_type(void *obj)
{
	return obj_to_header(obj)->type;
}

//----------------------------------------------------------------

struct header *get_header(value_t v)
{
	struct header *h = obj_to_header(as_ref(v));
	assert(h->magic == HEADER_MAGIC);
	return h;
};

enum object_type get_type(value_t v)
{

	if (get_tag(v) == TAG_FIXNUM)
		return FIXNUM;

	return get_obj_type(as_ref(v));
}

//----------------------------------------------------------------
// Values - immediate or reference
//
// The bottom 2 bits are used for tagging.

enum tag get_tag(value_t v)
{
	return v.i & 0x3;
}

value_t mk_fixnum(int i)
{
	value_t v;
	v.i = (i << 2) | TAG_FIXNUM;
	return v;
}

unsigned as_fixnum(value_t v)
{
	assert(get_tag(v) == TAG_FIXNUM);
	return v.i >> 2;
}

value_t mk_ref(void *ptr)
{
	value_t v;
	v.ptr = ptr;
	return v;
}

value_t mk_false()
{
	value_t v;
	v.i = TAG_FALSE;
	return v;
}

bool is_false(value_t v)
{
	return v.i == TAG_FALSE;
}

//----------------------------------------------------------------

struct memory_stats *get_memory_stats()
{
	return &memory_stats_;
}

//----------------------------------------------------------------
