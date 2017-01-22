#ifndef DMEXEC_CONS_H
#define DMEXEC_CONS_H

#include "mm.h"

/*----------------------------------------------------------------*/

typedef struct {
  Value car;
  Value cdr;
} Cons;

extern Value Nil;

Value car(Value cell);
Value cdr(Value cell);
Value cons(Value car, Value cdr);

/*----------------------------------------------------------------*/

#endif
