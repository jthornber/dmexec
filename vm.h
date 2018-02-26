#ifndef DMEXEC_VM_H
#define DMEXEC_VM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>

#include "vector.h"
#include "cons.h"
#include "env.h"
#include "error.h"
#include "list.h"
#include "mm.h"
#include "string_type.h"

/*----------------------------------------------------------------*/

typedef struct vm {
	Thunk *code;

	Value val;
	Frame *env;
	Value fun;
	Value arg1;
	Value arg2;
	Stack stack;
	Vector *constants;
	Vector *globals;
	Vector *global_syms; // for debug
} VM;

// Rather than constantly pass the single vm instance around I'm going to
// use a global.  Supporting error recovery meant the vm would have to be
// passed into practically every function eg. array_pop().  This global is
// set during evaluation, it is _not_ set when defining primitives.
extern VM *global_vm;


Value mk_quot(void);
void print_value(FILE *stream, Value v);

Value mk_fixnum(int i);
bool is_false(Value v);

Value mk_word(String *str);
Value mk_word_cstr(char *str);

void print_string(FILE *stream, String *str);

typedef struct {
	String *in;
	Token tok;
} TokenStream;

void stream_init(String *in, TokenStream *ts);
bool read_sexp(TokenStream *ts, Value *result);
void print(FILE *stream, Value v);

/*----------------------------------------------------------------*/

#endif
