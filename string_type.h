#ifndef DMEXEC_STRING_H
#define DMEXEC_STRING_H

//----------------------------------------------------------------

typedef struct {
	char *b;
	char *e;
} String;

void string_tmp(char *cstr, String *str);

static inline unsigned string_len(String *str)
{
	return str->e - str->b;
}

String *string_clone(String *str);
String *string_clone_cstr(char *str);

int string_cmp(String *lhs, String *rhs);
int string_cmp_cstr(String *lhs, char *rhs);

//----------------------------------------------------------------

#endif
