#ifndef DMEXEC_MM_H
#define DMEXEC_MM_H

#include <stdlib.h>

//----------------------------------------------------------------

// Update type_desc() if you change this.
typedef enum {
	FORWARD,
	PRIMITIVE,
	CLOSURE,
	STRING,
	SYMBOL,
	CONS,
	NIL,
	VECTOR,

	/* these are always tagged immediate values */
	// FIXME we need 64 bit integers for device sizes
	FIXNUM,
} ObjectType;

// FIXME: huge tag
typedef struct {
	ObjectType type;
	unsigned size; 		/* in bytes, we always round to a 4 byte boundary */
	unsigned magic;
} Header;

// Call these two from your main to set up the garbage collection.
void mm_init();
void mm_exit();

void *untyped_alloc(size_t s);
void *untyped_zalloc(size_t s);
void *untyped_clone(void *old_obj, size_t s);

void *alloc(ObjectType type, size_t s);
void *zalloc(ObjectType type, size_t s);
void *clone(void *obj);
void replace_obj(void *old_obj, void *new_obj);

void set_obj_type(void *obj, ObjectType t);
ObjectType get_obj_type(void *obj);

typedef struct {
	size_t total_allocated;
	size_t total_collected;
	size_t current_allocated;
	size_t max_allocated;
	unsigned nr_gcs;
} MemoryStats;

MemoryStats *get_memory_stats(void);

//----------------------------------------------------------------

typedef enum {
	TAG_REF = 0,
	TAG_FIXNUM = 1,
	TAG_NIL
} Tag;

typedef union value {
	void *ptr;
	int32_t i;
} Value;

Value mk_ref(void *ptr);
Value clone_value(Value v);

// functions that take a Value automatically chase forward ptrs.
void *as_ref(Value v);
Header *get_header(Value v);
ObjectType get_type(Value v);

Tag get_tag(Value v);
Value mk_nil(void);
Value mk_true(void);

int as_fixnum(Value v);
void *as_type(ObjectType t, Value v);

//----------------------------------------------------------------

#endif
