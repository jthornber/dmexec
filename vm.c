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

#include "namespace.h"
#include "primitives.h"
#include "string_type.h"
#include "vm.h"

//----------------------------------------------------------------
// Math utils

unsigned round_up(unsigned n, unsigned pow)
{
	return (n + (pow - 1)) & -pow;
}

//----------------------------------------------------------------
// Values - immediate or reference
//
// The bottom 2 bits are used for tagging.

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

value_t mk_ref(void *ptr)
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

//----------------------------------------------------------------
// Objects

#define HEADER_MAGIC 846219U

struct header {
	enum object_type type;
	unsigned size; 		/* in bytes, we always round to a 4 byte boundary */
	unsigned magic;
};

//----------------------------------------------------------------
// Memory manager

// FIXME: use the valgrind api to allow it to check for accesses to garbage

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

void *alloc(enum object_type type, size_t s)
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

static struct header *obj_to_header(void *obj)
{
	return ((struct header *) obj) - 1;
}

static struct header *get_header(value_t v)
{
	struct header *h = obj_to_header(v.ptr);
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

static void set_type(void *obj, enum object_type t)
{
	struct header *h = obj_to_header(obj);
	h->type = t;
}

//----------------------------------------------------------------
// Words
value_t mk_word_like(enum object_type type, struct string *str)
{
	struct string *w = string_clone(str);
	set_type(w, WORD);
	return mk_ref(w);
}

value_t mk_symbol(struct string *str)
{
	return mk_word_like(SYMBOL, str);
}

value_t mk_word(struct string *str)
{
	return mk_word_like(WORD, str);
}

//----------------------------------------------------------------
// Byte array

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

//----------------------------------------------------------------
// Arrays

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

//----------------------------------------------------------------
// Printing values

static void print_string(FILE *stream, struct string *str)
{
	const char *ptr;

	fputc('\"', stream);
	for (ptr = str->b; ptr != str->e; ptr++)
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

#if 0
static void print_tuple(FILE *stream, struct tuple *t)
{
	// FIXME: print out the class
	print_array_like(stream, t, '|', '|');
}
#endif

static void print_word(FILE *stream, struct string *w)
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
		case NAMESPACE:
			fprintf(stream, "~namespace~");
			break;

		case NAMESPACE_ENTRY:
			fprintf(stream, "~namespace entry~");
			break;

		case PRIMITIVE:
			fprintf(stream, "~primitive~");
			break;

		case STRING:
			print_string(stream, (struct string *) v.ptr);
			break;

		case BYTE_ARRAY:
			fprintf(stream, "~byte array~");
			break;

		case TUPLE:
			//		print_tuple(stream, v.ptr);
			break;

		case SYMBOL:
		case WORD:
			print_word(stream, v.ptr);
			break;

		case QUOT:
			print_quot(stream, (struct array *) v.ptr);
			break;

		case ARRAY:
			print_array(stream, (struct array *) v.ptr);
			break;

		case CODE_POSITION:
			fprintf(stream, "~code position~");
			break;

		case CONTINUATION:
			fprintf(stream, "~continuation~");
			break;

		case FIXNUM:
			fprintf(stream, "~boxed fixnum?!~");
			break;
		}
		break;

	case TAG_FALSE:
		fprintf(stream, "f");
		break;
	}
}

//----------------------------------------------------------------
// Stack engine

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

//----------------------------------------------------------------
// Lexer

static bool more_input(struct string *in)
{
	return in->b != in->e;
}

static void step_input(struct string *in)
{
	in->b++;
}

static void consume_space(struct string *in)
{
	while (more_input(in) && isspace(*in->b))
		step_input(in);
}

static bool scan_fixnum(struct string *in, struct token *result)
{
	int n = 0;

	result->str.b = in->b;
	while (more_input(in) && isdigit(*in->b)) {
		n *= 10;
		n += *in->b - '0'; /* FIXME: assumes ascii */
		step_input(in);
	}

	if (more_input(in) && !isspace(*in->b)) {
		while (more_input(in) && !isspace(*in->b))
			step_input(in);

		result->str.e = in->b;
		result->type = TOK_WORD;

	} else {
		result->type = TOK_FIXNUM;
		result->fixnum = n;
	}

	return true;
}

static bool scan_string(struct string *in, struct token *result)
{
	result->type = TOK_STRING;
	step_input(in);
	result->str.b = in->b;

	// FIXME: support escapes
	while (more_input(in) && *in->b != '\"')
		step_input(in);

	result->str.e = in->b;

	if (!more_input(in)) {
		fprintf(stderr, "bad string\n");
		exit(1);
	}

	step_input(in);

	return true;
}

static bool scan_word(struct string *in, struct token *result)
{
	result->type = TOK_WORD;
	result->str.b = in->b;

	while (more_input(in) && !isspace(*in->b))
		step_input(in);

	result->str.e = in->b;
	return true;
}

static bool scan(struct string *in, struct token *result)
{
	if (!more_input(in))
		return false;

	consume_space(in);

	if (!more_input(in))
		return false;

	if (isdigit(*in->b))
		return scan_fixnum(in, result);

	else if (*in->b == '\"')
		return scan_string(in, result);

	else
		return scan_word(in, result);
}

//----------------------------------------------------------------
// Interpreter

struct primitive {
	prim_fn fn;
};

struct dynamic_scope {
	struct list_head list;
	struct list_head definitions;
};

static void init_continuation(struct continuation *k)
{
	init_stack(&k->stack);
	INIT_LIST_HEAD(&k->call_stack);
}

static void init_vm(struct vm *vm)
{
	memset(vm, 0, sizeof(*vm));
	vm->current_ns = namespace_create();
	vm->k = zalloc(CONTINUATION, sizeof(*vm->k));
	init_continuation(vm->k);
}

void add_primitive(struct vm *vm, char *name, prim_fn fn)
{
	struct primitive *p = alloc(PRIMITIVE, sizeof(*p));
	struct string k;

	string_tmp(name, &k);
	p->fn = fn;
	namespace_insert(vm->current_ns, &k, mk_ref(p));
}

static void def_word(struct vm *vm, struct string *w, struct array *body)
{
	namespace_insert(vm->current_ns, w, mk_ref(body));
}

#if 0
static struct primitive *find_primitive(struct vm *vm, struct string *w)
{
	struct primitive *p;

	list_for_each_entry (p, &vm->prims, list)
		if (!string_cmp_cstr(w, p->name))
			return p;

	return NULL;
}

static struct array *find_word_def(struct vm *vm, struct string *w)
{
	struct def *d;

	list_for_each_entry (d, &vm->definitions, list)
		if (!string_cmp(w, d->w))
			return d->body;

	return NULL;
}
#endif

void push_call(struct vm *vm, struct array *code)
{
	struct code_position *pc = zalloc(CODE_POSITION, sizeof(*pc));
	pc->code = code;
	pc->position = 0;
	list_add(&pc->list, &vm->k->call_stack);
}

void pop_call(struct vm *vm)
{
	struct code_position *pc = list_first_entry(&vm->k->call_stack, struct code_position, list);
	list_del(&pc->list);
}

static void inc_pc(struct vm *vm)
{
	if (!list_empty(&vm->k->call_stack)) {
		struct code_position *pc = list_first_entry(&vm->k->call_stack, struct code_position, list);
		if (++pc->position == pc->code->nr_elts)
			pop_call(vm);
	}
}

static void eval_value(struct vm *vm, value_t v)
{
	const char *b;
	struct primitive *p;
	struct string *w;
	struct array *body;
	struct header *h;
	value_t def;

	switch (get_tag(v)) {
	case TAG_FIXNUM:
		PUSH(v);
		break;

	case TAG_FALSE:
		PUSH(v);
		break;

	case TAG_REF:
		h = get_header(v);
		switch (h->type) {
		case NAMESPACE:
		case NAMESPACE_ENTRY:
		case PRIMITIVE:
		case STRING:
		case ARRAY:
		case BYTE_ARRAY:
		case TUPLE:
		case QUOT:
		case CONTINUATION:
		case SYMBOL:
			PUSH(v);
			break;

		case WORD:
			w = as_ref(v);
			if (namespace_lookup(vm->current_ns, w, &def)) {
				switch (get_type(def)) {
				case PRIMITIVE:
					p = as_ref(def);
					p->fn(vm);
					break;

				case QUOT:
					body = as_ref(def);
					push_call(vm, body);
					break;

				default:
					fprintf(stderr, "bad def\n");
					exit(1);
				}

			} else {
				// FIXME: add better error handling
				fprintf(stderr, "couldn't find primitive '");
				for (b = w->b; b != w->e; b++)
					fprintf(stderr, "%c", *b);
				fprintf(stderr, "'\n");
				exit(1);
			}
			break;

		case FIXNUM:
			fprintf(stderr, "we shouldn't ever have non immediate fixnum objects\n");
			break;

		case CODE_POSITION:
			fprintf(stderr, "unexpected value\n");
			break;
		}
	}
}

void eval(struct vm *vm, struct array *code)
{
	value_t v;
	struct code_position *pc;

	if (code->nr_elts) {
		push_call(vm, code);
		while (!list_empty(&vm->k->call_stack)) {
			pc = list_first_entry(&vm->k->call_stack, struct code_position, list);
			v = pc->code->elts[pc->position];
			inc_pc(vm);
			eval_value(vm, v);
		}
	}
}

//----------------------------------------------------------------
// String source

struct string_source {
	struct string in;
	struct token tok;
};

static bool string_next_value(struct vm *vm, struct string_source *in, value_t *r);

static bool syntax_quot(struct vm *vm, struct string_source *ss, value_t *r)
{
	value_t r2;

	*r = mk_quot();
	while (string_next_value(vm, ss, &r2) &&
	       string_cmp_cstr(&ss->tok.str, "]"))
		append_array(*r, r2);

	return true;
}

static bool syntax_array(struct vm *vm, struct string_source *ss, value_t *r)
{
	value_t r2;

	*r = mk_array();
	while (string_next_value(vm, ss, &r2) &&
	       string_cmp_cstr(&ss->tok.str, "}"))
		append_array(*r, r2);

	return true;
}

static void syntax_definition(struct vm *vm, struct string_source *ss)
{
	value_t w, body, v;

	if (!string_next_value(vm, ss, &w)) {
		fprintf(stderr, "bad definition");
		exit(1);
	}

	body = mk_quot();
	while (string_next_value(vm, ss, &v) &&
	       string_cmp_cstr(&ss->tok.str, ";"))
		append_array(body, v);

	def_word(vm, as_ref(w), as_ref(body));
}

static bool string_next_value(struct vm *vm, struct string_source *ss, value_t *r)
{
	if (!scan(&ss->in, &ss->tok))
		return false;

	switch (ss->tok.type) {
	case TOK_FIXNUM:
		*r = mk_fixnum(ss->tok.fixnum);
		break;

	case TOK_STRING:
		*r = mk_ref(string_clone(&ss->tok.str));
		break;

	case TOK_WORD:
		/*
		 * Syntax words should be evaluated immediately.
		 */
		if (!string_cmp_cstr(&ss->tok.str, "f"))
			*r = mk_false();

		else if (!string_cmp_cstr(&ss->tok.str, "{"))
			return syntax_array(vm, ss, r);

		else if (!string_cmp_cstr(&ss->tok.str, "["))
			return syntax_quot(vm, ss, r);

		else if (!string_cmp_cstr(&ss->tok.str, ":")) {
			syntax_definition(vm, ss);
			return string_next_value(vm, ss, r);

		} else if (*ss->tok.str.b == ':')
			*r = mk_symbol(&ss->tok.str);

		else
			*r = mk_word(&ss->tok.str);
		break;

	default:
		assert(!"implemented");
		exit(1);
	}

	return true;
}

static struct array *_read(struct vm *vm, struct string *str)
{
	value_t v;
	struct string_source in;
	value_t a = mk_array();

	in.in = *str;
	while (string_next_value(vm, &in, &v))
		append_array(a, v);

	return as_ref(a);
}

static void load_file(struct vm *vm, const char *path)
{
	int fd;
	struct stat info;
	struct string input;
	struct array *code;

	if (stat(path, &info) < 0) {
		fprintf(stderr, "couldn't stat '%s'\n", path);
		exit(1);
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "couldn't open '%s'\n", path);
		exit(1);
	}

	input.b = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!input.b) {
		fprintf(stderr, "couldn't mmap '%s'\n", path);
		exit(1);
	}
	input.e = input.b + info.st_size;

	code = _read(vm, &input);
	eval(vm, code);
	munmap(input.b, info.st_size);
	close(fd);
}

//----------------------------------------------------------------
// Top level

static void print_stack(struct stack *s)
{
	unsigned i;

	printf("\n--- Data stack:\n");
	for (i = 0; i < s->nr_entries; i++) {
		print_value(stdout, s->values[i]);
		printf("\n");
	}
}

// FIXME: this should be written in dm code
static int repl(struct vm *vm)
{
	char buffer[4096];
	struct string input;

	for (;;) {
		printf("dmexec> ");
		fflush(stdout);

		if (!fgets(buffer, sizeof(buffer), stdin))
			break;

		input.b = buffer;
		input.e = buffer + strlen(buffer);
		eval(vm, _read(vm, &input));
		print_stack(&vm->k->stack);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct vm vm;

	init_vm(&vm);
	add_basic_primitives(&vm);
	add_dm_primitives(&vm);

	load_file(&vm, "prelude.dm");

	repl(&vm);
	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) memory_stats_.total_allocated);

	return 0;
}
