#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
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
// Words
value_t mk_word_like(enum object_type type, struct string *str)
{
	struct string *w = string_clone(str);
	set_obj_type(w, WORD);
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

value_t mk_word_cstr(char *str)
{
	struct string tmp;
	string_tmp(str, &tmp);
	return mk_word(&tmp);
}

#if 0
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
#endif
//----------------------------------------------------------------
// Quotations

value_t mk_quot()
{
	struct array *a = array_create();
	set_obj_type(a, QUOT);
	return mk_ref(a);
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
		print_value(stream, array_get(a, i));
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
		case FORWARD:
			error("unexpected fwd ptr");
			break;

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

	if (!more_input(in))
		error("bad string");

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
	k->stack = mk_ref(array_create());
	k->call_stack = mk_ref(array_create());
}

static void init_vm(struct vm *vm)
{
	memset(vm, 0, sizeof(*vm));
	vm->current_ns = namespace_create();
	vm->k = zalloc(CONTINUATION, sizeof(*vm->k));
	init_continuation(vm->k);
}

void def_primitive(struct vm *vm, char *name, prim_fn fn)
{
	struct primitive *p = alloc(PRIMITIVE, sizeof(*p));
	struct string k;

	string_tmp(name, &k);
	p->fn = fn;
	namespace_insert(vm->current_ns, &k, mk_ref(p));
}

static void def_word(struct string *w, struct array *body)
{
	namespace_insert(global_vm->current_ns, w, mk_ref(body));
}

void push_call(struct array *code)
{
	struct code_position *pc = zalloc(CODE_POSITION, sizeof(*pc));
	pc->code = code;
	pc->position = 0;
	array_push(as_ref(global_vm->k->call_stack), mk_ref(pc));
}

void pop_call()
{
	array_pop(as_ref(global_vm->k->call_stack));
}

static void inc_pc()
{
	struct array *s = as_ref(global_vm->k->call_stack);

	if (s->nr_elts) {
		struct code_position *pc = as_type(CODE_POSITION, array_peek(s));
		if (++pc->position == pc->code->nr_elts)
			pop_call();
	}
}

static void eval_value(value_t v)
{
	struct primitive *p;
	struct string *w;
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
		case FORWARD:
			error("unexpected fwd");
			break;

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
			if (namespace_lookup(global_vm->current_ns, w, &def)) {
				switch (get_type(def)) {
				case PRIMITIVE:
					p = as_ref(def);
					p->fn();
					break;

				case QUOT:
					push_call(as_ref(def));
					break;

				default:
					error("bad def");
				}

			} else {
				// FIXME: add better error handling
				error("couldn't lookup word");
				//for (b = w->b; b != w->e; b++)
				//	fprintf(stderr, "%c", *b);
				//fprintf(stderr, "'\n");
			}
			break;

		case FIXNUM:
			error("we shouldn't ever have non immediate fixnum objects");
			break;

		case CODE_POSITION:
			error("unexpected value");
			break;
		}
	}
}

static bool more_code()
{
	struct array *s = as_ref(global_vm->k->call_stack);
	return s->nr_elts;
}

struct vm *global_vm = NULL;

void eval(struct vm *vm, struct array *code)
{
	value_t v;
	struct code_position *pc;

	if (!code->nr_elts)
		return;

	push_call(code);
	setjmp(vm->eval_loop);

	while (more_code(vm)) {
		struct array *s = as_ref(vm->k->call_stack);
		pc = as_type(CODE_POSITION, array_peek(s));
		v = array_get(pc->code, pc->position);
		inc_pc(vm);
		eval_value(v);
	}
}

void error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	if (global_vm->exception_stack->nr_elts) {
		fprintf(stderr, "jumping back to eval loop");
		global_vm->k = as_ref(array_pop(global_vm->exception_stack));
		longjmp(global_vm->eval_loop, -1);

	} else {
		fprintf(stderr, "no error handler, exiting\n");
		exit(1);
	}
}

//----------------------------------------------------------------
// String source

struct string_source {
	struct string in;
	struct token tok;
};

static bool string_next_value(struct string_source *in, value_t *r);

static bool is_word(char *target, value_t v)
{
	struct string *w;

	if (get_type(v) != WORD)
		return false;

	w = as_ref(v);
	return !string_cmp_cstr(w, target);
}

static bool syntax_quot(struct string_source *ss, value_t *r)
{
	value_t r2;

	*r = mk_quot();
	while (string_next_value(ss, &r2) && !is_word("]", r2))
		array_push(as_ref(*r), r2);

	return true;
}

static bool syntax_array(struct string_source *ss, value_t *r)
{
	value_t r2;

	*r = mk_ref(array_create());
	while (string_next_value(ss, &r2) && !is_word("}", r2))
		array_push(as_ref(*r), r2);

	return true;
}

static void syntax_definition(struct string_source *ss)
{
	value_t w, body, v;

	if (!string_next_value(ss, &w)) {
		error("bad definition");
		exit(1);
	}

	body = mk_quot();
	while (string_next_value(ss, &v) && !is_word(";", v))
		array_push(as_ref(body), v);

	def_word(as_ref(w), as_ref(body));
}

static bool string_next_value(struct string_source *ss, value_t *r)
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
			return syntax_array(ss, r);

		else if (!string_cmp_cstr(&ss->tok.str, "["))
			return syntax_quot(ss, r);

		else if (!string_cmp_cstr(&ss->tok.str, ":")) {
			syntax_definition(ss);
			return string_next_value(ss, r);

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

static struct array *_read(struct string *str)
{
	value_t v;
	struct string_source in;
	value_t a = mk_ref(array_create());

	in.in = *str;
	while (string_next_value(&in, &v))
		array_push(as_ref(a), v);

	return as_ref(a);
}

static void load_file(struct vm *vm, const char *path)
{
	int fd;
	struct stat info;
	struct string input;
	struct array *code;

	if (stat(path, &info) < 0)
		error("couldn't stat '%s'\n", path);

	fd = open(path, O_RDONLY);
	if (fd < 0)
		error("couldn't open '%s'\n", path);

	input.b = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!input.b)
		error("couldn't mmap '%s'\n", path);

	input.e = input.b + info.st_size;

	global_vm = vm;
	code = _read(&input);
	eval(vm, code);
	munmap(input.b, info.st_size);
	close(fd);
}

//----------------------------------------------------------------
// Top level

static void print_stack(struct vm *vm, value_t s)
{
	unsigned i;
	struct array *a = as_ref(s);

	printf("\n--- Data stack:\n");
	for (i = 0; i < a->nr_elts; i++) {
		print_value(stdout, array_get(a, i));
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
		global_vm = vm;
		eval(vm, _read(&input));
		print_stack(vm, vm->k->stack);
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct vm vm;

	init_vm(&vm);
	def_basic_primitives(&vm);
	def_dm_primitives(&vm);

	load_file(&vm, "prelude.dm");

	if (argc > 1) {
		unsigned i;

		for (i = 1; i < argc; i++)
			load_file(&vm, argv[i]);

	} else
		repl(&vm);

	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) get_memory_stats()->total_allocated);

	return 0;
}
