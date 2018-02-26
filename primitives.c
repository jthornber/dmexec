#include "primitives.h"

//----------------------------------------------------------------

Value plus_i(Value lhs, Value rhs)
{
	return mk_fixnum(as_fixnum(lhs) + as_fixnum(rhs));
}

void def_basic_primitives(StaticEnv *r)
{
	Primitive *p = mm_alloc(PRIMITIVE, sizeof(*p));
	p->name = "+";
	p->argc = 2;
	p->prim2 = plus_i;
	r_add_prim(r, mk_ref(p));
}

//----------------------------------------------------------------
