#ifndef DMEXEC_SYMBOL_H
#define DMEXEC_SYMBOL_H

#include "mm.h"

#include <stdbool.h>

//----------------------------------------------------------------

static inline bool is_symbol(Value v)
{
	return get_type(v) == SYMBOL;
}

//----------------------------------------------------------------

#endif
