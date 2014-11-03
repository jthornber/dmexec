#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm.h"

/*----------------------------------------------------------------*/

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
	value_t new_q = mk_quot();
	struct array *a = as_ref(q);
	unsigned i;

	append_array(new_q, obj);
	for (i = 0; i < a->nr_elts; i++)
		append_array(new_q, a->elts[i]);

	PUSH(new_q);
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
#if 0
	value_t q = POP();
	value_t x = POP();

	value_t after = mk_quot();
	append_array(after, x);
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
		PUSH(ary->elts[i]);
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

void add_basic_primitives(struct vm *vm)
{
	add_primitive(vm, "clear", clear);
	add_primitive(vm, ".", dot);
	add_primitive(vm, "ndrop", ndrop);
	add_primitive(vm, "nnip", nnip);
	add_primitive(vm, "dup", _dup);
	add_primitive(vm, "2dup", _dup2);
	add_primitive(vm, "3dup", _dup3);
	add_primitive(vm, "over", over);
	add_primitive(vm, "2over", over2);
	add_primitive(vm, "pick", pick);
	add_primitive(vm, "swap", swap);
	add_primitive(vm, "dip", dip);

	add_primitive(vm, "+", fixnum_add);
	add_primitive(vm, "-", fixnum_sub);
	add_primitive(vm, "*", fixnum_mult);
	add_primitive(vm, "/", fixnum_div);

	add_primitive(vm, "call", call);
	add_primitive(vm, "curry", curry);

	add_primitive(vm, "?", choice);

	add_primitive(vm, "each", each);

	add_primitive(vm, "callcc0", callcc0);
	add_primitive(vm, "continue", continue_cc);
}

/*----------------------------------------------------------------*/
