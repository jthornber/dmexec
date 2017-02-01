#ifndef DMEXEC_PRIMITIVES_H
#define DMEXEC_PRIMITIVES_H

#include "vm.h"

/*----------------------------------------------------------------*/

typedef Value (*Primitive0)();
typedef Value (*Primitive1)(Value);
typedef Value (*Primitive2)(Value, Value);
typedef Value (*Primitive3)(Value, Value, Value);

void add_primitive0(VM *vm, Primitive0 fn);
void add_primitive1(VM *vm, Primitive1 fn);
void add_primitive2(VM *vm, Primitive2 fn);

void def_basic_primitives(VM *vm);
void def_dm_primitives(VM *vm);

/*----------------------------------------------------------------*/

#endif
