#ifndef DMEXEC_VM_H
#define DMEXEC_VM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
	value_t stack;
	struct list_head call_stack;
};

struct vm {
	struct namespace *current_ns;
	struct continuation *k;
};

typedef void (*prim_fn)(struct vm *);

#define PUSH(v) array_push(as_ref(vm->k->stack), v)
#define POP() array_pop(as_ref(vm->k->stack))
#define PEEK() array_peek(as_ref(vm->k->stack))
#define PEEKN(n) array_peekn(as_ref(vm->k->stack), n)

void push_call(struct vm *vm, struct array *code);
void pop_call(struct vm *vm);

value_t mk_string(const char *b, const char *e);
void def_primitive(struct vm *vm, char *k, prim_fn fn);

void eval(struct vm *vm, struct array *code);
value_t mk_quot();
void print_value(FILE *stream, value_t v);

value_t mk_fixnum(int i);
bool is_false(value_t v);

value_t mk_symbol(struct string *str);
value_t mk_word(struct string *str);
value_t mk_word_cstr(char *str);

/*----------------------------------------------------------------*/

#endif
