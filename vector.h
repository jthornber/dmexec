#ifndef DMEXEC_VECTOR_H
#define DMEXEC_VECTOR_H

#include "mm.h"

//----------------------------------------------------------------

struct __vector;
typedef struct __vector Vector;

Vector *v_alloc();

unsigned v_size(Vector *v);
Value v_ref(Vector *v, unsigned i);

// Immutable operations are the default
Vector *v_set(Vector *v, unsigned i, Value val);
Vector *v_resize(Vector *v, unsigned new_size, Value init);
Vector *v_append(Vector *v, Value val);

//----------------------------------------------------------------

#endif
