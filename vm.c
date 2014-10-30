#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "vm.h"
#include "primitives.h"

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

static enum tag get_tag(value_t v)
{
	return v.i & 0x3;
}

value_t mk_fixnum(int i)
{
	value_t v;
	v.i = (i << 2) | TAG_FIXNUM;
	return v;
}

unsigned as_fixnum(value_t v)
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

void *as_ref(value_t v)
{
	assert(get_tag(v) == TAG_REF);
	return v.ptr;
}

static value_t mk_false()
{
	value_t v;
	v.i = TAG_FALSE;
	return v;
}

bool is_false(value_t v)
{
	return v.i == TAG_FALSE;
}

/*----------------------------------------------------------------
 * Objects
 *--------------------------------------------------------------*/
#define HEADER_MAGIC 846219U

struct header {
	enum object_type type;
	unsigned size; 		/* in bytes, we always round to a 4 byte boundary */
	unsigned magic;
};

/*----------------------------------------------------------------
 * Memory manager
 *--------------------------------------------------------------*/
static struct {
	size_t total_allocated;
	size_t total_collected;
	size_t current_allocated;
	size_t max_allocated;
	unsigned nr_gcs;

} memory_stats_;

static void out_of_memory()
{
	fprintf(stderr, "Out of memory.\n");
	exit(1);
}

static void *alloc(enum object_type type, size_t s)
{
	size_t len = s + sizeof(struct header);
	struct header *h = malloc(len);

	if (!h)
		out_of_memory();

	h->type = type;
	h->size = s;
	h->magic = HEADER_MAGIC;

	memory_stats_.total_allocated += len;
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

static bool word_eq(struct word *lhs, struct word *rhs)
{
	const char *p1 = lhs->b;
	const char *p2 = rhs->b;

	while (p1 != lhs->e && p2 != rhs->e) {
		if (*p1 != *p2)
			return false;

		p1++, p2++;
	}

	return p1 == lhs->e && p2 == rhs->e;
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

value_t mk_string(const char *b, const char *e)
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
value_t mk_array()
{
	struct array *a = alloc(ARRAY, sizeof(*a));
	memset(a, 0, sizeof(*a));
	a->nr_elts = 0;
	return mk_ref(a);
}

value_t mk_quot()
{
	struct array *a = alloc(QUOT, sizeof(*a));
	memset(a, 0, sizeof(*a));
	a->nr_elts = 0;
	return mk_ref(a);
}

void append_array(value_t av, value_t v)
{
	struct array *a = as_ref(av);

	assert(a->nr_elts < MAX_ARRAY_SIZE);
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

void print_value(FILE *stream, value_t v)
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

	case TAG_FALSE:
		fprintf(stream, "f");
		break;
	}
}

/*----------------------------------------------------------------
 * Stack engine
 *--------------------------------------------------------------*/
void init_stack(struct stack *s)
{
	s->nr_entries = 0;
}

void push(struct stack *s, value_t v)
{
	assert(s->nr_entries < MAX_STACK);
	s->values[s->nr_entries++] = v;
}

value_t peek(struct stack *s)
{
	assert(s->nr_entries);
	return s->values[s->nr_entries - 1];
}

value_t peekn(struct stack *s, unsigned n)
{
	assert(s->nr_entries > n);
	return s->values[s->nr_entries - n - 1];
}

value_t pop(struct stack *s)
{
	assert(s->nr_entries);
	s->nr_entries--;

	return s->values[s->nr_entries];
}

/*----------------------------------------------------------------
 * Lexer
 *--------------------------------------------------------------*/
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

	result->begin = in->begin;
	while (more_input(in) && isdigit(*in->begin)) {
		n *= 10;
		n += *in->begin - '0'; /* FIXME: assumes ascii */
		step_input(in);
	}

	if (more_input(in) && !isspace(*in->begin)) {
		while (more_input(in) && !isspace(*in->begin))
			step_input(in);

		result->end = in->begin;
		result->type = TOK_WORD;

	} else {
		result->type = TOK_FIXNUM;
		result->fixnum = n;
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
struct primitive {
	struct list_head list;
	char *name;
	prim_fn fn;
};

struct def {
	struct list_head list;
	struct word *w;
	struct array *body;
};

struct dynamic_scope {
	struct list_head list;
	struct list_head definitions;
};

struct input_source {
	bool (*next_value)(struct input_source *in, value_t *v);
};

static void init_interpreter(struct interpreter *terp)
{
	memset(terp, 0, sizeof(*terp));
	INIT_LIST_HEAD(&terp->prims);
	INIT_LIST_HEAD(&terp->definitions);
	init_stack(&terp->stack);
	INIT_LIST_HEAD(&terp->call_stack);
}

void add_primitive(struct interpreter *terp, const char *name, prim_fn fn)
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

static void add_word_def(struct interpreter *terp, struct word *w, struct array *body)
{
	struct def *d = zalloc(DEF, sizeof(*d));
	list_add(&d->list, &terp->definitions);
	d->w = w;
	d->body = body;
}

static struct array *find_word_def(struct interpreter *terp, struct word *w)
{
	struct def *d;

	list_for_each_entry (d, &terp->definitions, list)
		if (word_eq(w, d->w))
			return d->body;

	return NULL;
}

void eval(struct interpreter *terp, struct array *code)
{
	const char *b;
	struct primitive *p;
	value_t v;
	struct word *w;
	struct array *body;
	struct header *h;
	unsigned i;

	for (i = 0; i < code->nr_elts; i++ ) {
		v = code->elts[i];

		switch (get_tag(v)) {
		case TAG_FIXNUM:
			push(&terp->stack, v);
			break;

		case TAG_FALSE:
			PUSH(v);
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
				if (p) {
					p->fn(terp);
					break;
				}

				body = find_word_def(terp, w);
				if (body) {
					eval(terp, body);
					break;
				}


				{
					// FIXME: add better error handling
					fprintf(stderr, "couldn't find primitive '");
					for (b = w->b; b != w->e; b++)
						fprintf(stderr, "%c", *b);
					fprintf(stderr, "'\n");
					exit(1);
				}

				break;

			case QUOT:
				push(&terp->stack, v);
				break;

			case FIXNUM:
				fprintf(stderr, "we shouldn't ever have non immediate fixnum objects\n");
				break;

			case DEF:
				fprintf(stderr, "unexpected def\n");
				break;
			}
		}
	}
}

/*----------------------------------------------------------------
 * String source
 *--------------------------------------------------------------*/
struct string_source {
	struct input_source source;

	struct input in;
	struct token tok;
};

static bool string_next_value(struct interpreter *terp, struct input_source *in, value_t *r);

static bool syntax_quot(struct interpreter *terp, struct string_source *ss, value_t *r)
{
	value_t r2;

	*r = mk_quot();
	while (string_next_value(terp, &ss->source, &r2) &&
	       cmp_str_tok("]", ss->tok.begin, ss->tok.end))
		append_array(*r, r2);

	return true;
}

static bool syntax_array(struct interpreter *terp, struct string_source *ss, value_t *r)
{
	value_t r2;

	*r = mk_array();
	while (string_next_value(terp, &ss->source, &r2) &&
	       cmp_str_tok("}", ss->tok.begin, ss->tok.end))
		append_array(*r, r2);

	return true;
}

static void syntax_definition(struct interpreter *terp, struct string_source *ss)
{
	value_t w, body, v;

	if (!string_next_value(terp, &ss->source, &w)) {
		fprintf(stderr, "bad definition");
		exit(1);
	}

	body = mk_quot();
	while (string_next_value(terp, &ss->source, &v) &&
	       cmp_str_tok(";", ss->tok.begin, ss->tok.end))
		append_array(body, v);

	add_word_def(terp, as_ref(w), as_ref(body));
}

static bool string_next_value(struct interpreter *terp, struct input_source *in, value_t *r)
{
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
		/*
		 * Syntax words should be evaluated immediately.
		 */
		if (!cmp_str_tok("f", ss->tok.begin, ss->tok.end))
			*r = mk_false();

		else if (!cmp_str_tok("{", ss->tok.begin, ss->tok.end))
			return syntax_array(terp, ss, r);

		else if (!cmp_str_tok("[", ss->tok.begin, ss->tok.end))
			return syntax_quot(terp, ss, r);

		else if (!cmp_str_tok(":", ss->tok.begin, ss->tok.end)) {
			syntax_definition(terp, ss);
			return string_next_value(terp, in, r);

		} else
			*r = mk_word(ss->tok.begin, ss->tok.end);
		break;

	default:
		assert(!"implemented");
		exit(1);
	}

	return true;
}

static struct array *_read(struct interpreter *terp, const char *b, const char *e)
{
	value_t v;
	struct string_source in;
	value_t a = mk_array();

	in.in.begin = b;
	in.in.end = e;

	while (string_next_value(terp, &in.source, &v))
		append_array(a, v);

	return as_ref(a);
}

static void load_file(struct interpreter *terp, const char *path)
{
	int fd;
	struct stat info;
	char *b, *e;

	if (stat(path, &info) < 0) {
		fprintf(stderr, "couldn't stat '%s'\n", path);
		exit(1);
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "couldn't open '%s'\n", path);
		exit(1);
	}

	b = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!b) {
		fprintf(stderr, "couldn't mmap '%s'\n", path);
		exit(1);
	}
	e = b + info.st_size;

	eval(terp, _read(terp, b, e));
	munmap(b, info.st_size);
	close(fd);
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

		eval(terp, _read(terp, buffer, buffer + strlen(buffer)));
		print_stack(&terp->stack);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct interpreter terp;

	init_interpreter(&terp);
	add_basic_primitives(&terp);
	add_dm_primitives(&terp);

	load_file(&terp, "prelude.dm");

	repl(&terp);
	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) memory_stats_.total_allocated);

	return 0;
}
