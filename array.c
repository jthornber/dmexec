#include "array.h"

#include "utils.h"

#include <assert.h>
#include <string.h>

//----------------------------------------------------------------

struct array *__array_create(unsigned nr_alloc)
{
	struct array *a = alloc(ARRAY, sizeof(*a) + sizeof(value_t) * nr_alloc);
	a->nr_elts = 0;
	a->nr_allocated = nr_alloc;
	return a;
}

struct array *array_create()
{
	return __array_create(4);
}

struct array *quot_create()
{
	struct array *q = __array_create(4);
	set_type(q, QUOT);
	return q;
}

static inline value_t *elt_ptr(struct array *a, unsigned i)
{
	return ((value_t *) (a + 1)) + i;
}

struct array *array_resize(struct array *a, unsigned new_nr_alloc)
{
	struct array *new = __array_create(new_nr_alloc);

	memcpy(elt_ptr(new, 0), elt_ptr(a, 0), sizeof(value_t) * a->nr_elts);
	replace_obj(a, new);
	return new;
}

value_t array_get(struct array *a, unsigned i)
{
	assert(i < a->nr_elts);	/* FIXME: throw */
	return *elt_ptr(a, i);
}

void array_set(struct array *a, unsigned i, value_t v)
{
	assert(i < a->nr_elts);	/* FIXME: throw */
	*elt_ptr(a, i) = v;
}

void array_push(struct array *a, value_t v)
{
	if (a->nr_elts == a->nr_allocated)
		a = array_resize(a, min(a->nr_elts * 2, a->nr_elts + 512));

	a->nr_elts++;
	*elt_ptr(a, a->nr_elts - 1) = v;
}

value_t array_pop(struct array *a)
{
	value_t v;

	assert(a->nr_elts);	/* FIXME: throw */
	v = *elt_ptr(a, a->nr_elts - 1);
	a->nr_elts--;
	return v;
}

void array_unshift(struct array *a, value_t v)
{
	if (a->nr_elts == a->nr_allocated)
		a = array_resize(a, min(a->nr_elts * 2, 512));

	if (a->nr_elts)
		memmove(elt_ptr(a, 1), elt_ptr(a, 0),
			sizeof(value_t) * a->nr_elts);

	*elt_ptr(a, 0) = v;
	a->nr_elts++;
}

value_t array_shift(struct array *a)
{
	value_t v;

	assert(a->nr_elts);
	v = *elt_ptr(a, 0);

	a->nr_elts--;
	if (a->nr_elts)
		memmove(elt_ptr(a, 0), elt_ptr(a, 1),
			sizeof(value_t) * a->nr_elts);

	return v;
}

//----------------------------------------------------------------
