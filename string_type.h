#ifndef DMEXEC_STRING_H
#define DMEXEC_STRING_H

#include "types.h"

//----------------------------------------------------------------

void string_tmp(const char *cstr, String *str);

static inline unsigned string_len(String *str)
{
	return str->e - str->b;
}

String *mk_string(ObjectType t, const char *b, const char *e);
String *mk_string_from_cstr(ObjectType t, const char *str);

int string_cmp(String *lhs, String *rhs);
int string_cmp_cstr(String *lhs, const char *rhs);

uint32_t string_hash(String *s);

//----------------------------------------------------------------

#endif
