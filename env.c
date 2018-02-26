#include "env.h"
#include "equality.h"
#include "hash_table.h"

#include <assert.h>

//----------------------------------------------------------------

StaticEnv *r_alloc()
{
	StaticEnv *r = mm_alloc(STATIC_ENV, sizeof(*r));

	r->constants = v_empty();
	r->primitives_r = ht_empty();
	r->globals_r = ht_empty();
	r->frames_r = v_empty();

	return r;
}

void r_push_frame(StaticEnv *r, Value ns)
{
	r->frames_r = v_push(r->frames_r, list_to_vector(ns));
}

void r_pop_frame(StaticEnv *r)
{
	r->frames_r = v_pop(r->frames_r);
}

unsigned r_add_constant(StaticEnv *r, Value v)
{
	r->constants = v_push(r->constants, v);
	return v_size(r->constants) - 1;
}

Value r_constant(StaticEnv *r, unsigned index)
{
	return v_ref(r->constants, index);
}

static void check_duplicate(StaticEnv *r, Value k)
{
	Value result;

	if (ht_lookup(r->primitives_r, k, &result))
		error("primitive has already been defined");
}

void r_add_prim(StaticEnv *r, Value prim)
{
	Primitive *p = as_ref(prim);
	Value n = mk_ref(mk_string_from_cstr(SYMBOL, p->name));
	Value v = mk_fixnum(v_size(r->constants));

	fprintf(stderr, "adding primitive %s\n", p->name);
	check_duplicate(r, n);
	r->primitives_r = ht_insert(r->primitives_r, n, v);
	r->constants = v_push(r->constants, prim);
}

// FIXME: review all this, it's key to our semantics.  Must get this the same
// as Scheme.
Kind compute_kind(StaticEnv *r, String *sym)
{
	Value v;
	unsigned nr_syms;
	unsigned n, i, j, nr_frames = v_size(r->frames_r);

	// Local?  These can override globals and constants.
	for (i = nr_frames; i; i--) {
		Vector *f = v_ref(r->frames_r, i).ptr;
		nr_syms = v_size(f);
		for (j = 0; j < nr_syms; j++)
			if (equalp(v_ref(f, j), mk_ref(sym)))
				return (Kind) {KindLocal, nr_frames - i, j};
	}

	// An already defined global overrides a primitive.
	if (ht_lookup(r->globals_r, mk_ref(sym), &v))
		return (Kind) {KindGlobal, as_fixnum(v), 0};

	// Primitive?
	if (ht_lookup(r->primitives_r, mk_ref(sym), &v))
		return (Kind) {KindConstant, as_fixnum(v), 0};

	// Assume it's an as yet undefined global
	n = ht_size(r->globals_r);
	r->globals_r = ht_insert(r->globals_r, mk_ref(sym), mk_fixnum(n));
	return (Kind) {KindGlobal, n, 0};
}

//----------------------------------------------------------------

