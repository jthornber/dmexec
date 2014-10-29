#ifndef DMEXEC_INTERPRETER_H
#define DMEXEC_INTERPRETER_H

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
	struct stack stack;
	struct token tok;
	struct list_head definitions;
};

typedef void (*prim_fn)(struct interpreter *);

#define PUSH(v) push(&terp->stack, v)
#define POP() pop(&terp->stack)
#define PEEK() peek(&terp->stack)
#define PEEKN(n) peekn(&terp->stack, n)

value_t mk_string(const char *b, const char *e);
void add_primitive(struct interpreter *terp, const char *name, prim_fn fn);

/*----------------------------------------------------------------*/

#endif
