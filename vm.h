#ifndef DMEXEC_VM_H
#define DMEXEC_VM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include "array.h"
#include "cons.h"
#include "error.h"
#include "list.h"
#include "mm.h"
#include "string_type.h"

/*----------------------------------------------------------------*/

typedef struct {
	struct list_head list;
	Array *code;
	unsigned position;
} CodePosition;

typedef enum {
	TOK_FIXNUM,
	TOK_STRING,
	TOK_SYM,
	TOK_OPEN,
	TOK_CLOSE,
	TOK_DOT,
	TOK_QUOTE,
	TOK_EOF
} TokenType;

typedef struct {
	TokenType type;
	String str;
	int fixnum;
} Token;

typedef struct {
	Array *data_stack;
	Array *call_stack;
	Array *catch_stack;
} Continuation;

typedef struct {
	struct namespace *current_ns;
	Continuation *k;

	jmp_buf eval_loop;
	bool handling_error;	// FIXME: is this needed?
} VM;

// Rather than constantly pass the single vm instance around I'm going to
// use a global.  Supporting error recovery meant the vm would have to be
// passed into practically every function eg. array_pop().  This global is
// set during evaluation, it is _not_ set when defining primitives.
extern VM *global_vm;

typedef void (*PrimFn)(void);

static inline void PUSH(Value v) {
	global_vm->k->data_stack = array_push(global_vm->k->data_stack, v);
}

static inline Value POP() {
	return array_pop(global_vm->k->data_stack);
}

static inline Value POP_TYPE(ObjectType t) {
	Value v = POP();
	if (get_type(v) != t)
		error("type error, expected %d", t);
	return v;
}

static inline Value PEEK() {
	return array_peek(global_vm->k->data_stack);
}

static inline Value PEEKN(unsigned n) {
	return array_peekn(global_vm->k->data_stack, n);
}

void push_call(Array *code);
void pop_call(void);

// FIXME: why aren't mk_{string, fixnum ...} in mm.h?
Value mk_string(const char *b, const char *e);
void def_primitive(VM *vm, char *k, PrimFn fn);

void eval(VM *vm, Array *code);
Value mk_quot(void);
void print_value(FILE *stream, Value v);

Value mk_fixnum(int i);
bool is_false(Value v);

Value mk_symbol(String *str);
Value mk_word(String *str);
Value mk_word_cstr(char *str);

void print_string(FILE *stream, String *str);

Continuation *cc(VM *vm);

void inc_pc(void);

/*----------------------------------------------------------------*/

#endif
