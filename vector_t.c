#include "vector.h"
#include "vm.h"

#include <assert.h>

//----------------------------------------------------------------

// FIXME: move somewhere for general use
static bool equalp(Value lhs, Value rhs)
{
	// FIXME: fixnums only atm
	if (get_type(lhs) != FIXNUM || get_type(rhs) != FIXNUM)
		return false;

	return lhs.i == rhs.i;
}

static void t_empty_vector()
{
	Vector *v = v_alloc();
	assert(v_size(v) == 0);
}

static void t_append_once()
{
	Vector *v = v_alloc();
	Vector *v2 = v_append(v, mk_fixnum(123));
	assert(v2 != v);
	assert(v_size(v2) == 1);
	assert(v_size(v) == 0);
	assert(equalp(v_ref(v2, 0), mk_fixnum(123)));
}

static void t_append32()
{
	unsigned i = 0;
	Vector *v = v_alloc();

	for (i = 0; i < 32; i++)
		v = v_append(v, mk_fixnum(i));

	for (i = 0; i < 32; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

static void t_append_million()
{
	unsigned i = 0;
	Vector *v = v_alloc();

	for (i = 0; i < 1000000; i++)
		v = v_append(v, mk_fixnum(i));

	for (i = 0; i < 1000000; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

int main(int argc, const char *argv[])
{
	mm_init();
	t_empty_vector();
	t_append_once();
	t_append32();
	t_append_million();
	mm_exit();

	return 0;
}
//----------------------------------------------------------------

