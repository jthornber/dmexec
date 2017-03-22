#include "env.h"
#include "equality.h"

#include <assert.h>

//----------------------------------------------------------------

StaticEnv *r_alloc()
{
	StaticEnv *r = mm_alloc(STATIC_ENV, sizeof(*r));
	r->constants = v_empty();
	r->frames = v_empty();
	return r;
}

void r_push_names(StaticEnv *r, Value ns)
{
	r->frames = v_push(r->frames, list_to_vector(ns));
}

void r_pop_names(StaticEnv *r)
{
	r->frames = v_pop(r->frames);
}

Kind compute_kind(StaticEnv *r, Symbol *sym)
{
	unsigned i = 0, j, nr_frames = v_size(r->frames);
	unsigned nr_syms;

	for (i = nr_frames; i; i--) {
		Vector *f = v_ref(r->frames, i).ptr;
		nr_syms = v_size(f);
		for (j = 0; j < nr_syms; j++)
			if (equalp(v_ref(f, j), mk_ref(sym)))
				return (Kind) {KindLocal, nr_frames - i, j};
	}

	error("implement global and predefinied look up\n");

	// global?

	// predefined?
	return (Kind) {KindLocal, 0, 0};
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

