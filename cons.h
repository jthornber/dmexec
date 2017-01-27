#ifndef DMEXEC_CONS_H
#define DMEXEC_CONS_H

#include <stdbool.h>

#include "mm.h"

/*----------------------------------------------------------------*/

typedef struct {
  Value car;
  Value cdr;
} Cons;

extern Value Nil;

Value car(Value cell);
Value cdr(Value cell);

void set_car(Value cell, Value new_car);
void set_cdr(Value cell, Value new_cdr);
Cons *cons(Value car, Value cdr);

bool is_cons(Value v);

/*----------------------------------------------------------------*/

// Builds a list destructively.  Avoids the (reverse acc) idiom.
typedef struct {
	Cons *head, *tail;
} ListBuilder;

void lb_init(ListBuilder *lb);
void lb_append(ListBuilder *lb, Value v);
Value lb_get(ListBuilder *lb);

/*----------------------------------------------------------------*/

#endif
