#include "namespace.h"

#include "list.h"

//----------------------------------------------------------------

struct namespace *namespace_create(struct namespace *parent)
{
	struct namespace *ns = alloc(NAMESPACE, sizeof(*ns));
	ns->parent = parent;
	ns->root = NULL;
	return ns;
}

static inline struct namespace_entry *to_entry(struct node *n)
{
	return container_of(n, struct namespace_entry, node);
}

static bool lookup_shallow(struct namespace *ns, String *k, Value *result)
{
	int c;
	struct node *n = ns->root;
	struct namespace_entry *e;

	while (n) {
		e = to_entry(n);
		c = string_cmp(e->key, k);

		if (c < 0)
			n = n->l;
		else if (c > 0)
			n = n->r;
		else {
			*result = e->value;
			return true;
		}
	}

	return false;
}

bool namespace_lookup(struct namespace *ns, String *k, Value *result)
{
	bool r;

retry:
	r = lookup_shallow(ns, k, result);

	if (!r && ns->parent) {
		ns = ns->parent;
		goto retry;
	}

	return r;
}

void namespace_insert(struct namespace *ns, String *k, Value v)
{
	int c;
	struct node **current = &ns->root;
	struct namespace_entry *e;

	while (*current) {
		e = to_entry(*current);
		c = string_cmp(e->key, k);

		if (c < 0)
			current = &((*current)->l);
		else if (c > 0)
			current = &((*current)->r);
		else {
			e->value = v;
			return;
		}
	}

	e = alloc(NAMESPACE_ENTRY, sizeof(*e));
	e->node.l = e->node.r = NULL;
	e->key = string_clone(k);
	e->value = v;

	*current = &e->node;
}

static void visit_node(struct node *n,
		       void (*callback)(void *, String *, Value),
		       void *ctx)
{
	struct namespace_entry *e;

	if (n->l)
		visit_node(n->l, callback, ctx);

	e = to_entry(n);
	callback(ctx, e->key, e->value);

	if (n->r)
		visit_node(n->r, callback, ctx);
}

void namespace_visit(struct namespace *ns,
		     void (*callback)(void *, String *, Value),
		     void *ctx)
{
	if (ns->root)
		visit_node(ns->root, callback, ctx);
}

//----------------------------------------------------------------
