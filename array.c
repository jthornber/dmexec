#include "array.h"

#include <assert.h>
#include <string.h>

//----------------------------------------------------------------

struct array *array_create(unsigned nr_alloc)
{
	struct array *a = alloc(ARRAY, sizeof(*a) + sizeof(value_t) * nr_alloc);
	a->nr_elts = 0;
	a->nr_allocated = nr_alloc;
	return a;
}

struct array *quot_create(unsigned nr_alloc)
{
	struct array *q = array_create(nr_alloc);
	set_type(q, QUOT);
	return q;
}

struct array *array_resize(struct array *old, unsigned new_nr_alloc)
{
	unsigned i;
	struct array *new = alloc(ARRAY, new_nr_alloc);

	for (i = 0; i < old->nr_elts; i++)
		array_set(new, i, array_get(old, i));

	return new;
}

static inline value_t *elt_ptr(struct array *a, unsigned i)
{
	return ((value_t *) (a + 1)) + i;
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
	assert(a->nr_elts < a->nr_allocated); /* FIXME: throw */
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
	assert(a->nr_elts < a->nr_allocated); /* FIXME: throw */

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

value_t mk_array()
{
	struct array *a = array_create(32);
	return mk_ref(a);
}

void append_array(value_t av, value_t v)
{
	struct array *a = as_ref(av);
	array_push(a, v);
}

//----------------------------------------------------------------
