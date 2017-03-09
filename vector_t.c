#include "vector.h"
#include "vm.h"

#include <assert.h>
#include <string.h>

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
	Vector *v = v_empty();
	assert(v_size(v) == 0);
}

static void t_append_once()
{
	Vector *v = v_empty();
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
	Vector *v = v_empty();

	for (i = 0; i < count; i++)
		v = v_append(v, mk_fixnum(i));

	for (i = 0; i < count; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

static void t_append_million()
{
	unsigned count = 1024 * 1024;
	unsigned i;
	Vector *v = v_empty();

	for (i = 0; i < count; i++) {
		v = v_append(v, mk_fixnum(i));
		assert(equalp(v_ref(v, i), mk_fixnum(i)));

		if (!(i % (32 * 1024))) {
			Value val = mk_ref(v);
			mm_garbage_collect(&val, 1);
		}
	}

	for (i = 0; i < count; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

static void t_append_million_transient()
{
	unsigned count = 1024 * 1024;
	unsigned i;
	Vector *v = v_empty();

	v = v_transient_begin(v);
	v = v_resize(v, count, mk_fixnum(0));
	for (i = 0; i < count; i++) {
		v_set(v, i, mk_fixnum(i));
		assert(equalp(v_ref(v, i), mk_fixnum(i)));

		if (!(i % (32 * 1024))) {
			Value val = mk_ref(v);
			mm_garbage_collect(&val, 1);
		}
	}
	v_transient_end(v);

	for (i = 0; i < count; i++)
		assert(equalp(v_ref(v, i), mk_fixnum(i)));
}

static void t_square()
{
	unsigned count = 32 * 1024;
	unsigned i;
	Vector *v = v_empty();

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

//----------------------------------------------------------------

static size_t total_allocated_()
{
	return memory_stats_.total_allocated;
}

static void indent(unsigned n)
{
	while (n--)
		fputc(' ', stderr);
}

static void run(const char *name, void (*fn)())
{
	size_t before, after;

	fprintf(stderr, "%s", name);
	indent(24 - strlen(name));
	fprintf(stderr, "... ");
	before = total_allocated_();

	fn();

	after = total_allocated_();
	fprintf(stderr, "%llu\n", (unsigned long long) (after - before));
}

int main(int argc, const char *argv[])
{
	mm_init(32 * 1024 * 1024);
	run("empty_vector", t_empty_vector);
	run("append_once", t_append_once);
	run("append32", t_append32);
	run("square", t_square);
	run("append_million", t_append_million);
	run("append_million_transient", t_append_million_transient);
	mm_exit();

	return 0;
}

//----------------------------------------------------------------

