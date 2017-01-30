#ifndef DMEXEC_ARRAY_H
#define DMEXEC_ARRAY_H

#include "mm.h"

//----------------------------------------------------------------

typedef struct {
	unsigned nr_elts;
	unsigned nr_allocated;
} Array;

Array *array_create(void);
Array *quot_create(void);

// Clones the values (but no deeper)
Array *array_deep_clone(Array *a);

Array *array_resize(Array *old, unsigned new_nr_alloc);

Value array_get(Array *a, unsigned i);
void array_set(Array *a, unsigned i, Value v);

Array *array_push(Array *a, Value v);
Value array_pop(Array *a);

Value array_peek(Array *a);
Value array_peekn(Array *a, unsigned n);

Array *array_unshift(Array *a, Value v);
Value array_shift(Array *a);

Array *array_concat(Array *a, Array *a2);

void array_reverse(Array *a);

//----------------------------------------------------------------

#endif
