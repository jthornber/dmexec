#ifndef DMEXEC_TYPES_H
#define DMEXEC_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "list.h"

//----------------------------------------------------------------

typedef union value {
	void *ptr;
	int32_t i;
} Value;

typedef struct {
	struct list_head list;
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

// FIXME: Use another of Phil Bagwell's immutable structures
typedef struct {
       const char *b;
       const char *e;
} String;

typedef struct {
	TokenType type;
	String str;
	int fixnum;
} Token;

#define MAX_STACK 4096

typedef struct {
	unsigned current;
	Value sp[MAX_STACK];
} Stack;

typedef struct _frame {
	struct _frame *next;
	unsigned nr;
	Value values[0];
} Frame;

typedef struct {
	unsigned char *b, *e, *alloc_e;
} Thunk;

// FIXME: replace with a hash
typedef struct _symbol {
	struct _symbol *left, *right;
	// FIXME: store globals in separate hash table
	bool global_set;
	Value global;
	String *str;
} Symbol;

typedef struct {
  Value car;
  Value cdr;
} Cons;

// FIXME: rename RADIX_*
#define RADIX_SHIFT 4u
#define ENTRIES_PER_VBLOCK (1 << RADIX_SHIFT)
#define RADIX_MASK (ENTRIES_PER_VBLOCK - 1u)
typedef Value *VBlock;

typedef struct __vector {
	unsigned size;
	unsigned cursor_index;

	VBlock root;
	VBlock cursor;
	bool cursor_dirty:1;
	bool transient:1;
} Vector;

//----------------------------------------------------------------

#endif
