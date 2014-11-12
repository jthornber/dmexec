#ifndef DMEXEC_ARRAY_H
#define DMEXEC_ARRAY_H

#include "mm.h"

//----------------------------------------------------------------

struct array {
	unsigned nr_elts;
	unsigned nr_allocated;
};

struct array *array_create(unsigned nr_alloc);
struct array *quot_create(unsigned nr_alloc);
struct array *array_resize(struct array *old, unsigned new_nr_alloc);

value_t array_get(struct array *a, unsigned i);
void array_set(struct array *a, unsigned i, value_t v);

void array_push(struct array *a, value_t v);
value_t array_pop(struct array *a);

void array_unshift(struct array *a, value_t v);
value_t array_shift(struct array *a);

// FIXME: remove this interface
//value_t mk_array();

//----------------------------------------------------------------

#endif
