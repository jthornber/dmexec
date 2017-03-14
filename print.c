#include "vm.h"

//----------------------------------------------------------------
// Print

static void print_string_unquoted(FILE *stream, String *str)
{
	const char *ptr;
	for (ptr = str->b; ptr != str->e; ptr++)
		fputc(*ptr, stream);
}

void print_string(FILE *stream, String *str)
{
	fputc('\"', stream);
	print_string_unquoted(stream, str);
	fputc('\"', stream);
}

static void print_symbol(FILE *stream, Symbol *sym)
{
	print_string_unquoted(stream, sym->str);
}

static void print_list(FILE *stream, Value v);

void print(FILE *stream, Value v)
{
	switch (get_tag(v)) {
	case TAG_FIXNUM:
		fprintf(stream, "%d", as_fixnum(v));
		break;

	case TAG_REF:
		switch (get_type(v)) {
		case PRIMITIVE:
			fprintf(stream, "~primitive~");
			break;

		case STRING:
			print_string(stream, v.ptr);
			break;

		case CONS:
			print_list(stream, v);
			break;

		case SYMBOL:
			print_symbol(stream, v.ptr);
			break;

		case FIXNUM:
			fprintf(stream, "~boxed fixnum?!~");
			break;

		case CLOSURE:
			fprintf(stream, "~closure~");
			break;

		case NIL:
			fprintf(stream, "()");
			break;

		case VECTOR:
			fprintf(stream, "~vector~");
			break;

		case VBLOCK:
			fprintf(stream, "~vblock~");
			break;

		case HTABLE:
			fprintf(stream, "~htable~");
			break;

		case HBLOCK:
			fprintf(stream, "~hblock~");
			break;

		case STATIC_ENV:
			fprintf(stream, "~static-env~");
			break;

		case STATIC_FRAME:
			fprintf(stream, "~static-frame~");
			break;

		case FRAME:
			fprintf(stream, "~frame~");
			break;

		case THUNK:
			fprintf(stream, "~thunk~");
			break;

		case RAW:
			fprintf(stream, "~raw~");
			break;
		}
		break;

	case TAG_NIL:
		fprintf(stream, "()");
		break;
	}
}

static void print_list(FILE *stream, Value v)
{
	fprintf(stream, "(");
	while (is_cons(v)) {
		print(stream, car(v));
		v = cdr(v);
		if (is_cons(v))
			fprintf(stream, " ");
	}
	fprintf(stream, ")");
}

