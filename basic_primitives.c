#include <stdio.h>
#include <stdlib.h>

#include "interpreter.h"

/*----------------------------------------------------------------*/

/*----------------------------------------------------------------
 * Primitives
 *--------------------------------------------------------------*/
static void clear(struct interpreter *terp)
{
	terp->stack.nr_entries = 0;
}

static void call(struct interpreter *terp)
{
	value_t maybe_q = POP();

	if (get_type(maybe_q) != QUOT) {
		fprintf(stderr, "not a quotation\n");
		exit(1);
	}

	interpret_quot(terp, as_ref(maybe_q));
}

static void curry(struct interpreter *terp)
{
	value_t q = POP();
	value_t obj = POP();
	value_t new_q = mk_quot();
	struct array *a = as_ref(q);
	unsigned i;

	append_array(new_q, obj);
	for (i = 0; i < a->nr_elts; i++)
		append_array(new_q, a->elts[i]);

	PUSH(new_q);
}

static void dot(struct interpreter *terp)
{
	value_t v = POP();
	print_value(stdout, v);
	printf("\n");
}

static void ndrop(struct interpreter *terp)
{
	unsigned i;
	value_t v = POP();

	for (i = as_fixnum(v); i; i--)
		POP();
}

static void nnip(struct interpreter *terp)
{
	unsigned i;
	value_t v = POP();
	value_t restore = POP();

	for (i = as_fixnum(v); i; i--)
		POP();

	PUSH(restore);
}

static void _dup(struct interpreter *terp)
{
	value_t v = PEEK();
	PUSH(v);
}

static void _dup2(struct interpreter *terp)
{
	value_t y = PEEK();
	value_t x = PEEKN(1);
	PUSH(x);
	PUSH(y);
}

static void _dup3(struct interpreter *terp)
{
	value_t z = PEEK();
	value_t y = PEEKN(1);
	value_t x = PEEKN(2);

	PUSH(x);
	PUSH(y);
	PUSH(z);
}

static void over(struct interpreter *terp)
{
	PUSH(PEEKN(1));
}

static void over2(struct interpreter *terp)
{
	PUSH(PEEKN(2));
	PUSH(PEEKN(2));
}

static void pick(struct interpreter *terp)
{
	PUSH(PEEKN(2));
}

static void swap(struct interpreter *terp)
{
	value_t v1 = POP();
	value_t v2 = POP();
	PUSH(v1);
	PUSH(v2);
}

static void dip(struct interpreter *terp)
{
	value_t q = POP();
	value_t x = POP();
	PUSH(q);
	call(terp);
	PUSH(x);
}

static void fixnum_add(struct interpreter *terp)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v1) + as_fixnum(v2)));
}

static void fixnum_sub(struct interpreter *terp)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) - as_fixnum(v1)));
}

static void fixnum_mult(struct interpreter *terp)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) * as_fixnum(v1)));
}

static void fixnum_div(struct interpreter *terp)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) / as_fixnum(v1)));
}

static void each(struct interpreter *terp)
{
	value_t q = POP();
	value_t a = POP();
	struct array *ary;
	unsigned i;

	if (get_type(q) != QUOT) {
		fprintf(stderr, "not a quotation\n");
		exit(1);
	}

	if (get_type(a) != ARRAY) {
		fprintf(stderr, "not an array\n");
		exit(1);
	}

	ary = as_ref(a);
	for (i = 0; i < ary->nr_elts; i++) {
		PUSH(ary->elts[i]);
		interpret_quot(terp, as_ref(q));
	}
}

static void choice(struct interpreter *terp)
{
	value_t f = POP();
	value_t t = POP();
	value_t p = POP();

	if (is_false(p))
		PUSH(f);
	else
		PUSH(t);
}

void add_basic_primitives(struct interpreter *terp)
{
	add_primitive(terp, "clear", clear);
	add_primitive(terp, ".", dot);
	add_primitive(terp, "ndrop", ndrop);
	add_primitive(terp, "nnip", nnip);
	add_primitive(terp, "dup", _dup);
	add_primitive(terp, "2dup", _dup2);
	add_primitive(terp, "3dup", _dup3);
	add_primitive(terp, "over", over);
	add_primitive(terp, "2over", over2);
	add_primitive(terp, "pick", pick);
	add_primitive(terp, "swap", swap);
	add_primitive(terp, "dip", dip);

	add_primitive(terp, "+", fixnum_add);
	add_primitive(terp, "-", fixnum_sub);
	add_primitive(terp, "*", fixnum_mult);
	add_primitive(terp, "/", fixnum_div);

	add_primitive(terp, "call", call);
	add_primitive(terp, "curry", curry);

	add_primitive(terp, "?", choice);

	add_primitive(terp, "each", each);
}

/*----------------------------------------------------------------*/
