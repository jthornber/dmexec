#include "cons.h"

/*----------------------------------------------------------------*/

Value car(Value cell)
{
	return ((Cons *) as_ref(cell))->car;
}

Value cadr(Value cell)
{
	return car(cdr(cell));
}

Value caddr(Value cell)
{
	return car(cdr(cdr(cell)));
}

Value cadddr(Value cell)
{
	return car(cdr(cdr(cdr(cell))));
}

Value cdr(Value cell)
{
	return ((Cons *) as_ref(cell))->cdr;
}

Cons *cons(Value car, Value cdr)
{
	Cons *cell = alloc(CONS, sizeof(*cell));
	cell->car = car;
	cell->cdr = cdr;
	return cell;
}

bool is_cons(Value v)
{
	return get_type(v) == CONS;
}

unsigned list_len(Value v)
{
	unsigned r = 0;

	while (is_cons(v)) {
		r++;
		v = cdr(v);
	}

	return r;
}

void lb_init(ListBuilder *lb)
{
	lb->head = lb->tail = NULL;
}

void lb_append(ListBuilder *lb, Value v)
{
	Cons *new_cell = cons(v, mk_nil());
	if (lb->head) {
		lb->tail->cdr = mk_ref(new_cell);
		lb->tail = new_cell;
	} else
		lb->head = lb->tail = new_cell;
}

Value lb_get(ListBuilder *lb)
{
	return lb->head ? mk_ref(lb->head) : mk_nil();
}

/*----------------------------------------------------------------*/

