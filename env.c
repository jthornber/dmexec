#include "env.h"

#include <assert.h>

//----------------------------------------------------------------

StaticEnv *r_alloc() {
	StaticEnv *r = untyped_zalloc(sizeof(*r));
	return r;
}

void r_push_names(StaticEnv *r, Value ns)
{
	unsigned i = 0, len = list_len(ns);
	StaticFrame *f = untyped_alloc(sizeof(*f) + sizeof(Symbol *) * len);
	while (is_cons(ns))
		f->syms[i++] = as_symbol(car(ns));
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
	
}

unsigned r_add_constant(StaticEnv *r, Value v)
{
	if (r->nr_constants == MAX_CONSTANTS)
		error("too many constants");

	r->constants[r->nr_constants++] = v;
}

Value r_constant(StaticEnv *r, unsigned index)
{
	return r->constants[index];
}

//----------------------------------------------------------------

#endif
