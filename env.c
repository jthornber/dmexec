#include "env.h"

#include <assert.h>

//----------------------------------------------------------------

StaticEnv *r_alloc() {
	StaticEnv *r = mm_zalloc(STATIC_ENV, sizeof(*r));
	return r;
}

void r_push_names(StaticEnv *r, Value ns)
{
	unsigned i = 0, len = list_len(ns);
	StaticFrame *f = mm_alloc(STATIC_FRAME, sizeof(*f) + sizeof(Symbol *) * len);
	f->nr_syms = len;
	while (is_cons(ns)) {
		f->syms[i++] = as_type(SYMBOL, car(ns));
		ns = cdr(ns);
	}

	f->prev = r->frames;
	r->frames = f;
}

void r_pop_names(StaticEnv *r)
{
	assert(r->frames);
	r->frames = r->frames->prev;
}

Kind compute_kind(StaticEnv *r, Symbol *sym)
{
	unsigned i = 0, j;
	StaticFrame *f = r->frames;

	// local?
	while (f) {
		for (j = 0; j < f->nr_syms; j++)
			if (sym == f->syms[j])
				return (Kind) {KindLocal, i, j};
		f = f->prev;
	}

	error("implement global and predefinied look up\n");

	// global?

	// predefined?
	return (Kind) {KindLocal, 0, 0};
}

unsigned r_add_constant(StaticEnv *r, Value v)
{
	unsigned i;

	if (r->nr_constants == MAX_CONSTANTS)
		error("too many constants");

	i = r->nr_constants++;
	r->constants[i] = v;
	return i;
}

Value r_constant(StaticEnv *r, unsigned index)
{
	return r->constants[index];
}

//----------------------------------------------------------------

