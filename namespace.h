#ifndef DMEXEC_NAMESPACE_H
#define DMEXEC_NAMESPACE_H

#include "mm.h"
#include "string_type.h"
#include "tree.h"

#include <stdbool.h>

//----------------------------------------------------------------

struct namespace_entry {
	struct node node;
	String *key;
	Value value;
};

struct namespace {
	struct namespace *parent;
	struct node *root;
};

struct namespace *namespace_create(struct namespace *parent);
bool namespace_lookup(struct namespace *ns, String *k, Value *result);
void namespace_insert(struct namespace *ns, String *k, Value v);
void namespace_visit(struct namespace *ns,
		     void (*callback)(void *, String *, Value),
		     void * ctxt);

//----------------------------------------------------------------

#endif
