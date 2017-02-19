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
	unsigned count = 32;
	unsigned i = 0;
	Vector *v = v_alloc();

	for (i = 0; i < count; i++)
		v = v_append(v, mk_fixnum(i));

	for (i = 0; i < count; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

static void t_append_million()
{
	unsigned count = 1024 * 1024; // FIXME: up this to 1 million
	unsigned i;
	Vector *v = v_alloc();

	for (i = 0; i < count; i++) {
		v = v_append(v, mk_fixnum(i));
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
	}

	for (i = 0; i < count; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

static void t_square()
{
	unsigned count = 32 * 1024;
	unsigned i;
	Vector *v = v_alloc();

	v = v_resize(v, count, mk_fixnum(0));
	v = v_transient_begin(v);
	for (i = 0; i < count; i++) {
		v = v_set(v, i, mk_fixnum(i * i));
		assert(equalp(v_ref(v, i), mk_fixnum(i * i)));
	}
	v_transient_end(v);

	for (i = 0; i < count; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i * i)));
}

int main(int argc, const char *argv[])
{
	mm_init();
	t_empty_vector();
	t_append_once();
	t_append32();
	t_square();
	t_append_million();
	mm_exit();

	return 0;
}
//----------------------------------------------------------------

