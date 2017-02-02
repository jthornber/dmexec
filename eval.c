#include "cons.h"
#include "symbol.h"
#include "vm.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

//----------------------------------------------------------------
// Thunks

static Thunk *t_new(size_t size)
{
	Thunk *r = untyped_zalloc(sizeof(*r) + size);
	r->b = (uint8_t *) (r + 1);
	r->e = r->b;
	r->alloc_e = r->b + size;
	return r;
}

static size_t t_size(Thunk *t)
{
	return t->e - t->b;
}

static void t_append(Thunk *t, uint8_t byte)
{
	if (t->e == t->alloc_e) {
		size_t old_len = t->alloc_e - t->b;
		// FIXME: make sure this throws an exception or calls error on
		// failure.
		t->b = realloc(t->b, 2 * old_len);
		t->e = t->b + old_len;
		t->alloc_e = t->b + 2 * old_len;
	}

	*t->e++ = byte;
}

static Thunk *t_merge(Thunk *lhs, Thunk *rhs)
{
	Thunk *t = t_new(t_size(lhs) + t_size(rhs));
	memcpy(t->b, lhs->b, t_size(lhs));
	memcpy(t->b + t_size(lhs), rhs->b, t_size(rhs));
	t->e = t->alloc_e;
	return t;
}

//----------------------------------------------------------------
// Runtime environment

static void push_v(Stack *s, Value v)
{
	assert(s->current < MAX_STACK);
	s->sp[s->current++] = v;
}

static Value pop_v(Stack *s)
{
	assert(s->current);
	return s->sp[s->current--];
}

static void push_p(Stack *s, void *ptr)
{
	assert(s->current < MAX_STACK);
	s->sp[s->current++].ptr = ptr;
}

static void *pop_p(Stack *s)
{
	assert(s->current);
	return s->sp[s->current--].ptr;
}

static void *peek_p(Stack *s)
{
	assert(s->current);
	return s->sp[s->current].ptr;
}

//----------------------------------------------------------------

static Frame *f_new(unsigned count)
{
	Frame *f = untyped_alloc(sizeof(*f) + count * sizeof(Value));
	f->next = NULL;
	return f;
}

static Value f_get(Frame *f, unsigned index)
{
	assert(index < f->nr);
	return f->values[index];
}

static Value f_deep_get(Frame *f, unsigned depth, unsigned index)
{
	while (depth--) {
		f = f->next;
		if (!f)
			error("bad fetch");
	}

	return f_get(f, index);
}

static void f_set(Frame *f, unsigned index, Value v)
{
	assert(index < f->nr);
	f->values[index] = v;
}

static void f_deep_set(Frame *f, unsigned depth, unsigned index, Value v)
{
	while (depth--) {
		f = f->next;
		assert(f);
	}

	f_set(f, index, v);
}

//----------------------------------------------------------------
// Closure

// We don't need to store the arity or nary status since that gets compiled
// into the thunk; the thunk knows how to prepare the frame from the stack
// contents.
// FIXME: if we're doing it this way then call/cc is back on the table.
typedef struct {
	Thunk code;
	Frame *env;
} Closure;

//----------------------------------------------------------------
// Virtual Machine

typedef enum {
	ALLOCATE_DOTTED_FRAME,
	ALLOCATE_FRAME,
	ARITY_EQ,
	ARITY_GEQ,

	CALL0,
	CALL1,
	CALL2,
	CALL3,

	CHECKED_GLOBAL_REF,
	CONSTANT,
	CREATE_CLOSURE,

	DEEP_ARGUMENT_REF,
	DEEP_ARGUMENT_SET,

	ENV_EXTEND,
	ENV_PRESERVE,
	ENV_RESTORE,
	ENV_UNLINK,

	FINISH,

	GLOBAL_REF,
	GLOBAL_SET,

	GOTO,
	INVOKE,

	JUMP_FALSE,
	PACK_ARG,
	POP_ARG1,
	POP_ARG2,
	POP_CONS_FRAME,
	POP_FRAME,
	POP_FUNCTION,
	PREDEFINED,
	RETURN,

	SHALLOW_ARGUMENT_REF,
	SHALLOW_ARGUMENT_SET,

	VALUE_POP,
	VALUE_PUSH,
} ByteOp;

static inline ByteOp shift_op(Thunk *t)
{
	return (ByteOp) *t->b++;
}

static inline uint8_t shift8(Thunk *t)
{
	if (t->b >= t->e)
		error("ran off the end of the bytecode");

	return *t->b++;
}

static inline uint16_t shift16(Thunk *t)
{
	uint16_t r = shift8(t);
	r = r << 8;
	r |= shift8(t);
	return r;
}

// Returns false if program exits
static inline bool step(VM *vm)
{
	uint8_t i, j;
	uint16_t ij;
	Frame *f;
	Closure *c;

	switch (shift_op(vm->code)) {
	case ALLOCATE_DOTTED_FRAME:
		vm->val = mk_ref(f_new(shift8(vm->code) + 1));
		break;

	case ALLOCATE_FRAME:
		push_p(&vm->stack, f_new(shift8(vm->code)));
		break;

	case ARITY_EQ:
		break;

	case ARITY_GEQ:
		break;

	case CALL0:
		break;

	case CALL1:
		break;

	case CALL2:
		break;

	case CALL3:
		break;

	case CHECKED_GLOBAL_REF:
		break;

	case CONSTANT:
		break;

	case CREATE_CLOSURE:
		i = shift8(vm->code);
		j = shift16(vm->code);
		c = alloc(CLOSURE, sizeof(*c));
		c->code.b = vm->code->b + i;
		c->code.e = vm->code->b + j;
		c->env = vm->env;
		vm->val = mk_ref(c);
		break;

	case DEEP_ARGUMENT_REF:
		assert(vm->env);
		i = shift8(vm->code);
		j = shift8(vm->code);
		f_deep_get(vm->env, i, j);
		break;

	case ENV_EXTEND:
		f = as_ref(vm->val);
		f->next = vm->env;
		vm->env = f;
		break;

	case FINISH:
		return false;
		break;

	case GLOBAL_REF:
		break;

	case GOTO:
		vm->code->b += shift16(vm->code);
		break;

	case INVOKE:
		break;

	case JUMP_FALSE:
		ij = shift16(vm->code);
		if (is_nil(vm->val))
			vm->code->b += ij;
		break;

	case PACK_ARG:
		f_set(peek_p(&vm->stack), shift8(vm->code), vm->val);
		break;

	case POP_ARG1:
		vm->arg1 = pop_v(&vm->stack);
		break;

	case POP_ARG2:
		vm->arg2 = pop_v(&vm->stack);
		break;

	case POP_CONS_FRAME:
		break;

	case POP_FRAME:
		f = as_ref(vm->val);
		f->next = vm->env;
		vm->env = f;
		break;

	case POP_FUNCTION:
		break;

	case PREDEFINED:
		break;

	case ENV_PRESERVE:
		push_p(&vm->stack, vm->env);
		break;

	case VALUE_PUSH:
		push_v(&vm->stack, vm->val);
		break;

	case ENV_RESTORE:
		vm->env = pop_p(&vm->stack);
		break;

	case RETURN:
		// FIXME: pop pc from stack? env?
		break;

	case DEEP_ARGUMENT_SET:
		// FIXME: variant of this op that packs i, j into a single byte?
		assert(vm->env);
		i = shift8(vm->code);
		j = shift8(vm->code);
		f_deep_set(vm->env, i, j, vm->val);
		break;

	case GLOBAL_SET:
		break;

	case SHALLOW_ARGUMENT_SET:
		assert(vm->env);
		f_set(vm->env, shift8(vm->code), vm->val);
		break;

	case SHALLOW_ARGUMENT_REF:
		assert(vm->env);
		vm->val = f_get(vm->env, shift8(vm->code));
		break;

	case ENV_UNLINK:
		assert(vm->env);
		vm->env = vm->env->next;
		break;

	case VALUE_POP:
		break;
	}

	return true;
}

static void run(VM *vm)
{
	while (step(vm))
		;
}

static unsigned add_constant(Value v)
{
	// FIXME: finish
	return 0;
}

static void disassemble(Thunk *t)
{
	switch (shift_op(t)) {
	case ALLOCATE_DOTTED_FRAME:
		printf("allocate_dotted_frame");
		break;

	case ALLOCATE_FRAME:
		printf("allocate_frame");
		break;

	case ARITY_EQ:
		printf("arity_eq");
		break;

	case ARITY_GEQ:
		printf("arity_geq");
		break;

	case CALL0:
		printf("call0");
		break;

	case CALL1:
		printf("call1");
		break;

	case CALL2:
		printf("call2");
		break;

	case CALL3:
	     	printf("call3");
		break;

	case CHECKED_GLOBAL_REF:
		printf("checked_global_ref");
		break;

	case CONSTANT:
		printf("constant");
		break;

	case CREATE_CLOSURE:
		printf("create_closure");
		break;

	case DEEP_ARGUMENT_REF:
		printf("deep_argument_ref");
		break;

	case DEEP_ARGUMENT_SET:
		printf("deep_argument_set");
		break;

	case ENV_EXTEND:
		printf("env_extend");
		break;

	case ENV_PRESERVE:
		printf("env_preserve");
		break;

	case ENV_RESTORE:
		printf("env_restore");
		break;

	case ENV_UNLINK:
		printf("env_unlink");
		break;

	case FINISH:
		printf("finish");
		break;

	case GLOBAL_REF:
		printf("global_ref");
		break;

	case GLOBAL_SET:
		printf("global_set");
		break;

	case GOTO:
		printf("goto");
		break;

	case INVOKE:
		printf("invoke");
		break;

	case JUMP_FALSE:
		printf("jump_false");
		break;

	case PACK_ARG:
		printf("pack_arg");
		break;

	case POP_ARG1:
		printf("pop_arg1");
		break;

	case POP_ARG2:
		printf("pop_arg2");
		break;

	case POP_CONS_FRAME:
		printf("pop_cons_frame");
		break;

	case POP_FRAME:
		printf("pop_frame");
		break;

	case POP_FUNCTION:
		printf("pop_function");
		break;

	case PREDEFINED:
		printf("predefined");
		break;

	case RETURN:
		printf("return");
		break;

	case SHALLOW_ARGUMENT_REF:
		printf("shallow_argument_ref");
		break;

	case SHALLOW_ARGUMENT_SET:
		printf("shallow_argument_set");
		break;

	case VALUE_POP:
		printf("value_pop");
		break;

	case VALUE_PUSH:
		printf("value_push");
		break;
	}
}

//----------------------------------------------------------------
// Intermediate instructions

static inline void op(Thunk *t, ByteOp o)
{
	t_append(t, (uint8_t) o);
}

static inline void op8(Thunk *t, ByteOp o, unsigned v)
{
	op(t, o);
	assert(v < 256);
	t_append(t, (uint8_t) v);
}

static inline void op8_8(Thunk *t, ByteOp o, unsigned i, unsigned j)
{
	op(t, o);
	assert(i < 256);
	t_append(t, (uint8_t) i);
	assert(j < 256);
	t_append(t, (uint8_t) j);
}

static inline void op8_16(Thunk *t, ByteOp o, unsigned i, unsigned j)
{
	op(t, o);
	assert(i < 256);
	t_append(t, (uint8_t) i);
	assert(j < 256 * 256);
	t_append(t, (uint8_t) j >> 8);
	t_append(t, (uint8_t) j & 0xff);
}

static inline void op16(Thunk *t, ByteOp o, unsigned v)
{
	op(t, o);
	assert(v < 256 * 256);
	t_append(t, (uint8_t) v >> 8);
	t_append(t, (uint8_t) v & 0xff);
}

static Thunk *i_shallow_argument_ref(unsigned i)
{
	Thunk *t = t_new(2);
	op8(t, SHALLOW_ARGUMENT_REF, i);
	return t;
}

static Thunk *i_shallow_argument_set(unsigned i, Thunk *val)
{
	Thunk *t = t_new(t_size(val) + 2);
	t_merge(t, val);
	op8(t, SHALLOW_ARGUMENT_SET, i);
	return t;
}

static Thunk *i_deep_argument_ref(unsigned i, unsigned j)
{
	Thunk *t = t_new(3);
	op8_8(t, DEEP_ARGUMENT_REF, i, j);
	return t;
}

static Thunk *i_deep_argument_set(unsigned i, unsigned j, Thunk *val)
{
	Thunk *t = t_new(t_size(val) + 3);
	t_merge(t, val);
	op8_8(t, DEEP_ARGUMENT_SET, i, j);
	return t;
}

static Thunk *i_checked_global_ref(unsigned i)
{
	Thunk *t = t_new(3);
	op16(t, GLOBAL_REF, i);
	return t;
}

static Thunk *i_global_set(unsigned i, Thunk *val)
{
	Thunk *t = t_new(t_size(val) + 3);
	t_merge(t, val);
	op16(t, GLOBAL_SET, i);
	return t;
}

static Thunk *i_predefined(unsigned i)
{
	Thunk *t = t_new(3);
	op16(t, PREDEFINED, i);
	return t;
}

static Thunk *i_constant(Value v)
{
	unsigned i = add_constant(v);

	Thunk *t = t_new(3);
	op16(t, CONSTANT, i);
	return t;
}

static Thunk *i_alternative(Thunk *t1, Thunk *t2, Thunk *t3)
{
	Thunk *t = t_new(t_size(t1) + t_size(t2) + t_size(t3) + 20);
	t_merge(t, t1);
	op16(t, JUMP_FALSE, t_size(t2) + 1);
	t_merge(t, t2);
	op16(t, GOTO, t_size(t3));
	t_merge(t, t3);
	return t;
}

static Thunk *i_sequence(Thunk *t1, Thunk *t2)
{
	Thunk *t = t_new(t_size(t1) + t_size(t2));
	t_merge(t, t1);
	t_merge(t, t2);
	return t;
}

static Thunk *i_fix_closure(Thunk *body, unsigned arity)
{
	Thunk *t, *fn;

	fn = t_new(t_size(body) + 20);
	op8(fn, ARITY_EQ, arity);
	t_merge(fn, body);
	op(fn, RETURN);

	t = t_new(t_size(fn) + 10);
	op8_16(t, CREATE_CLOSURE, 1, t_size(fn));
	op16(t, GOTO, t_size(fn));
	t_merge(t, fn);
	return t;
}

static Thunk *i_nary_closure(Thunk *body, unsigned arity)
{
	Thunk *t, *fn;

	fn = t_new(t_size(body) + 20);
	op8(fn, ARITY_GEQ, arity + 1);
	op(fn, ENV_EXTEND);
	t_merge(fn, body);
	op(fn, RETURN);

	t = t_new(t_size(fn) + 10);
	op8(t, CREATE_CLOSURE, 1);
	op16(t, GOTO, t_size(fn));
	t_merge(t, fn);
	return t;
}

static Thunk *i_regular_call(Thunk *fn, Thunk *args)
{
	Thunk *t = t_new(t_size(fn) + t_size(args) + 20);
	t_merge(t, fn); // leaves fn in vm->val, where fn is a closure
	op(t, VALUE_PUSH);
	t_merge(t, args);
	op(t, ENV_PRESERVE);
	op(t, INVOKE);
	return t;
}

static Thunk *i_tr_regular_call(Thunk *fn, Thunk *args)
{
	// FIXME: this isn't right
	Thunk *t = t_new(t_size(fn) + t_size(args) + 20);
	t_merge(t, fn);
	op(t, VALUE_PUSH);
	op(t, POP_FRAME);
	t_merge(t, args);
	op(t, ENV_PRESERVE);
	op(t, INVOKE);
	return t;
}

static Thunk *i_pack_args(unsigned argc, Thunk **args)
{
	Thunk *t;
	unsigned i, tot = 0;

	for (i = 0; i < argc; i++)
		tot += t_size(args[i]);

	t = t_new(tot + argc * 2 + 3);
	op8(t, ALLOCATE_FRAME, argc);
	for (i = 0; i < argc; i++) {
		t_merge(t, args[i]);
		op8(t, PACK_ARG, i);
	}
	op(t, VALUE_POP);

	return t;
}

//----------------------------------------------------------------
// Static Environment

typedef struct {

} StaticEnv;

static StaticEnv *r_extend(StaticEnv *r, Value ns)
{
	// FIXME: finish
	return r;
}

static bool symbol_eq(Symbol *sym, const char *name)
{
	return string_cmp_cstr(sym->str, name) == 0;
}

//----------------------------------------------------------------
// We can classify variables into three kinds:
// Local i, j where i is the frame depth, and j is the value index.
// Global i where i is an index into a global array.
// Predefined i where i is an index into an array of predefined (constant) values.

typedef enum {
	KindLocal,
	KindGlobal,
	KindPredefined
} KindType;

typedef struct {
	KindType t;
	unsigned i, j;
} Kind;

static Kind compute_kind(StaticEnv *r, Symbol *sym)
{
	error("not implemented");
	// FIXME: finish
	return (Kind) {KindLocal, 0, 0};
}

//----------------------------------------------------------------
// Compilation

Thunk *compile(Value e, StaticEnv *r, bool tail);


static Thunk *c_quotation(Value v, StaticEnv *r, bool tail)
{
	return i_constant(v);
}

static Thunk *c_reference(Value n, StaticEnv *r, bool tail)
{
	Kind k = compute_kind(r, as_ref(n));
	switch (k.t) {
	case KindLocal:
		if (k.i == 0)
			return i_shallow_argument_ref(k.j);
		else
			return i_deep_argument_ref(k.i, k.j);

	case KindGlobal:
		return i_checked_global_ref(k.i);

	case KindPredefined:
		return i_predefined(k.i);
	}

	return NULL;
}

static Thunk *c_assignment(Symbol *n, Value e, StaticEnv *r, bool tail)
{
	Thunk *t = compile(e, r, false);
	Kind k = compute_kind(r, n);

	switch (k.t) {
	case KindLocal:
		if (k.i == 0)
			return i_shallow_argument_set(k.j, t);
		else
			return i_deep_argument_set(k.i, k.j, t);

	case KindGlobal:
		return i_global_set(k.i, t);

	case KindPredefined:
		error("Predefined variables are immutable.");
	}

	return NULL;
}

static Thunk *c_alternative(Value e1, Value e2, Value e3, StaticEnv *r, bool tail)
{
	Thunk *t1 = compile(e1, r, false);
	Thunk *t2 = compile(e2, r, tail);
	Thunk *t3 = compile(e3, r, tail);

	return i_alternative(t1, t2, t3);
}

static Thunk *c_sequence(Value es, StaticEnv *r, bool tail);
static Thunk *c_multiple_seq(Value e, Value es, StaticEnv *r, bool tail)
{
	Thunk *t1 = compile(e, r, false);
	Thunk *t2 = c_sequence(es, r, tail);

	return i_sequence(t1, t2);
}

static Thunk *c_sequence(Value es, StaticEnv *r, bool tail)
{
	if (is_cons(es)) {
		if (is_cons(cdr(es)))
			return c_multiple_seq(car(es), cdr(es), r, tail);
		else
			return compile(car(es), r, tail);
	}

	error("bad sequence");
	return NULL;
}

static Thunk *c_fix_abstraction(Value ns, Value es, StaticEnv *r, bool tail)
{
	unsigned arity = list_len(ns);
	StaticEnv *r2 = r_extend(r, ns);
	Thunk *t = c_sequence(es, r2, true);

	return i_fix_closure(t, arity);
}

static Thunk *c_dotted_abstraction(Value ns, Value es, StaticEnv *r, bool tail)
{
	unsigned arity = list_len(ns) - 1; // extra param is already on here
	StaticEnv *r2 = r_extend(r, ns);
	Thunk *t = c_sequence(es, r2, tail);

	return i_nary_closure(t, arity);
}

static Thunk *c_abstraction(Value ns, Value es, StaticEnv *r, bool tail)
{
	ListBuilder lb;
	lb_init(&lb);

	while (is_cons(ns)) {
		lb_append(&lb, car(ns));
		ns = cdr(ns);
	}

	if (is_nil(ns))
		return c_fix_abstraction(lb_get(&lb), es, r, tail);

	else {
		lb_append(&lb, ns);
		return c_dotted_abstraction(lb_get(&lb), es, r, tail);
	}
}

static Thunk *c_args(Value es, StaticEnv *r, bool tail)
{
	// We're not supporting call/cc, so we can allocate the frame first.
	unsigned i, argc = list_len(es);
	Thunk *args[argc]; // Variable length arrays are in c99

	for (i = 0; i < argc; i++) {
		args[i] = compile(car(es), r, false);
		es = cdr(es);
	}

	return i_pack_args(argc, args);
}

static Thunk *c_regular_application(Value e, Value es, StaticEnv *r, bool tail)
{
	Thunk *fn = compile(e, r, false);
	Thunk *args = c_args(es, r, false);

	if (tail)
		return i_tr_regular_call(fn, args);
	else
		return i_regular_call(fn, args);
}

static Thunk *c_closed_application(Value e, Value es, StaticEnv *r, bool tail)
{
	// FIXME: finish
	return NULL;
}

static Thunk *c_primitive_application(Value e, Value es, StaticEnv *r, bool tail)
{
	// FIXME: finish
	return NULL;
}

static Thunk *c_application(Value e, Value es, StaticEnv *r, bool tail)
{
	// FIXME: check es is a proper list
	if (is_symbol(e)) {
		Kind k = compute_kind(r, as_ref(e));
		switch (k.t) {
		case KindLocal:
		case KindGlobal:
			return c_regular_application(e, es, r, tail);

		case KindPredefined:
			// FIXME: check the primitive is a function
			return c_primitive_application(e, es, r, tail);
		}

	} else if (is_cons(e) &&
		   is_symbol(car(e)) &&
		   symbol_eq(as_ref(car(e)), "lambda"))
		return c_closed_application(e, es, r, tail);

	else
		return c_regular_application(e, es, r, tail);

	return NULL;
}

// We assume that symbols can use ptr comparison.  Probably will always be
// true.
Thunk *compile(Value e, StaticEnv *r, bool tail)
{
	if (is_cons(e)) {
		if (is_symbol(car(e))) {
			Symbol *s = as_ref(car(e));
			if (symbol_eq(s, "quote"))
				return c_quotation(cadr(e), r, tail);

			else if (symbol_eq(s, "lambda"))
				return c_abstraction(cadr(e), caddr(e), r, tail);

			else if (symbol_eq(s, "if"))
				return c_alternative(cadr(e), caddr(e), cadddr(e),
					      	     r, tail);

			else if (symbol_eq(s, "begin"))
				return c_sequence(cdr(e), r, tail);

			else if (symbol_eq(s, "set!"))
				return c_assignment(as_ref(cadr(e)), caddr(e), r, tail);

			// Fall through
		}

		return c_application(car(e), cdr(e), r, tail);
	} else {
		if (is_symbol(e))
			return c_reference(e, r, tail);
		else
			return c_quotation(e, r, tail);
	}
}

Value eval(VM *vm, Value sexp)
{
	Thunk *t = compile(sexp, NULL, true);
	disassemble(t);

	return sexp;
}

