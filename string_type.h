#ifndef DMEXEC_STRING_H
#define DMEXEC_STRING_H

//----------------------------------------------------------------

typedef struct {
	const char *b;
	const char *e;
} String;

void string_tmp(const char *cstr, String *str);

static inline unsigned string_len(String *str)
{
	return str->e - str->b;
}

String *string_clone(String *str);
String *string_clone_cstr(const char *str);

int string_cmp(String *lhs, String *rhs);
int string_cmp_cstr(String *lhs, const char *rhs);

//----------------------------------------------------------------

#endif
