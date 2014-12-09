#ifndef DMEXEC_VM_H
#define DMEXEC_VM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include "array.h"
#include "error.h"
#include "list.h"
#include "mm.h"
#include "string_type.h"

/*----------------------------------------------------------------*/

struct code_position {
	struct list_head list;
	struct array *code;
	unsigned position;
};

enum token_type {
	TOK_FIXNUM,
	TOK_STRING,
	TOK_WORD,
	TOK_COLON,
	TOK_SEMI_COLON,
	TOK_OPEN_BRACE,
	TOK_CLOSE_BRACE,
	TOK_OPEN_SQUARE,
	TOK_CLOSE_SQUARE
};

struct token {
	enum token_type type;
	struct string str;
	int fixnum;
};

struct continuation {
	struct array *data_stack;
	struct array *call_stack;
	struct array *catch_stack;
};

struct vm {
	struct namespace *current_ns;
	struct continuation *k;

	jmp_buf eval_loop;
	bool handling_error;	// FIXME: is this needed?
};

// Rather than constantly pass the single vm instance around I'm going to
// use a global.  Supporting error recovery meant the vm would have to be
// passed into practically every function eg. array_pop().  This global is
// set during evaluation, it is _not_ set when defining primitives.
extern struct vm *global_vm;

typedef void (*prim_fn)(void);

static inline void PUSH(value_t v) {
	global_vm->k->data_stack = array_push(global_vm->k->data_stack, v);
}

static inline value_t POP() {
	return array_pop(global_vm->k->data_stack);
}

static inline value_t POP_TYPE(enum object_type t) {
	value_t v = POP();
	if (get_type(v) != t)
		error("type error, expected %d", t);
	return v;
}

static inline value_t PEEK() {
	return array_peek(global_vm->k->data_stack);
}

static inline value_t PEEKN(unsigned n) {
	return array_peekn(global_vm->k->data_stack, n);
}

void push_call(struct array *code);
void pop_call(void);

value_t mk_string(const char *b, const char *e);
void def_primitive(struct vm *vm, char *k, prim_fn fn);

void eval(struct vm *vm, struct array *code);
value_t mk_quot(void);
void print_value(FILE *stream, value_t v);

value_t mk_fixnum(int i);
bool is_false(value_t v);

value_t mk_symbol(struct string *str);
value_t mk_word(struct string *str);
value_t mk_word_cstr(char *str);

void throw(void);
void print_string(FILE *stream, struct string *str);

struct continuation *cc(struct vm *vm);

void inc_pc(void);

/*----------------------------------------------------------------*/

#endif
