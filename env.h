#ifndef DMEXEC_ENV_H
#define DMEXEC_ENV_H

#include "vm.h"

//----------------------------------------------------------------
// We classify variables into three kinds:
//
// - Local i, j; where i is the frame depth, and j is the value index.
// - Global i; where i is an index into a global array.
// - Predefined i; where i is an index into an array of predefined (constant)
//   values.

typedef enum {
	KindLocal,
	KindGlobal,
	KindPredefined
} KindType;

typedef struct {
	KindType t;
	unsigned i, j;

} Kind;

//----------------------------------------------------------------

typedef struct __static_frame {
	struct __static_frame *prev;
	unsigned nr_syms;
	Symbol *syms[];
} StaticFrame;

#define MAX_CONSTANTS 1024
#define MAX_PRIMS 1024

typedef struct {
	// FIXME: I'm not sure if the constants should be in here, or in the
	// vm.
	unsigned nr_constants;
	Value constants[MAX_CONSTANTS];

	StaticFrame *frames;
} StaticEnv;

StaticEnv *r_alloc();

void r_push_names(StaticEnv *r, Value ns);
void r_pop_names(StaticEnv *r);
Kind compute_kind(StaticEnv *r, Symbol *sym);

unsigned r_add_constant(StaticEnv *r, Value v);
void r_add_primitive(StaticEnv *r, Value v);

void r_add_prim0(StaticEnv *r, const char *name, Prim0 p);
void r_add_prim1(StaticEnv *r, const char *name, Prim1 p);
void r_add_prim2(StaticEnv *r, const char *name, Prim2 p);

//----------------------------------------------------------------

#endif
