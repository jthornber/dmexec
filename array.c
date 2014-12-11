#include "array.h"

#include "error.h"
#include "utils.h"

#include <string.h>

//----------------------------------------------------------------

struct array *__array_create(unsigned nr_alloc)
{
	struct array *a = alloc(ARRAY, sizeof(*a) + sizeof(value_t) * nr_alloc);
	a->nr_elts = 0;
	a->nr_allocated = nr_alloc;
	return a;
}

struct array *array_create(void)
{
	return __array_create(4);
}

struct array *quot_create(void)
{
	struct array *q = __array_create(4);
	set_obj_type(q, QUOT);
	return q;
}

struct array *array_deep_clone(struct array *a)
{
	unsigned i;
	struct array *copy = clone(a);

	for (i = 0; i < a->nr_elts; i++)
		array_set(copy, i, clone_value(array_get(a, i)));

	return copy;
}

static inline value_t *elt_ptr(struct array *a, unsigned i)
{
	return ((value_t *) (a + 1)) + i;
}

struct array *array_resize(struct array *a, unsigned new_nr_alloc)
{
	struct array *new = __array_create(new_nr_alloc);

	set_obj_type(new, get_obj_type(a));
	memcpy(elt_ptr(new, 0), elt_ptr(a, 0), sizeof(value_t) * a->nr_elts);
	new->nr_elts = a->nr_elts;

	replace_obj(a, new);
	return new;
}

static void check_bounds(struct array *a, unsigned i)
{
	if (i >= a->nr_elts)
		error("array index (%u) out of bounds (%u).", i , a->nr_elts);
}

value_t array_get(struct array *a, unsigned i)
{
	check_bounds(a, i);
	return *elt_ptr(a, i);
}

void array_set(struct array *a, unsigned i, value_t v)
{
	check_bounds(a, i);
	*elt_ptr(a, i) = v;
}

struct array *array_push(struct array *a, value_t v)
{
	if (a->nr_elts == a->nr_allocated)
		a = array_resize(a, min(a->nr_elts * 2, a->nr_elts + 512));

	*elt_ptr(a, a->nr_elts) = v;
	a->nr_elts++;

	return a;
}

value_t array_pop(struct array *a)
{
	value_t v;

	if (!a->nr_elts)
		error("asked to pop an empty array.");

	v = *elt_ptr(a, a->nr_elts - 1);
	a->nr_elts--;
	return v;
}

value_t array_peekn(struct array *a, unsigned n)
{
	return *elt_ptr(a, a->nr_elts - 1 - n);
}

value_t array_peek(struct array *a)
{
	return array_peekn(a, 0);
}

struct array *array_unshift(struct array *a, value_t v)
{
	if (a->nr_elts == a->nr_allocated)
		a = array_resize(a, min(a->nr_elts * 2, 512));

	if (a->nr_elts)
		memmove(elt_ptr(a, 1), elt_ptr(a, 0),
			sizeof(value_t) * a->nr_elts);

	*elt_ptr(a, 0) = v;
	a->nr_elts++;

	return a;
}

value_t array_shift(struct array *a)
{
	value_t v;

	if (!a->nr_elts)
		error("asked to shift an array with zero elements.");

	v = *elt_ptr(a, 0);

	a->nr_elts--;
	if (a->nr_elts)
		memmove(elt_ptr(a, 0), elt_ptr(a, 1),
			sizeof(value_t) * a->nr_elts);

	return v;
}

struct array *array_concat(struct array *a, struct array *a2)
{
	unsigned i;

	for (i = 0; i < a2->nr_elts; i++)
		a = array_push(a, *elt_ptr(a2, i));

	return a;
}

void array_reverse(struct array *a)
{
	value_t tmp;

	for (unsigned i = 0; i < a->nr_elts / 2; i++) {
		tmp = *elt_ptr(a, i);
		*elt_ptr(a, i) = *elt_ptr(a, a->nr_elts - i - 1);
		*elt_ptr(a, a->nr_elts - i - 1) = tmp;
	}
}

//----------------------------------------------------------------
