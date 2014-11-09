#include "namespace.h"

#include "list.h"

//----------------------------------------------------------------

struct namespace *namespace_create()
{
	struct namespace *ns = alloc(NAMESPACE, sizeof(*ns));
	ns->root = NULL;
	return ns;
}

static inline struct namespace_entry *to_entry(struct node *n)
{
	return container_of(n, struct namespace_entry, node);
}

bool namespace_lookup(struct namespace *ns, struct string *k, value_t *result)
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

void namespace_insert(struct namespace *ns, struct string *k, value_t v)
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

//----------------------------------------------------------------