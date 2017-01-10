#ifndef DMEXEC_ARRAY_H
#define DMEXEC_ARRAY_H

#include "mm.h"

//----------------------------------------------------------------

struct array {
	unsigned nr_elts;
	unsigned nr_allocated;
};

struct array *array_create(void);
struct array *quot_create(void);

// Clones the values (but no deeper)
struct array *array_deep_clone(struct array *a);

struct array *array_resize(struct array *old, unsigned new_nr_alloc);

Value array_get(struct array *a, unsigned i);
void array_set(struct array *a, unsigned i, Value v);

struct array *array_push(struct array *a, Value v);
Value array_pop(struct array *a);

Value array_peek(struct array *a);
Value array_peekn(struct array *a, unsigned n);

struct array *array_unshift(struct array *a, Value v);
Value array_shift(struct array *a);

struct array *array_concat(struct array *a, struct array *a2);

void array_reverse(struct array *a);

//----------------------------------------------------------------

#endif
