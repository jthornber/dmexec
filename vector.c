#include "vector.h"

#include "error.h"
#include "utils.h"

#include <string.h>

//----------------------------------------------------------------

Array *__array_create(unsigned nr_alloc)
{
	Array *a = alloc(ARRAY, sizeof(*a) + sizeof(Value) * nr_alloc);
	a->nr_elts = 0;
	a->nr_allocated = nr_alloc;
	return a;
}

Array *array_create(void)
{
	return __array_create(4);
}

Array *quot_create(void)
{
	Array *q = __array_create(4);
	set_obj_type(q, QUOT);
	return q;
}

Array *array_deep_clone(Array *a)
{
	unsigned i;
	Array *copy = clone(a);

	for (i = 0; i < a->nr_elts; i++)
		array_set(copy, i, clone_value(array_get(a, i)));

	return copy;
}

static inline Value *elt_ptr(Array *a, unsigned i)
{
	return ((Value *) (a + 1)) + i;
}

Array *array_resize(Array *a, unsigned new_nr_alloc)
{
	Array *new = __array_create(new_nr_alloc);

	set_obj_type(new, get_obj_type(a));
	memcpy(elt_ptr(new, 0), elt_ptr(a, 0), sizeof(Value) * a->nr_elts);
	new->nr_elts = a->nr_elts;

	replace_obj(a, new);
	return new;
}

static void check_bounds(Array *a, unsigned i)
{
	if (i >= a->nr_elts)
		error("array index (%u) out of bounds (%u).", i , a->nr_elts);
}

Value array_get(Array *a, unsigned i)
{
	check_bounds(a, i);
	return *elt_ptr(a, i);
}

void array_set(Array *a, unsigned i, Value v)
{
	check_bounds(a, i);
	*elt_ptr(a, i) = v;
}

Array *array_push(Array *a, Value v)
{
	if (a->nr_elts == a->nr_allocated)
		a = array_resize(a, min(a->nr_elts * 2, a->nr_elts + 512));

	*elt_ptr(a, a->nr_elts) = v;
	a->nr_elts++;

	return a;
}

Value array_pop(Array *a)
{
	Value v;

	if (!a->nr_elts)
		error("asked to pop an empty array.");

	v = *elt_ptr(a, a->nr_elts - 1);
	a->nr_elts--;
	return v;
}

Value array_peekn(Array *a, unsigned n)
{
	return *elt_ptr(a, a->nr_elts - 1 - n);
}

Value array_peek(Array *a)
{
	return array_peekn(a, 0);
}

Array *array_unshift(Array *a, Value v)
{
	if (a->nr_elts == a->nr_allocated)
		a = array_resize(a, min(a->nr_elts * 2, 512));

	if (a->nr_elts)
		memmove(elt_ptr(a, 1), elt_ptr(a, 0),
			sizeof(Value) * a->nr_elts);

	*elt_ptr(a, 0) = v;
	a->nr_elts++;

	return a;
}

Value array_shift(Array *a)
{
	Value v;

	if (!a->nr_elts)
		error("asked to shift an array with zero elements.");

	v = *elt_ptr(a, 0);

	a->nr_elts--;
	if (a->nr_elts)
		memmove(elt_ptr(a, 0), elt_ptr(a, 1),
			sizeof(Value) * a->nr_elts);

	return v;
}

Array *array_concat(Array *a, Array *a2)
{
	unsigned i;

	for (i = 0; i < a2->nr_elts; i++)
		a = array_push(a, *elt_ptr(a2, i));

	return a;
}

void array_reverse(Array *a)
{
	Value tmp;

	for (unsigned i = 0; i < a->nr_elts / 2; i++) {
		tmp = *elt_ptr(a, i);
		*elt_ptr(a, i) = *elt_ptr(a, a->nr_elts - i - 1);
		*elt_ptr(a, a->nr_elts - i - 1) = tmp;
	}
}

//----------------------------------------------------------------
