#include "string_type.h"

#include "mm.h"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>

//----------------------------------------------------------------

void string_tmp(char *cstr, struct string *str)
{
	str->b = cstr;
	str->e = cstr + strlen(cstr);
}

struct string *string_clone(struct string *str)
{
	size_t len = str->e - str->b;
	struct string *copy = alloc(STRING, sizeof(*copy) + len);
	copy->b = (char *) (copy + 1);
	copy->e = copy->b + len;
	memcpy(copy->b, str->b, len);

	return copy;
}

struct string *string_clone_cstr(char *str)
{
	struct string tmp;
	string_tmp(str, &tmp);
	return string_clone(&tmp);
}

int string_cmp(struct string *lhs, struct string *rhs)
{
	size_t lhs_len = string_len(lhs);
	size_t rhs_len = string_len(rhs);
	int cmp = memcmp(lhs->b, rhs->b, MIN(lhs_len, rhs_len));

	if (cmp)
		return cmp;

	if (lhs_len < rhs_len)
		return -1;

	if (lhs_len > rhs_len)
		return 1;

	return 0;
}

int string_cmp_cstr(struct string *lhs, char *rhs)
{
	struct string tmp;
	string_tmp(rhs, &tmp);
	return string_cmp(lhs, &tmp);
}

//----------------------------------------------------------------
