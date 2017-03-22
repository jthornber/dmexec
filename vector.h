#ifndef DMEXEC_VECTOR_H
#define DMEXEC_VECTOR_H

#include "mm.h"
#include "types.h"

//----------------------------------------------------------------


Vector *v_empty();
Value list_to_vector(Value xs);

unsigned v_size(Vector *v);
Value v_ref(Vector *v, unsigned i);

// Immutable operations are the default
Vector *v_set(Vector *v, unsigned i, Value val);
Vector *v_resize(Vector *v, unsigned new_size, Value init);
Vector *v_push(Vector *v, Value val);
Vector *v_pop(Vector *v);

/*
 * Setting transient mode speeds things up by dropping immutability *within*
 * the transient period.  Call the normal vector methods as normal, but the
 * return value will be the same as the arg.
 */
Vector *v_transient_begin(Vector *v);
void v_transient_end(Vector *v);

//----------------------------------------------------------------

#endif
