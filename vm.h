#ifndef DMEXEC_INTERPRETER_H
#define DMEXEC_INTERPRETER_H

#include <stdbool.h>
#include <stdint.h>

#include "list.h"

/*----------------------------------------------------------------*/

enum tag {
	TAG_REF = 0,
	TAG_FIXNUM = 1,
	TAG_FALSE
};

typedef union value {
	void *ptr;
	int32_t i;
} value_t;

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
	const char *begin;
	const char *end;

	int fixnum;
};

struct interpreter {
	struct list_head prims;
	struct token tok;
	struct list_head definitions;

	struct stack stack;
	struct list_head call_stack;
};

typedef void (*prim_fn)(struct interpreter *);

#define PUSH(v) push(&terp->stack, v)
#define POP() pop(&terp->stack)
#define PEEK() peek(&terp->stack)
#define PEEKN(n) peekn(&terp->stack, n)

void push_call(struct interpreter *terp, struct array *code);
void pop_call(struct interpreter *terp);

value_t mk_string(const char *b, const char *e);
void add_primitive(struct interpreter *terp, const char *name, prim_fn fn);

enum object_type {
	STRING,
	BYTE_ARRAY,
	TUPLE,
	WORD,
	QUOT,
	ARRAY,
	DEF,
	CODE_POSITION,
	FIXNUM			/* these are always tagged immediate values */
};

enum object_type get_type(value_t v);

#define MAX_ARRAY_SIZE 32

// FIXME: add dynamic resizing
struct array {
	unsigned nr_elts;
	value_t elts[MAX_ARRAY_SIZE]; /* yee haa! */
};

void eval(struct interpreter *terp, struct array *code);
void *as_ref(value_t v);
value_t mk_quot();
value_t mk_array();
void append_array(value_t av, value_t v);
void print_value(FILE *stream, value_t v);
unsigned as_fixnum(value_t v);
value_t mk_fixnum(int i);
bool is_false(value_t v);

/*----------------------------------------------------------------*/

#endif
