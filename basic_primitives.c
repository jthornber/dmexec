#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

/*----------------------------------------------------------------
 * Primitives
 *--------------------------------------------------------------*/
static void clear(struct vm *vm)
{
	vm->k->stack.nr_entries = 0;
}

static void call(struct vm *vm)
{
	value_t callable = POP();

	switch (get_type(callable)) {
	case QUOT:
		push_call(vm, as_ref(callable));
		break;

	default:
		fprintf(stderr, "not a callable: ");
		print_value(stderr, callable);
		fprintf(stderr, "\n");
		exit(1);
	}
}

static void callcc0(struct vm *vm)
{
	value_t quot = POP();
	struct code_position *cp, *new_cp;
	struct continuation *k = alloc(CONTINUATION, sizeof(*k));

	memcpy(&k->stack, &vm->k->stack, sizeof(k->stack));
	INIT_LIST_HEAD(&k->call_stack);

	list_for_each_entry (cp, &vm->k->call_stack, list) {
		new_cp = alloc(CODE_POSITION, sizeof(*cp));
		new_cp->code = cp->code;
		new_cp->position = cp->position;
		list_add_tail(&new_cp->list, &k->call_stack);
	}

	PUSH(mk_ref(k));
	PUSH(quot);
	call(vm);
}

static void continue_cc(struct vm *vm)
{
	value_t k = POP();

	if (get_type(k) != CONTINUATION) {
		fprintf(stderr, "not a continuation\n");
		exit(1);
	}

	vm->k = as_ref(k);
}

static void curry(struct vm *vm)
{
	value_t q = POP();
	value_t obj = POP();
	struct array *a = as_ref(q);
	struct array *new_q;
	unsigned i;

	// FIXME: it would be nice to use array_unshift
	new_q = quot_create(a->nr_elts + 1);
	array_push(new_q, obj);
	for (i = 0; i < a->nr_elts; i++)
		array_push(new_q, array_get(a, i));
	PUSH(mk_ref(new_q));
}

static void dot(struct vm *vm)
{
	value_t v = POP();
	print_value(stdout, v);
	printf("\n");
}

static void ndrop(struct vm *vm)
{
	unsigned i;
	value_t v = POP();

	for (i = as_fixnum(v); i; i--)
		POP();
}

static void nnip(struct vm *vm)
{
	unsigned i;
	value_t v = POP();
	value_t restore = POP();

	for (i = as_fixnum(v); i; i--)
		POP();

	PUSH(restore);
}

static void _dup(struct vm *vm)
{
	value_t v = PEEK();
	PUSH(v);
}

static void _dup2(struct vm *vm)
{
	value_t y = PEEK();
	value_t x = PEEKN(1);
	PUSH(x);
	PUSH(y);
}

static void _dup3(struct vm *vm)
{
	value_t z = PEEK();
	value_t y = PEEKN(1);
	value_t x = PEEKN(2);

	PUSH(x);
	PUSH(y);
	PUSH(z);
}

static void over(struct vm *vm)
{
	PUSH(PEEKN(1));
}

static void over2(struct vm *vm)
{
	PUSH(PEEKN(2));
	PUSH(PEEKN(2));
}

static void pick(struct vm *vm)
{
	PUSH(PEEKN(2));
}

static void swap(struct vm *vm)
{
	value_t v1 = POP();
	value_t v2 = POP();
	PUSH(v1);
	PUSH(v2);
}

static void dip(struct vm *vm)
{
#if 1
	// bi only works with this branch
	value_t q = POP();
	value_t x = POP();

	value_t after = mk_quot();
	array_push(as_ref(after), x);
	push_call(vm, as_ref(after));

	PUSH(q);
	call(vm);
#else
	value_t q = POP();
	value_t x = POP();

	PUSH(q);
	call(vm);
	PUSH(x);
#endif
}

static void fixnum_add(struct vm *vm)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v1) + as_fixnum(v2)));
}

static void fixnum_sub(struct vm *vm)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) - as_fixnum(v1)));
}

static void fixnum_mult(struct vm *vm)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) * as_fixnum(v1)));
}

static void fixnum_div(struct vm *vm)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) / as_fixnum(v1)));
}

static void each(struct vm *vm)
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
		PUSH(array_get(ary, i));
		push_call(vm, as_ref(q));
	}
}

static void choice(struct vm *vm)
{
	value_t f = POP();
	value_t t = POP();
	value_t p = POP();

	if (is_false(p))
		PUSH(f);
	else
		PUSH(t);
}

static void mk_tuple(struct vm *vm)
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

	def_primitive(vm, "each", each);

	def_primitive(vm, "callcc0", callcc0);
	def_primitive(vm, "continue", continue_cc);

	def_primitive(vm, "mk-tuple", mk_tuple);
}

/*----------------------------------------------------------------*/
