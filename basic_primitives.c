#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

/*----------------------------------------------------------------
 * Primitives
 *--------------------------------------------------------------*/
static void clear(void)
{
	struct array *s = as_ref(global_vm->k->stack);
	s->nr_elts = 0;
}

static void call(void)
{
	value_t callable = POP();

	switch (get_type(callable)) {
	case QUOT:
		push_call(as_ref(callable));
		break;

	default:
		error("not a callable");
		//print_value(stderr, callable);
	}
}

static void callcc0(void)
{
	value_t quot = POP();
	struct continuation *k = alloc(CONTINUATION, sizeof(*k));

	k->stack = clone_value(global_vm->k->stack);
	k->call_stack = clone_value(global_vm->k->call_stack);

	PUSH(mk_ref(k));
	push_call(as_type(QUOT, quot));
}

static void continue_cc(void)
{
	value_t k = POP();

	if (get_type(k) != CONTINUATION)
		error("not a continuation");

	global_vm->k = as_ref(k);
}

static void curry(void)
{
	value_t q = POP();
	value_t obj = POP();
	struct array *a = as_ref(q);
	struct array *new_q;
	unsigned i;

	// FIXME: it would be nice to use array_unshift
	new_q = quot_create();
	array_push(new_q, obj);
	for (i = 0; i < a->nr_elts; i++)
		array_push(new_q, array_get(a, i));
	PUSH(mk_ref(new_q));
}

static void dot(void)
{
	value_t v = POP();
	print_value(stdout, v);
	printf("\n");
}

static void ndrop(void)
{
	unsigned i;
	value_t v = POP();

	for (i = as_fixnum(v); i; i--)
		POP();
}

static void nnip(void)
{
	unsigned i;
	value_t v = POP();
	value_t restore = POP();

	for (i = as_fixnum(v); i; i--)
		POP();

	PUSH(restore);
}

static void _dup(void)
{
	value_t v = PEEK();
	PUSH(v);
}

static void _dup2(void)
{
	value_t y = PEEK();
	value_t x = PEEKN(1);
	PUSH(x);
	PUSH(y);
}

static void _dup3(void)
{
	value_t z = PEEK();
	value_t y = PEEKN(1);
	value_t x = PEEKN(2);

	PUSH(x);
	PUSH(y);
	PUSH(z);
}

static void over(void)
{
	PUSH(PEEKN(1));
}

static void over2(void)
{
	PUSH(PEEKN(2));
	PUSH(PEEKN(2));
}

static void pick(void)
{
	PUSH(PEEKN(2));
}

static void swap(void)
{
	value_t v1 = POP();
	value_t v2 = POP();
	PUSH(v1);
	PUSH(v2);
}

static void dip(void)
{
#if 1
	// bi only works with this branch
	value_t q = POP();
	value_t x = POP();

	value_t after = mk_quot();
	array_push(as_ref(after), x);
	push_call(as_ref(after));

	PUSH(q);
	call();
#else
	value_t q = POP();
	value_t x = POP();

	PUSH(q);
	call(vm);
	PUSH(x);
#endif
}

static void fixnum_add(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v1) + as_fixnum(v2)));
}

static void fixnum_sub(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) - as_fixnum(v1)));
}

static void fixnum_mult(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) * as_fixnum(v1)));
}

static void fixnum_div(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) / as_fixnum(v1)));
}

static void narray(void)
{
	int n = as_fixnum(POP());
	struct array *a = array_create();

	for (unsigned i = 0; i < n; i++)
		a = array_push(a, POP());

	array_reverse(a);
	PUSH(mk_ref(a));
}

static void each(void)
{
	value_t q = POP();
	value_t a = POP();
	struct array *ary, *computation = quot_create();
	unsigned i;

	if (get_type(q) != QUOT)
		error("not a quotation");

	if (get_type(a) != ARRAY)
		error("not an array");

	// Build up a single quotation that does all the work
	ary = as_ref(a);
	for (i = 0; i < ary->nr_elts; i++) {
		computation = array_push(computation, array_get(ary, i));
		computation = array_concat(computation, as_ref(q));
	}

	push_call(computation);
}

static void map(void)
{
	value_t q = POP();
	value_t a = POP();
	struct array *ary, *computation = quot_create();
	unsigned i;

	if (get_type(q) != QUOT)
		error("not a quotation");

	if (get_type(a) != ARRAY)
		error("not an array");

	// Build up a single quotation that does all the work
	ary = as_ref(a);
	for (i = 0; i < ary->nr_elts; i++) {
		computation = array_push(computation, array_get(ary, i));
		computation = array_concat(computation, as_ref(q));
	}

	computation = array_push(computation, mk_fixnum(ary->nr_elts));
	computation = array_push(computation, mk_word_cstr("narray"));

	push_call(computation);
}

static void choice(void)
{
	value_t f = POP();
	value_t t = POP();
	value_t p = POP();

	if (is_false(p))
		PUSH(f);
	else
		PUSH(t);
}

static void mk_tuple(void)
{
	unsigned i;
	value_t name = POP();
	int count = as_fixnum(POP());
	struct array *a = alloc(TUPLE, sizeof(*a));
	value_t r = mk_ref(a);

	a->nr_elts = count + 1;
	array_push(a, name);

	for (i = 0; i < count; i++)
		array_push(a, PEEKN(i));

	for (i = 0; i < count; i++)
		POP();

	PUSH(r);
}

void def_basic_primitives(struct vm *vm)
{
	def_primitive(vm, "clear", clear);
	def_primitive(vm, ".", dot);
	def_primitive(vm, "ndrop", ndrop);
	def_primitive(vm, "nnip", nnip);
	def_primitive(vm, "dup", _dup);
	def_primitive(vm, "2dup", _dup2);
	def_primitive(vm, "3dup", _dup3);
	def_primitive(vm, "over", over);
	def_primitive(vm, "2over", over2);
	def_primitive(vm, "pick", pick);
	def_primitive(vm, "swap", swap);
	def_primitive(vm, "dip", dip);

	def_primitive(vm, "+", fixnum_add);
	def_primitive(vm, "-", fixnum_sub);
	def_primitive(vm, "*", fixnum_mult);
	def_primitive(vm, "/", fixnum_div);

	def_primitive(vm, "call", call);
	def_primitive(vm, "curry", curry);

	def_primitive(vm, "?", choice);

	def_primitive(vm, "narray", narray);
	def_primitive(vm, "each", each);
	def_primitive(vm, "map", map);

	def_primitive(vm, "callcc0", callcc0);
	def_primitive(vm, "continue", continue_cc);

	def_primitive(vm, "mk-tuple", mk_tuple);
}

/*----------------------------------------------------------------*/
