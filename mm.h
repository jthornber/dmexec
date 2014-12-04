#ifndef DMEXEC_MM_H
#define DMEXEC_MM_H

#include <stdlib.h>

//----------------------------------------------------------------

enum object_type {
	FORWARD,
	NAMESPACE,
	NAMESPACE_ENTRY,
	PRIMITIVE,
	STRING,
	BYTE_ARRAY,
	TUPLE,
	SYMBOL,
	WORD,
	QUOT,
	ARRAY,
	CODE_POSITION,
	CONTINUATION,

	/* these are always tagged immediate values */
	FIXNUM,
	FALSE_TYPE
};

struct header {
	enum object_type type;
	unsigned size; 		/* in bytes, we always round to a 4 byte boundary */
	unsigned magic;
};

void *alloc(enum object_type type, size_t s);
void *zalloc(enum object_type type, size_t s);
void *clone(void *obj);
void replace_obj(void *old_obj, void *new_obj);

void set_obj_type(void *obj, enum object_type t);
enum object_type get_obj_type(void *obj);

struct memory_stats {
	size_t total_allocated;
	size_t total_collected;
	size_t current_allocated;
	size_t max_allocated;
	unsigned nr_gcs;
};

struct memory_stats *get_memory_stats(void);

//----------------------------------------------------------------

enum tag {
	TAG_REF = 0,
	TAG_FIXNUM = 1,
	TAG_FALSE
};

typedef union value {
	void *ptr;
	int32_t i;
} value_t;

value_t mk_ref(void *ptr);
value_t clone_value(value_t v);

// functions that take a value_t automatically chase forward ptrs.
void *as_ref(value_t v);
struct header *get_header(value_t v);
enum object_type get_type(value_t v);

enum tag get_tag(value_t v);
value_t mk_false(void);
value_t mk_true(void);

int as_fixnum(value_t v);
void *as_type(enum object_type t, value_t v);

//----------------------------------------------------------------

#endif
