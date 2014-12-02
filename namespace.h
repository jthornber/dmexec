#ifndef DMEXEC_NAMESPACE_H
#define DMEXEC_NAMESPACE_H

#include "mm.h"
#include "string_type.h"
#include "tree.h"

#include <stdbool.h>

//----------------------------------------------------------------

struct namespace_entry {
	struct node node;
	struct string *key;
	value_t value;
};

struct namespace {
	struct namespace *parent;
	struct node *root;
};

struct namespace *namespace_create(struct namespace *parent);
bool namespace_lookup(struct namespace *ns, struct string *k, value_t *result);
void namespace_insert(struct namespace *ns, struct string *k, value_t v);

//----------------------------------------------------------------

#endif
