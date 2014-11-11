#include "array.h"

#include <assert.h>
#include <string.h>

//----------------------------------------------------------------

value_t mk_array()
{
	struct array *a = alloc(ARRAY, sizeof(*a));
	memset(a, 0, sizeof(*a));
	a->nr_elts = 0;
	return mk_ref(a);
}

void append_array(value_t av, value_t v)
{
	struct array *a = as_ref(av);

	assert(a->nr_elts < MAX_ARRAY_SIZE);
	a->elts[a->nr_elts++] = v;
}

//----------------------------------------------------------------
