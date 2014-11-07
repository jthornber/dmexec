#ifndef DMEXEC_MM_H
#define DMEXEC_MM_H

#include <stdlib.h>

//----------------------------------------------------------------

enum object_type {
	STRING,
	BYTE_ARRAY,
	TUPLE,
	SYMBOL,
	WORD,
	QUOT,
	ARRAY,
	DEF,
	CODE_POSITION,
	CONTINUATION,
	FIXNUM			/* these are always tagged immediate values */
};

void *alloc(enum object_type type, size_t s);

//----------------------------------------------------------------

#endif
