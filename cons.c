#include "cons.h"

/*----------------------------------------------------------------*/
// FIXME: does this really need it's own file?
Value car(Value cell)
{
	return ((Cons *) as_ref(cell))->car;
}

Value cdr(Value cell)
{
	return ((Cons *) as_ref(cell))->cdr;
}

Value cons(Value car, Value cdr)
{
	Cons *cell = alloc(CONS, sizeof(*cell));
	cell->car = car;
	cell->cdr = cdr;
	return mk_ref(cell);
}

/*----------------------------------------------------------------*/

