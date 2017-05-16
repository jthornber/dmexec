#include "string_type.h"

#include "mm.h"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>

//----------------------------------------------------------------

void string_tmp(const char *cstr, String *str)
{
	str->b = cstr;
	str->e = cstr + strlen(cstr);
}

static size_t p4(size_t len)
{
	return 4 - (len & 3);
}

// We pad the string up to a 4-byte boundary so we can calculate the hash in
// word sized chunks.
static String *string_clone_(ObjectType t, String *str)
{
	size_t len = str->e - str->b;
	size_t padding = p4(len);
	String *copy = mm_alloc(t, sizeof(*copy) + len + padding);
	copy->b = (const char *) (copy + 1);
	copy->e = copy->b + len;
	memcpy((char *) copy->b, str->b, len);
	memset((char *) copy->e, 0, padding);

	return copy;
}

String *mk_string(ObjectType t, const char *b, const char *e)
{
	String tmp;
	tmp.b = b;
	tmp.e = e;
	return string_clone_(t, &tmp);
}

String *mk_string_from_cstr(ObjectType t, const char *str)
{
	String tmp;
	string_tmp(str, &tmp);
	return string_clone_(t, &tmp);
}

int string_cmp(String *lhs, String *rhs)
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

int string_cmp_cstr(String *lhs, const char *rhs)
{
	String tmp;
	string_tmp(rhs, &tmp);
	return string_cmp(lhs, &tmp);
}

// FIXME: it's really important this hash is good and runs quickly:
// - statically generate the sequence a
// - consume the string a word at a time (pad with zeroes on allocation)
// - draw some graphs with some representative datasets
uint32_t string_hash(String *s)
{
	const char *ptr = s->b;
	uint32_t h = 0, a = 31415, b = 27183;

	while (ptr != s->e) {
		h = a * h + *ptr;

		a = (a * b) % (UINT32_MAX - 1);
		ptr++;
	}

	return h;
}

//----------------------------------------------------------------
