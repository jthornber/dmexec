#ifndef DMEXEC_VM_H
#define DMEXEC_VM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "array.h"
#include "list.h"
#include "mm.h"
#include "string_type.h"

/*----------------------------------------------------------------*/

#define MAX_STACK 8192

struct stack {
	unsigned nr_entries;
	value_t values[MAX_STACK];
};

struct code_position {
	struct list_head list;
	struct array *code;
	unsigned position;
};

void init_stack(struct stack *s);
void push(struct stack *s, value_t v);
value_t peek(struct stack *s);
value_t peekn(struct stack *s, unsigned n);
value_t pop(struct stack *s);

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
	struct stack stack;
	struct list_head call_stack;
};

struct vm {
	struct namespace *current_ns;
	struct continuation *k;
};

typedef void (*prim_fn)(struct vm *);

#define PUSH(v) push(&vm->k->stack, v)
#define POP() pop(&vm->k->stack)
#define PEEK() peek(&vm->k->stack)
#define PEEKN(n) peekn(&vm->k->stack, n)

void push_call(struct vm *vm, struct array *code);
void pop_call(struct vm *vm);

value_t mk_string(const char *b, const char *e);
void def_primitive(struct vm *vm, char *k, prim_fn fn);

void eval(struct vm *vm, struct array *code);
value_t mk_quot();
void print_value(FILE *stream, value_t v);
unsigned as_fixnum(value_t v);
value_t mk_fixnum(int i);
bool is_false(value_t v);

/*----------------------------------------------------------------*/

#endif
