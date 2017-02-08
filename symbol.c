#include "vm.h"

static Symbol **lookup(Symbol **r, String *str)
{
	while (*r) {
		int cmp = string_cmp(str, (*r)->str);
		if (cmp < 0)
			r = &(*r)->left;
		else if (cmp > 0)
			r = &(*r)->right;
		else
			break;
	}

	return r;
}

Value mk_symbol(Symbol **root, String *str)
{
	Symbol **r = lookup(root, str);
	if (!(*r)) {
		Symbol *new = zalloc(SYMBOL, sizeof(*new));
		new->left = new->right = NULL;
		new->str = string_clone(str);
		*r = new;
	}

	return mk_ref(*r);
}
