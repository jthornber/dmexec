#ifndef DMEXEC_EVAL_H
#define DMEXEC_EVAL_H

#include "env.h"
#include "vm.h"

//----------------------------------------------------------------

Value eval(StaticEnv *r, VM *vm, Value sexp);

//----------------------------------------------------------------

#endif
