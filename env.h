#ifndef DMEXEC_ENV_H
#define DMEXEC_ENV_H

#include "vm.h"

//----------------------------------------------------------------
// We classify variables into three kinds:
//
// - Local i, j;     i is the frame depth, and j is the value index.
// - Global i;       i is an index into a global array.
// - Constant i;     i is an index into the constants array.

typedef enum {
	KindLocal,
	KindGlobal,
	KindConstant
} KindType;

typedef struct {
	KindType t;
	unsigned i, j;

} Kind;

//----------------------------------------------------------------

typedef struct {
	// int -> value, constants includes primitives
	Vector *constants;

	// name -> index
	HashTable *primitives_r;
	HashTable *globals_r;

	// Vector of frames containing names, the compiled code will create the
	// runtime frames.
	Vector *frames_r;

} StaticEnv;

StaticEnv *r_alloc();

// ns is a list of symbols (it gets converted to a vector)
void r_push_frame(StaticEnv *r, Value ns);
void r_pop_frame(StaticEnv *r);

unsigned r_add_constant(StaticEnv *r, Value v);
void r_add_prim(StaticEnv *r, Value p);

Kind compute_kind(StaticEnv *r, String *sym);

//----------------------------------------------------------------

#endif
