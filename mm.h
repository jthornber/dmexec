#ifndef DMEXEC_MM_H
#define DMEXEC_MM_H

#include <stdlib.h>

//----------------------------------------------------------------

enum object_type {
	NAMESPACE,
	NAMESPACE_ENTRY,
	PRIMITIVE,
	STRING,
	BYTE_ARRAY,
	TUPLE,
	SYMBOL,
	WORD,
	QUOT,
	ARRAY,
	CODE_POSITION,
	CONTINUATION,
	FIXNUM			/* these are always tagged immediate values */
};

void *alloc(enum object_type type, size_t s);

enum tag {
	TAG_REF = 0,
	TAG_FIXNUM = 1,
	TAG_FALSE
};

typedef union value {
	void *ptr;
	int32_t i;
} value_t;

enum object_type get_type(value_t v);
value_t mk_ref(void *ptr);
void *as_ref(value_t v);

//----------------------------------------------------------------

#endif
