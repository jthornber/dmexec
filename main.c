#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "list.h"

/*----------------------------------------------------------------
 * Math utils
 *--------------------------------------------------------------*/
unsigned round_up(unsigned n, unsigned pow)
{
	return (n + (pow - 1)) & -pow;
}

/*----------------------------------------------------------------
 * Values - immediate or reference
 *
 * The bottom 2 bits are used for tagging.
 *--------------------------------------------------------------*/
enum tag {
	TAG_REF = 0,
	TAG_FIXNUM = 1
};

typedef union value {
	void *ptr;
	int32_t i;
} value_t;

static enum tag get_tag(value_t v)
{
	return v.i & 0x3;
}

static value_t mk_fixnum(int i)
{
	value_t v;
	v.i = (i << 2) | TAG_FIXNUM;
	return v;
}

static unsigned as_fixnum(value_t v)
{
	assert(get_tag(v) == TAG_FIXNUM);
	return v.i >> 2;
}

static value_t mk_ref(void *ptr)
{
	value_t v;
	v.ptr = ptr;
	return v;
}

static void *as_ref(value_t v)
{
	assert(get_tag(v) == TAG_REF);
	return v.ptr;
}

/*----------------------------------------------------------------
 * Objects
 *--------------------------------------------------------------*/
#define HEADER_MAGIC 846219U

enum object_type {
	FIXNUM,
	STRING,
	TUPLE,
	WORD,
	QUOT,
	ARRAY
};

struct header {
	enum object_type type;
	unsigned size; 		/* in bytes, we always round to a 4 byte boundary */
	unsigned magic;
};

struct string {
	char *begin;
	char *end;
	char *alloc_end;
};

/*----------------------------------------------------------------
 * Memory manager
 *--------------------------------------------------------------*/
static void out_of_memory()
{
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

static void *alloc(enum object_type type, size_t s)
{
	struct header *h = malloc(s + sizeof(*h));

	if (!h)
		out_of_memory();

	h->type = type;
	h->size = s;
	h->magic = HEADER_MAGIC;

	return ((char *) (h + 1));
}

static struct header *get_header(value_t v)
{
	struct header *h = (struct header *) v.ptr - 1;
	assert(h->magic == HEADER_MAGIC);
	return h;
};

/*----------------------------------------------------------------
 * String handling
 *--------------------------------------------------------------*/
static struct string *alloc_string(unsigned space)
{
	struct string *s;
	char *b;

	space = round_up(space, 4);
	s = alloc(STRING, sizeof(struct string) + space);
	b = (char *) (s + 1);

	s->begin = s->end = b;
	s->alloc_end = b + space;

	return s;
}

static struct string *clone_string(struct string *orig)
{
	struct string *new = alloc_string(orig->alloc_end - orig->begin);
	memcpy(new->begin, orig->begin, orig->end - orig->begin);
	return new;
}

static value_t mk_string(const char *b, const char *e)
{
	unsigned len = e - b;
	struct string *s = alloc_string(len);

	memcpy(s->begin, b, len);
	s->end = s->begin + len;

	return mk_ref(s);
}

/*----------------------------------------------------------------
 * Printing values
 *--------------------------------------------------------------*/
static void print_string(FILE *stream, struct string *str)
{
	const char *ptr;

	fputc('\"', stream);
	for (ptr = str->begin; ptr != str->end; ptr++)
		fputc(*ptr, stream);
	fputc('\"', stream);
}

static void print_value(FILE *stream, value_t v)
{
	struct header *h;

	switch (get_tag(v)) {
	case TAG_FIXNUM:
		fprintf(stream, "%d", as_fixnum(v));
		break;

	case TAG_REF:
		h = get_header(v);
		switch (h->type) {
		case STRING:
			print_string(stream, (struct string *) v.ptr);
			break;

		default:
			fprintf(stderr, "not implemented\n");
		}
		break;
	}
}

/*----------------------------------------------------------------
 * Stack engine
 *--------------------------------------------------------------*/
#define MAX_STACK 8192

struct stack {
	unsigned nr_entries;
	value_t values[MAX_STACK];
};

static void init_stack(struct stack *s)
{
	s->nr_entries = 0;
}

static void push(struct stack *s, value_t v)
{
	assert(s->nr_entries < MAX_STACK);
	s->values[s->nr_entries++] = v;
}

static value_t peek(struct stack *s)
{
	assert(s->nr_entries);
	return s->values[s->nr_entries - 1];
}

static value_t pop(struct stack *s)
{
	assert(s->nr_entries);
	s->nr_entries--;

	return s->values[s->nr_entries];
}

/*----------------------------------------------------------------
 * Lexer
 *--------------------------------------------------------------*/
enum token_type {
	TOK_FIXNUM,
	TOK_STRING,
	TOK_WORD,
	TOK_COLON,
	TOK_SEMI_COLON,
	TOK_OPEN_BRACE,
	TOK_CLOSE_BRACE,
	TOK_OPEN_SQUARE,
	TOK_CLOSE_SQUARE
};

struct token {
	enum token_type type;
	const char *begin;
	const char *end;

	int fixnum;
};

struct input {
	const char *begin;
	const char *end;
};

static bool more_input(struct input *in)
{
	return in->begin != in->end;
}

static void step_input(struct input *in)
{
	in->begin++;
}

static void consume_space(struct input *in)
{
	while (more_input(in) && isspace(*in->begin))
		step_input(in);
}

static bool scan_fixnum(struct input *in, struct token *result)
{
	int n = 0;

	while (more_input(in) && isdigit(*in->begin)) {
		n *= 10;
		n += *in->begin - '0'; /* FIXME: assumes ascii */
		step_input(in);
	}

	result->type = TOK_FIXNUM;
	result->fixnum = n;

	if (more_input(in) && !isspace(*in->begin)) {
		fprintf(stderr, "lex error");
		exit(1);	/* FIXME: handle */
	}

	return true;
}

static bool scan_string(struct input *in, struct token *result)
{
	result->type = TOK_STRING;
	step_input(in);
	result->begin = in->begin;

	// FIXME: support escapes
	while (more_input(in) && *in->begin != '\"')
		step_input(in);

	result->end = in->begin;

	if (!more_input(in)) {
		fprintf(stderr, "bad string\n");
		exit(1);
	}

	step_input(in);

	return true;
}

static bool scan_word(struct input *in, struct token *result)
{
	result->type = TOK_WORD;
	result->begin = in->begin;

	while (more_input(in) && !isspace(*in->begin))
		step_input(in);

	result->end = in->begin;
	return true;
}

static bool scan(struct input *in, struct token *result)
{
	if (!more_input(in))
		return false;

	consume_space(in);

	if (!more_input(in))
		return false;

	if (isdigit(*in->begin))
		return scan_fixnum(in, result);

	else if (*in->begin == '\"')
		return scan_string(in, result);

	else
		return scan_word(in, result);
}

/*----------------------------------------------------------------
 * Interpreter
 *--------------------------------------------------------------*/
struct interpreter;

typedef void (*prim_fn)(struct interpreter *);

struct primitive {
	struct list_head list;
	char *name;
	prim_fn fn;
};

struct dynamic_scope {
	struct list_head list;
	struct list_head definitions;
};

struct interpreter {
	struct list_head prims;
	struct stack stack;
	struct token tok;
};

static void init_interpreter(struct interpreter *terp)
{
	memset(terp, 0, sizeof(*terp));
	INIT_LIST_HEAD(&terp->prims);
	init_stack(&terp->stack);
}

static void add_primitive(struct interpreter *terp, const char *name, prim_fn fn)
{
	// FIXME: should this be managed by the mm?
	struct primitive *p = malloc(sizeof(*p));

	assert(p);
	p->name = strdup(name);
	p->fn = fn;

	list_add(&p->list, &terp->prims);
}

static int cmp_str_tok(const char *str, const char *b, const char *e)
{
	while (b != e && *str) {
		if (*b < *str)
			return -1;

		else if (*b > *str)
			return 1;

		b++;
		str++;
	}

	if (b == e && !*str)
		return 0;

	else if (b == e)
		return -1;

	else
		return 1;
}

static struct primitive *find_primitive(struct interpreter *terp, const char *b, const char *e)
{
	struct primitive *p;

	list_for_each_entry (p, &terp->prims, list)
		if (!cmp_str_tok(p->name, b, e))
			return p;

	return NULL;
}

static void interpret(struct interpreter *terp, struct input *in)
{
	const char *b;
	struct primitive *p;

	while (scan(in, &terp->tok)) {
		switch (terp->tok.type) {
		case TOK_FIXNUM:
			push(&terp->stack, mk_fixnum(terp->tok.fixnum));
			break;

		case TOK_STRING:
			push(&terp->stack, mk_string(terp->tok.begin, terp->tok.end));
			break;

		case TOK_WORD:
			p = find_primitive(terp, terp->tok.begin, terp->tok.end);
			if (!p) {
				// FIXME: add better error handling
				fprintf(stderr, "couldn't find primitive '");
				for (b = terp->tok.begin; b != terp->tok.end; b++)
					fprintf(stderr, "%c", *b);
				fprintf(stderr, "'\n");
				exit(1);
			}
			p->fn(terp);
			break;

		case TOK_COLON:
		case TOK_SEMI_COLON:
		case TOK_OPEN_BRACE:
		case TOK_CLOSE_BRACE:
		case TOK_OPEN_SQUARE:
		case TOK_CLOSE_SQUARE:
			assert(!"implemented");
		};
	}
}

static void interpret_string(struct interpreter *terp, const char *str)
{
	struct input in;

	in.begin = str;
	in.end = str + strlen(str);

	interpret(terp, &in);
}

/*----------------------------------------------------------------
 * Primitives
 *--------------------------------------------------------------*/
static void dot(struct interpreter *terp)
{
	value_t v = pop(&terp->stack);
	print_value(stdout, v);
	printf("\n");
}

static void dup(struct interpreter *terp)
{
	value_t v = peek(&terp->stack);
	push(&terp->stack, v);
}

static void fixnum_add(struct interpreter *terp)
{
	value_t v1 = pop(&terp->stack);
	value_t v2 = pop(&terp->stack);

	push(&terp->stack, mk_fixnum(as_fixnum(v1) + as_fixnum(v2)));
}

static void fixnum_sub(struct interpreter *terp)
{
	value_t v1 = pop(&terp->stack);
	value_t v2 = pop(&terp->stack);

	push(&terp->stack, mk_fixnum(as_fixnum(v2) - as_fixnum(v1)));
}

static void add_primitives(struct interpreter *terp)
{
	add_primitive(terp, ".", dot);
	add_primitive(terp, "dup", dup);
	add_primitive(terp, "+", fixnum_add);
	add_primitive(terp, "-", fixnum_sub);
}

/*----------------------------------------------------------------
 * Top level
 *--------------------------------------------------------------*/
static void print_stack(struct stack *s)
{
	unsigned i;

	printf("\n ~~~ stack ~~~\n");
	for (i = 0; i < s->nr_entries; i++) {
		print_value(stdout, s->values[i]);
		printf("\n");
	}
}

static int repl(struct interpreter *terp)
{
	char buffer[4096];

	for (;;) {
		printf("dmexec> ");
		fflush(stdout);

		if (!fgets(buffer, sizeof(buffer), stdin))
			break;

		interpret_string(terp, buffer);
		print_stack(&terp->stack);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct interpreter terp;

	init_interpreter(&terp);
	add_primitives(&terp);
	return repl(&terp);
}
