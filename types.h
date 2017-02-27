#ifndef DMEXEC_TYPES_H
#define DMEXEC_TYPES_H

#include <stdbool.h>

//----------------------------------------------------------------

typedef Value *VBlock;

typedef struct __vector {
	unsigned size;
	unsigned cursor_index;

	VBlock root;
	VBlock cursor;
	bool cursor_dirty:1;
	bool transient:1;
} Vector;

//----------------------------------------------------------------

#endif
