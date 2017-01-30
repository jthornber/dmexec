#include "vm.h"

//----------------------------------------------------------------
// Thunks

// FIXME: slow, implement a ropes library
#define DEFAULT_THUNK_SIZE 16

typedef struct {
	unsigned char *b, *e, *alloc_e;
} Thunk;

static Thunk t_new(size_t alloc)
{
	Thunk r;
	r.b = untyped_zalloc(alloc);
	r.e = r.b;
	r.alloc_e = r.b + DEFAULT_THUNK_SIZE;
	return r;
}

static size_t t_size(Thunk *t)
{
	return t->e - t->b;
}

static size_t t_allocated(Thunk *t)
{
	return t->alloc_e - t->b;
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

	t->e++ = byte;
}

static Thunk t_join(Thunk *lhs, Thunk *rhs)
{
	Thunk t = t_new(t_size(lhs) + t_size(rhs));
	memcpy(t->b, lhs->b, t_size(lhs));
	memcpy(t->b + t_size(lhs), rhs->b, t_size(rhs));
	t->e = t->alloc_e;
	return t;
}

// Used as a return value in unreachable code.
static Thunk bottom()
{
	return (Thunk) {};
}

//----------------------------------------------------------------
// Runtime environment

#define MAX_STACK 4096

typedef struct {
	unsigned current;
	Value sp[MAX_STACK];
} Stack;

static void push_v(Stack *s, Value v)
{
	assert(s->current < MAX_STACK);
	s->sp[s->current++] = v;
}

static Value pop_v(Stack *s)
{
	Value r;
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

//----------------------------------------------------------------

typedef struct _frame {
	struct _frame *next;
	unsigned nr;
	Value values[0];
} Frame;

static Frame *new_frame(unsigned count)
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

static void f_deep_set(Frame *f, unsigned depth, unsigned index)
{
	while (depth--) {
		f = f->next;
		assert(f);
	}

	f_set(f, index, v);
}

//----------------------------------------------------------------
// Closure
//
// FIXME: this is a first class value, so needs to move to mm.h to be tagged

typedef struct {
	// We only need code_e for the disassembly
	uint8_t *code_b, *code_e;
	Frame *env;
} Closure;

//----------------------------------------------------------------
// Virtual Machine

typedef struct {
	uint8_t *pc, *pc_end;

	Value val;
	Frame *env;
	Value fun;
	Value arg1;
	Value arg2;
	Stack stack;
} VM;

type enum {
	ALLOCATE_DOTTED_FRAME,
	ALLOCATE_FRAME,
	ARITY_EQ,
	ARITY_GEQ,
	CALL0,
	CHECKED_GLOBAL_REF,
	CONSTANT,
	CREATE_CLOSURE,
	DEEP_ARGUMENT_REF,
	EXTEND_ENV,
	FINISH
	FUNCTION_INVOKE,
	GLOBAL_REF,
	GOTO,
	INVOKE1,
	INVOKE2,
	INVOKE3,
	JUMP_FALSE,
	PACK_FRAME,
	POP_ARG1,
	POP_ARG2,
	POP_CONS_FRAME,
	POP_FRAME,
	POP_FUNCTION,
	PREDEFINED,
	PRESERVE_ENV,
	PUSH_VALUE,
	RESTORE_ENV,
	RETURN,
	SET_DEEP_ARGUMENT_REF,
	SET_GLOBAL,
	SET_SHALLOW_ARGUMENT,
	SHALLOW_ARGUMENT_REF,
	UNLINK_ENV,
} ByteOp;

static inline uint8_t shift8(VM *vm)
{
	if (vm->pc >= vm->pc_end)
		error("ran off the end of the bytecode");

	return *pc++;
}

// Returns false if program exits
static inline bool step(VM *vm)
{
	ByteOp op = (ByteOp) shift8(vm);
	uint8_t rand1, rand2;
	uin16_t rand_wide;

	switch (op) {
	case ALLOCATE_DOTTED_FRAME:
		
		break;

	case ALLOCATE_FRAME:
		vm->val = new_frame(shift8(vm));
		break;

	case ARITY_EQ:
		break;

	case ARITY_GEQ:
		break;

	case CALL0:
		break;

	case CHECKED_GLOBAL_REF:
		break;

	case CONSTANT:
		break;

	case CREATE_CLOSURE:
		break;

	case DEEP_ARGUMENT_REF:
		assert(vm->env);
		f_deep_get(vm->env, shift8(vm), shift8(vm));
		break;

	case EXTEND_ENV:
		break;

	case FINISH:
		return false;
		break;

	case FUNCTION_INVOKE:
		break;

	case GLOBAL_REF:
		break;

	case GOTO:
		vm->pc += shift16(vm);
		break;

	case INVOKE1:
		break;

	case INVOKE2:
		break;

	case INVOKE3:
		break;

	case JUMP_FALSE:
		rand16 = shift16(vm);
		if (is_nil(vm->val))
			vm->pc += rand16;
		break;

	case PACK_FRAME:
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

	case PRESERVE_ENV:
		push_p(&vm->stack, vm->env);
		break;

	case PUSH_VALUE:
		push_v(&vm->stack, vm->val);
		break;

	case RESTORE_ENV:
		vm->env = pop_p(&vm->stack);
		break;

	case RETURN:
		// FIXME: pop pc from stack? env?
		break;

	case SET_DEEP_ARGUMENT_REF:
		// FIXME: variant of this op that packs i, j into a single byte?
		assert(vm->env);
		f_deep_set(vm->env, shift8(vm), shift8(vm), vm->val);
		break;

	case SET_GLOBAL:
		break;

	case SET_SHALLOW_ARGUMENT:
		assert(vm->env);
		f_set(vm->env, shift8(vm), vm->val);
		break;

	case SHALLOW_ARGUMENT_REF:
		assert(vm->env);
		vm->val = f_get(vm->env, shift8(vm));
		break;

	case UNLINK_ENV:
		assert(vm->env);
		vm->env = vm->env->next;
		break;
	}

	return true;
}

static void run(VM *vm)
{
	while (step(vm))
		;
}

//----------------------------------------------------------------
// Intermediate instructions

static Thunk i_shallow_argument_ref(unsigned i)
{

}

static Thunk i_shallow_argument_set(unsigned i, Thunk t)
{

}

static Thunk i_deep_argument_ref(unsigned i, unsigned j)
{

}

static Thunk i_deep_argument_set(unsigned i, unsigned j, Thunk t)
{

}

static Thunk i_checked_global_ref(unsigned i)
{

}

static Thunk i_global_set(unsigned i, Thunk t)
{

}

static Thunk i_predefined(unsigned i)
{

}

static Thunk i_constant(Value v)
{

}

static Thunk i_alternative(Thunk t1, Thunk t2, Thunk t3)
{

}

static Thunk i_sequence(Thunk t1, Thunk t2)
{

}

static Thunk i_fix_closure(Thunk t, unsigned arity)
{

}

static Thunk i_nary_closure(Thunk t, unsigned arity)
{

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
}

//----------------------------------------------------------------
// Compilation

static Thunk c_quotation(Value v, StaticEnv *r, bool tail)
{
	return i_constant(v);
}

static Thunk c_reference(Value n, StaticEnv *r, bool tail)
{
	Kind k = compute_kind(r, as_ref(n));
	switch (k->t) {
	case KindLocal:
		if (k->i == 0)
			return i_shallow_argument_ref(k->j);
		else
			return i_deep_argument_ref(k->i, k->j);

	case KindGlobal:
		return i_checked_global_ref(k->i);

	case KindPredefined:
		return i_predefined(k->i);
	}
}

static Thunk c_assignment(Value n, Value e, SaticEnv *r, bool tail)
{
	Thunk t = compile(e, r, false);
	Kind k = compute_kind(r, n);

	switch (k->t) {
	case KindLocal:
		if (k->i == 0)
			return i_shallow_argument_set(k->j, t);
		else
			return i_deep_argument_set(k->i, k->j, t);

	case KindGlobal:
		return i_global_set(k->i, t);

	case KindPredefined:
		error("Predefined variables are immutable.");
		return bottom();
	}
}

static Thunk c_alternative(Value e1, Value e2, Value e3, StaticEnv *r, bool tail)
{
	Thunk t1 = compile(e1, r, false);
	Thunk t2 = compile(e2, r, tail);
	Thunk t3 = compile(e3, r, tail);

	return i_alternative(t1, t2, t3);
}

static Thunk c_sequence(Value es, StaticEnv *r, bool tail);
static Thunk c_multiple_seq(Value e, Value es, StaticEnv *r, bool tail)
{
	Thunk t1 = compile(e);
	Thunk t2 = c_sequence(es, r, tail);

	return i_sequence(t1, t2);
}

static Thunk c_sequence(Value es, StaticEnv *r, bool tail)
{
	if (is_cons(es)) {
		if (is_cons(cdr(es)))
			return c_multiple_seq(car(e), cdr(e), r, tail);
		else
			return compile(car(e), r, tail);
	}

	error("bad sequence");
	return bottom();
}

static Thunk c_fix_abstraction(Value ns, Value es, StaticEnv *r, bool tail)
{
	unsigned arity = len(ns);
	StaticEnv *r2 = r_extend(r, ns);
	Thunk t = c_sequence(es, r2, true);

	return i_fix_closure(t, arity);
}

static Thunk c_dotted_abstraction(Value ns, Value es, StaticEnv *r, bool tail)
{
	unsigned arity = len(ns) - 1; // extra param is already on here
	StaticEnv *r2 = r_extend(r, ns);
	Thunk t = c_sequence(es, r2, tail);

	return i_nary_closure(t, arity);
}

static Thunk c_abstraction(Value ns, Value es, StaticEnv *r, bool tail)
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
		return c_dotted_abstraction(lb_get(&lb), ns, es, r, tail);
	}
}

static Thunk c_application(Value e, Value es, StaticEnv *r, bool tail)
{
	if (is_symbol(e)) {
		Kind k = compute_kind(r, as_ref(e));
		switch (k->t) {
		case KindLocal:
		case KindGlobal:
			return c_regular_application(e, es, r tail);
			break;

		case KindPrimitive:
			// FIXME: check hte primitive is a function
			return c_primitive_application(e, es, r, tail);
		}

	} else if (is_cons(e) && is_symbol(car(e)) && as_ref(car(e)) == LAMBDA_SYMBOL) {
		
	}
}

// We assume that symbols can use ptr comparison.  Probably will always be
// true.
Thunk compile(Value e, StaticEnv *r, bool tail)
{
	if (is_cons(e)) {
		if (is_symbol(car(e))) {
			Symbol *s = as_ref(car(e));
			if (s == QUOTE_SYMBOL)
				return c_quotation(cadr(e), r, tail);

			else if (s == LAMBDA_SYMBOL)
				return c_abstraction(cadr(e), caddr(e), r, tail);

			else if (s == IF_SYMBOL)
				return c_alternative(cadr(e), caddr(e), cadddr(e),
					      	     r, tail);

			else if (s == BEGIN_SYMBOL)
				return c_sequence(cdr(e), r, tail);

			else if (s == SET_SYMBOL)
				return c_assignment(cadr(e), caddr(e), r, tail);

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

	return sexp;
}

