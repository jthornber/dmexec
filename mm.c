#include "mm.h"

#include <gc.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define HEADER_MAGIC 846219U

#include "error.h"
#include "string_type.h"

//----------------------------------------------------------------
// Memory manager

// FIXME: use the valgrind api to allow it to check for accesses to garbage

static MemoryStats memory_stats_;

static void out_of_memory(void)
{
	error("out of memory");
}

void *alloc(ObjectType type, size_t s)
{
	size_t len = s + sizeof(Header);

	// Also zeroes memory
	Header *h = GC_MALLOC(len);

	if (!h)
		out_of_memory();

	h->type = type;
	h->size = s;
	h->magic = HEADER_MAGIC;

	memory_stats_.total_allocated += len;
	return h + 1;
}

void *zalloc(ObjectType type, size_t s)
{
	return alloc(type, s);
}

static Header *obj_to_header(void *obj)
{
	return ((Header *) obj) - 1;
}

static void *follow_fwd_ptrs(void *obj)
{
	Header *h = obj_to_header(obj);

	while (h->type == FORWARD) {
		obj = *((void **) (h + 1));
		h = obj_to_header(obj);
	}

	return obj;
}

void *clone(void *obj_)
{
	void *obj = follow_fwd_ptrs(obj_);
	Header *h = obj_to_header(obj);
	void *new = alloc(h->type, h->size);

	memcpy(new, obj, h->size);
	return new;
}

void *as_ref(Value v)
{
	if (get_tag(v) != TAG_REF)
		error("type error: value is not a reference.");
	return follow_fwd_ptrs(v.ptr);
}

void replace_obj(void *old_obj, void *new_obj)
{
	set_obj_type(old_obj, FORWARD);
	*((void **) old_obj) = new_obj;
}

void set_obj_type(void *obj, ObjectType t)
{
	obj_to_header(obj)->type = t;
}

ObjectType get_obj_type(void *obj)
{
	return obj_to_header(obj)->type;
}

//----------------------------------------------------------------

Header *get_header(Value v)
{
	Header *h = obj_to_header(as_ref(v));
	if (h->magic != HEADER_MAGIC) {
		fprintf(stderr, "memory corruption detected.");
		abort();
	}
	return h;
};

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
	return mk_ref(clone(as_ref(v)));
}

Value mk_nil()
{
	Value v;
	v.i = TAG_NIL;
	return v;
}

// FIXME: move else where, and factor out common code with mk-symbol
Value mk_true(void)
{
	String str, *copy;
	string_tmp(":true", &str);
	copy = string_clone(&str);
	set_obj_type(copy, SYMBOL);
	return mk_ref(copy);
}

bool is_false(Value v)
{
	return v.i == NIL;
}

void *as_type(ObjectType t, Value v)
{
	if (get_type(v) != t)
		error("type error: expected type %d.", t);

	return as_ref(v);
}

//----------------------------------------------------------------

MemoryStats *get_memory_stats()
{
	return &memory_stats_;
}

//----------------------------------------------------------------
