#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#include "namespace.h"
#include "vm.h"

/*----------------------------------------------------------------
 * Primitives
 *--------------------------------------------------------------*/
static void clear(void)
{
	global_vm->k->data_stack->nr_elts = 0;
	inc_pc();
}

static void call(void)
{
	value_t callable = POP();

	switch (get_type(callable)) {
	case QUOT:
		inc_pc();
		push_call(as_ref(callable));
		break;

	default:
		error("not a callable");
		//print_value(stderr, callable);
	}
}

static void current_continuation(void)
{
	PUSH(mk_ref(cc(global_vm)));
	inc_pc();
}

static void continue_with(void)
{
	value_t k = POP_TYPE(CONTINUATION);
	value_t obj = POP();

	global_vm->k = as_ref(k);
	PUSH(obj);
	inc_pc();
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
	new_q = array_push(new_q, obj);
	for (i = 0; i < a->nr_elts; i++)
		new_q = array_push(new_q, array_get(a, i));
	PUSH(mk_ref(new_q));
	inc_pc();
}

static void dot(void)
{
	value_t v = POP();
	print_value(stdout, v);
	printf("\n");
	inc_pc();
}

static void ndrop(void)
{
	unsigned i;
	value_t v = POP();

	for (i = as_fixnum(v); i; i--)
		POP();
	inc_pc();
}

static void nnip(void)
{
	unsigned i;
	value_t v = POP();
	value_t restore = POP();

	for (i = as_fixnum(v); i; i--)
		POP();

	PUSH(restore);
	inc_pc();
}

static void _dup(void)
{
	value_t v = PEEK();
	PUSH(v);
	inc_pc();
}

static void _dup2(void)
{
	value_t y = PEEK();
	value_t x = PEEKN(1);
	PUSH(x);
	PUSH(y);
	inc_pc();
}

static void _dup3(void)
{
	value_t z = PEEK();
	value_t y = PEEKN(1);
	value_t x = PEEKN(2);

	PUSH(x);
	PUSH(y);
	PUSH(z);
	inc_pc();
}

static void over(void)
{
	PUSH(PEEKN(1));
	inc_pc();
}

static void over2(void)
{
	PUSH(PEEKN(2));
	PUSH(PEEKN(2));
	inc_pc();
}

static void pick(void)
{
	PUSH(PEEKN(2));
	inc_pc();
}

static void swap(void)
{
	value_t v1 = POP();
	value_t v2 = POP();
	PUSH(v1);
	PUSH(v2);
	inc_pc();
}

static void dip(void)
{
	value_t q = POP();
	value_t x = POP();

	value_t after = mk_quot();
	array_push(as_ref(after), x);
	inc_pc();
	push_call(as_ref(after));
	push_call(as_ref(q));
}

static void fixnum_add(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v1) + as_fixnum(v2)));
	inc_pc();
}

static void fixnum_sub(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) - as_fixnum(v1)));
	inc_pc();
}

static void fixnum_mult(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) * as_fixnum(v1)));
	inc_pc();
}

static void fixnum_div(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	PUSH(mk_fixnum(as_fixnum(v2) / as_fixnum(v1)));
	inc_pc();
}

static void narray(void)
{
	int n = as_fixnum(POP());
	struct array *a = array_create();

	for (unsigned i = 0; i < n; i++)
		a = array_push(a, POP());

	array_reverse(a);
	PUSH(mk_ref(a));
	inc_pc();
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

	inc_pc();
	push_call(computation);
}

static void _push(void)
{
	struct array *a = as_ref(POP_TYPE(ARRAY));
	array_push(a, POP());	/* FIXME: does the forward ptr handle resizing ok? */
	inc_pc();
}

static void _pop(void)
{
	struct array *a = as_ref(POP_TYPE(ARRAY));
	PUSH(array_pop(a));
	inc_pc();
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

	inc_pc();
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
	inc_pc();
}

static void mk_tuple(void)
{
	unsigned i;
	value_t name = POP();
	int count = as_fixnum(POP());
	struct array *a = alloc(TUPLE, sizeof(*a));
	value_t r = mk_ref(a);

	a->nr_elts = count + 1;
	a = array_push(a, name);

	for (i = 0; i < count; i++)
		a = array_push(a, PEEKN(i));

	for (i = 0; i < count; i++)
		POP();

	PUSH(r);
	inc_pc();
}

static void mk_namespace(void)
{
	struct namespace *n = namespace_create(NULL);
	PUSH(mk_ref(n));
	inc_pc();
}

static void namestack_star(void)
{
	PUSH(mk_ref(global_vm->current_ns));
	inc_pc();
}

static void namespace_get(void)
{
	value_t v;
	struct string *k = as_ref(POP_TYPE(SYMBOL));

	if (namespace_lookup(global_vm->current_ns, k, &v))
		PUSH(v);
	else
		PUSH(mk_false());
	inc_pc();
}

static void namespace_set(void)
{
	struct string *k = as_ref(POP_TYPE(SYMBOL));
	value_t v = POP();
	namespace_insert(global_vm->current_ns, k, v);
	inc_pc();
}

static void namespace_push(void)
{
	struct namespace *n = as_ref(POP_TYPE(NAMESPACE));
	n->parent = global_vm->current_ns;
	global_vm->current_ns = n;
	inc_pc();
}

static void namespace_pop(void)
{
	if (!global_vm->current_ns->parent)
		error("cannot remove global namespace");
	global_vm->current_ns = global_vm->current_ns->parent;
	inc_pc();
}

static void throw_error(void)
{
	struct string *msg = as_ref(POP_TYPE(STRING));
	fprintf(stderr, "throwing: ");
	print_string(stderr, msg);
	fprintf(stderr, "\n");
	throw();
}

static void rethrow_error(void)
{
	fprintf(stderr, "rethrowing");
	throw();
}

static void catch_stack(void)
{
	PUSH(mk_ref(global_vm->k->catch_stack));
	inc_pc();
}

static bool ll_eq(value_t v1, value_t v2)
{
	enum object_type t = get_type(v1);

	if (get_type(v2) != t)
		return false;

	// FIXME: if refs are equal then we don't need to do a deep comparison
	switch (t) {
	case ARRAY:
		// FIXME: factor out
	{
		unsigned i;
		struct array *a1 = v1.ptr;
		struct array *a2 = v2.ptr;

		if (a1->nr_elts != a2->nr_elts)
			return false;

		for (i = 0; i < a1->nr_elts; i++)
			if (!ll_eq(array_get(a1, i),
				   array_get(a2, i)))
				return false;

		return true;
	}

	case FORWARD:
	case NAMESPACE:
	case NAMESPACE_ENTRY:
	case PRIMITIVE:
	case STRING:
	case BYTE_ARRAY:
	case TUPLE:
	case SYMBOL:
	case WORD:
	case QUOT:

	case CODE_POSITION:
	case CONTINUATION:
		error("eq not implemented for this type");

	case FIXNUM:
		return as_fixnum(v1) == as_fixnum(v2);

	case FALSE_TYPE:
		return true;
	}

	return false;
}

static void eq(void)
{
	value_t v1 = POP();
	value_t v2 = POP();

	if (ll_eq(v1, v2))
		PUSH(mk_true());
	else
		PUSH(mk_false());

	inc_pc();
}

void def_basic_primitives(struct vm *vm)
{
	def_primitive(vm, "eq", eq);

	def_primitive(vm, "catch-stack", catch_stack);
	def_primitive(vm, "throw", throw_error);
	def_primitive(vm, "rethrow", rethrow_error);

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
	def_primitive(vm, "push", _push);
	def_primitive(vm, "pop", _pop);

	def_primitive(vm, "current-continuation", current_continuation);
	def_primitive(vm, "continue-with", continue_with);

	def_primitive(vm, "mk-tuple", mk_tuple);

	def_primitive(vm, "namespace", mk_namespace);
	def_primitive(vm, "namestack*", namestack_star); /* FIXME: I'm not sure this is needed */

	def_primitive(vm, "get", namespace_get);
	def_primitive(vm, "set", namespace_set);

	def_primitive(vm, "namespace-push", namespace_push);
	def_primitive(vm, "namespace-pop", namespace_pop);
}

/*----------------------------------------------------------------*/
