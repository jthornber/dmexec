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

static void print_value(FILE *stream, value_t v);

/*----------------------------------------------------------------
 * Objects
 *--------------------------------------------------------------*/
#define HEADER_MAGIC 846219U

enum object_type {
	STRING,
	BYTE_ARRAY,
	TUPLE,
	WORD,
	QUOT,
	ARRAY,
	FIXNUM			/* these are always tagged immediate values */
};

struct header {
	enum object_type type;
	unsigned size; 		/* in bytes, we always round to a 4 byte boundary */
	unsigned magic;
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

static void *zalloc(enum object_type type, size_t s)
{
	void *ptr = alloc(type, s);
	memset(ptr, 0, s);
	return ptr;
}

static struct header *get_header(value_t v)
{
	struct header *h = (struct header *) v.ptr - 1;
	assert(h->magic == HEADER_MAGIC);
	return h;
};

enum object_type get_type(value_t v)
{
	struct header *h;

	if (get_tag(v) == TAG_FIXNUM)
		return FIXNUM;

	h = get_header(v);
	return h->type;
}

/*----------------------------------------------------------------
 * Words
 *--------------------------------------------------------------*/
struct word {
	char *b, *e;
};

value_t mk_word(const char *begin, const char *end)
{
	char *ptr;
	struct word *w = alloc(WORD, sizeof(*w) + round_up(end - begin, 4));

	w->b = (char *)(w + 1);
	for (ptr = w->b; begin != end; ptr++, begin++)
		*ptr = *begin;
	w->e = ptr;

	return mk_ref(w);
}

/*----------------------------------------------------------------
 * Byte array
 *--------------------------------------------------------------*/
struct byte_array {
	unsigned allocated;
	unsigned len;
	unsigned char *bytes;
};

struct byte_array *mk_byte_array(unsigned len)
{
	struct byte_array *ba = zalloc(BYTE_ARRAY, sizeof(*ba));
	ba->bytes = malloc(len);
	if (!ba->bytes)
		out_of_memory();

	ba->allocated = len;
	return ba;
}

void realloc_byte_array(struct byte_array *ba, unsigned new_len)
{
	unsigned char *new_bytes = malloc(new_len);
	if (!new_bytes)
		out_of_memory();
	memcpy(new_bytes, ba->bytes, ba->len);
	free(ba->bytes);
	ba->bytes = new_bytes;
}

void push_byte(struct byte_array *ba, unsigned b)
{
	if (ba->len == ba->allocated)
		realloc_byte_array(ba, ba->len * 2);

	ba->bytes[ba->len++] = b;
}

/*----------------------------------------------------------------
 * String handling
 *--------------------------------------------------------------*/
struct string {
	char *begin;
	char *end;
	char *alloc_end;
};

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
 * Arrays
 *--------------------------------------------------------------*/
#define MAX_ARRAY_SIZE 32

// FIXME: add dynamic resizing
struct array {
	unsigned nr_elts;
	value_t elts[MAX_ARRAY_SIZE]; /* yee haa! */
};

static value_t mk_array()
{
	struct array *a = alloc(ARRAY, sizeof(*a));
	memset(a, 0, sizeof(*a));
	a->nr_elts = 0;
	return mk_ref(a);
}

static value_t mk_quot()
{
	struct array *a = alloc(QUOT, sizeof(*a));
	memset(a, 0, sizeof(*a));
	a->nr_elts = 0;
	return mk_ref(a);
}

static void append_array(value_t av, value_t v)
{
	struct array *a = as_ref(av);

	assert(a->nr_elts < MAX_ARRAY_SIZE);
	fprintf(stderr, "adding value");
	print_value(stderr, v);
	a->elts[a->nr_elts++] = v;
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

static void print_array_like(FILE *stream, struct array *a, char b, char e)
{
	unsigned i;

	fprintf(stream, "%c ", b);
	for (i = 0; i < a->nr_elts; i++) {
		print_value(stream, a->elts[i]);
		fprintf(stream, " ");
	}
	fprintf(stream, "%c", e);
}

static void print_array(FILE *stream, struct array *a)
{
	print_array_like(stream, a, '{', '}');
}

static void print_quot(FILE *stream, struct array *a)
{
	print_array_like(stream, a, '[', ']');
}

static void print_word(FILE *stream, struct word *w)
{
	const char *ptr;

	for (ptr = w->b; ptr != w->e; ptr++)
		fputc(*ptr, stream);
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

		case ARRAY:
			print_array(stream, (struct array *) v.ptr);
			break;

		case QUOT:
			print_quot(stream, (struct array *) v.ptr);
			break;

		case WORD:
			print_word(stream, (struct word *) v.ptr);
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

struct input_source {
	bool (*next_value)(struct input_source *in, value_t *v);
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

static void interpret(struct interpreter *terp, struct input_source *in)
{
	const char *b;
	struct primitive *p;
	value_t v;
	struct word *w;
	struct header *h;

	while (in->next_value(in, &v)) {
		switch (get_tag(v)) {
		case TAG_FIXNUM:
			push(&terp->stack, v);
			break;

		case TAG_REF:
			h = get_header(v);
			switch (h->type) {
			case STRING:
				push(&terp->stack, v);
				break;

			case ARRAY:
				push(&terp->stack, v);
				break;

			case BYTE_ARRAY:
				push(&terp->stack, v);
				break;

			case TUPLE:
				push(&terp->stack, v);
				break;

			case WORD:
				w = (struct word *) as_ref(v);
				p = find_primitive(terp, w->b, w->e);
				if (!p) {
					// FIXME: add better error handling
					fprintf(stderr, "couldn't find primitive '");
					for (b = w->b; b != w->e; b++)
						fprintf(stderr, "%c", *b);
					fprintf(stderr, "'\n");
					exit(1);

				} else
					p->fn(terp);
				break;

			case QUOT:
				push(&terp->stack, v);
				break;

			case FIXNUM:
				fprintf(stderr, "we shouldn't ever have non immediate fixnum objects\n");
				break;
			}
		}
	}
}

/*----------------------------------------------------------------
 * Quot source
 *--------------------------------------------------------------*/
struct quot_source {
	struct input_source source;
	struct array *quot;
	unsigned index;
};

static bool quot_next_value(struct input_source *in, value_t *r)
{
	struct quot_source *qs = container_of(in, struct quot_source, source);
	if (qs->index < qs->quot->nr_elts) {
		*r = qs->quot->elts[qs->index++];
		return true;
	}

	return false;
}

static void interpret_quot(struct interpreter *terp, struct array *q)
{
	struct quot_source in;

	in.source.next_value = quot_next_value;
	in.quot = q;
	in.index = 0;

	interpret(terp, &in.source);
}

/*----------------------------------------------------------------
 * String source
 *--------------------------------------------------------------*/
struct string_source {
	struct input_source source;

	struct input in;
	struct token tok;
};

static bool string_next_value(struct input_source *in, value_t *r)
{
	value_t r2;
	struct string_source *ss = container_of(in, struct string_source, source);

	if (!scan(&ss->in, &ss->tok))
		return false;

	switch (ss->tok.type) {
	case TOK_FIXNUM:
		*r = mk_fixnum(ss->tok.fixnum);
		break;

	case TOK_STRING:
		*r = mk_string(ss->tok.begin, ss->tok.end);
		break;

	case TOK_WORD:
		// FIXME: do a full compare of the token
		if (*ss->tok.begin == '{') {
			*r = mk_array();
			while (string_next_value(in, &r2))
				append_array(*r, r2);

		} else if (*ss->tok.begin == '[') {
			*r = mk_quot();
			while (string_next_value(in, &r2))
				append_array(*r, r2);

		} else if (*ss->tok.begin == '}') {
			return false;

		} else if (*ss->tok.begin == ']') {
			return false;

		} else
			*r = mk_word(ss->tok.begin, ss->tok.end);
		break;

	default:
		assert(!"implemented");
		exit(1);
	}

	return true;
}

static void interpret_string(struct interpreter *terp, const char *str)
{
	struct string_source in;

	in.source.next_value = string_next_value;
	in.in.begin = str;
	in.in.end = str + strlen(str);

	interpret(terp, &in.source);
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

static void call(struct interpreter *terp)
{
	value_t maybe_q = pop(&terp->stack);

	fprintf(stderr, "calliing call\n");

	if (get_type(maybe_q) != QUOT) {
		fprintf(stderr, "not a quotation\n");
		exit(1);
	}

	interpret_quot(terp, as_ref(maybe_q));
}

static void add_primitives(struct interpreter *terp)
{
	add_primitive(terp, ".", dot);
	add_primitive(terp, "dup", dup);
	add_primitive(terp, "+", fixnum_add);
	add_primitive(terp, "-", fixnum_sub);
	add_primitive(terp, "call", call);
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
