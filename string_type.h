#ifndef DMEXEC_STRING_H
#define DMEXEC_STRING_H

//----------------------------------------------------------------

struct string {
	char *b;
	char *e;
};

inline unsigned string_len(struct string *str)
{
	return str->e - str->b;
}

struct string *string_clone(struct string *str);
struct string *string_clone_cstr(char *str);

int string_cmp(struct string *lhs, struct string *rhs);
int string_cmp_cstr(struct string *lhs, char *rhs);

//----------------------------------------------------------------

#endif
