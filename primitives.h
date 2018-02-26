#ifndef DMEXEC_PRIMITIVES_H
#define DMEXEC_PRIMITIVES_H

#include "env.h"

/*----------------------------------------------------------------*/

void add_primitive(StaticEnv *r, Value p);

void def_basic_primitives(StaticEnv *r);
void def_dm_primitives(StaticEnv *r);

/*----------------------------------------------------------------*/

#endif
