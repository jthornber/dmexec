#ifndef DMEXEC_ARRAY_H
#define DMEXEC_ARRAY_H

#include "mm.h"

//----------------------------------------------------------------

// FIXME: add dynamic resizing
#define MAX_ARRAY_SIZE 32

struct array {
	unsigned nr_elts;
	value_t elts[MAX_ARRAY_SIZE]; /* yee haa! */
};

value_t mk_array();
void append_array(value_t av, value_t v);

//----------------------------------------------------------------

#endif
