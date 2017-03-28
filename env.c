#include "env.h"
#include "equality.h"
#include "hash_table.h"

#include <assert.h>

//----------------------------------------------------------------

StaticEnv *r_alloc()
{
	StaticEnv *r = mm_alloc(STATIC_ENV, sizeof(*r));

	r->constants = v_empty();
	r->frames = v_empty();
	r->predefined = ht_empty();
	r->globals = ht_empty();

	return r;
}

void r_push_frame(StaticEnv *r, Value ns)
{
	r->frames = v_push(r->frames, list_to_vector(ns));
}

void r_pop_frame(StaticEnv *r)
{
	r->frames = v_pop(r->frames);
}

Kind compute_kind(StaticEnv *r, String *sym)
{
	Value v;
	unsigned nr_syms;
	unsigned i, j, nr_frames = v_size(r->frames);

	for (i = nr_frames; i; i--) {
		Vector *f = v_ref(r->frames, i).ptr;
		nr_syms = v_size(f);
		for (j = 0; j < nr_syms; j++)
			if (equalp(v_ref(f, j), mk_ref(sym)))
				return (Kind) {KindLocal, nr_frames - i, j};
	}

	// predefined?

	// global?
	if (ht_lookup(r->globals, mk_ref(sym), &v))
		return (Kind) {KindGlobal, as_fixnum(v), 0};

	unsigned n = ht_size(r->globals);
	r->globals = ht_insert(r->globals, mk_ref(sym), mk_fixnum(n));
	return (Kind) {KindGlobal, n, 0};
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

//----------------------------------------------------------------

