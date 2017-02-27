#ifndef DMEXEC_MM_H
#define DMEXEC_MM_H

#include <stdlib.h>

//----------------------------------------------------------------

// Update type_desc() if you change this.
typedef enum {
	PRIMITIVE,
	CLOSURE,
	STRING,
	SYMBOL,
	CONS,
	NIL,
	VECTOR,
	VBLOCK,
	FRAME,
	STATIC_FRAME,
	STATIC_ENV,
	THUNK,
	RAW,

	/* these are always tagged immediate values */
	// FIXME we need 64 bit integers for device sizes
	FIXNUM,
} ObjectType;

typedef union value {
	void *ptr;
	int32_t i;
} Value;

// Call these two from your main to set up the garbage collection.
void mm_init(size_t mem_size);
void mm_exit();

void mm_add_root(Value *v);
void mm_rm_root(Value *v);

// Only memory older than the last checkpoint gets garbage collected.  Make
// sure you call this frequently (after you register the roots).
void mm_checkpoint();

void *mm_alloc(ObjectType type, size_t s);
void *mm_zalloc(ObjectType type, size_t s);
void *mm_clone(void *obj);

//----------------------------------------------------------------

ObjectType get_obj_type(void *obj);
size_t get_obj_size(void *obj);

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

Value mk_ref(void *ptr);
Value clone_value(Value v);

void *as_ref(Value v);
ObjectType get_type(Value v);

Tag get_tag(Value v);
Value mk_nil(void);

int as_fixnum(Value v);
void *as_type(ObjectType t, Value v);

//----------------------------------------------------------------

#endif
