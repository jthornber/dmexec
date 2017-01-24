#include <ctype.h>
#include <fcntl.h>
#include <gc.h>
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
#include <readline/readline.h>
#include <readline/history.h>

#include "namespace.h"
#include "primitives.h"
#include "string_type.h"
#include "vm.h"

//----------------------------------------------------------------
// Words
Value mk_word_like(ObjectType type, String *str)
{
	String *w = string_clone(str);
	set_obj_type(w, type);
	return mk_ref(w);
}

Value mk_symbol(String *str)
{
	return mk_word_like(SYMBOL, str);
}

Value mk_word(String *str)
{
	return mk_word_like(WORD, str);
}

Value mk_word_cstr(char *str)
{
	String tmp;
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
// Printing values
static void print_continuation(FILE *stream, Continuation *k);

void print_string(FILE *stream, String *str)
{
	const char *ptr;

	fputc('\"', stream);
	for (ptr = str->b; ptr != str->e; ptr++)
		fputc(*ptr, stream);
	fputc('\"', stream);
}

static void print_array_like(FILE *stream, Array *a, char b, char e)
{
	unsigned i;

	fprintf(stream, "%c ", b);
	for (i = 0; i < a->nr_elts; i++) {
		print_value(stream, array_get(a, i));
		fprintf(stream, " ");
	}
	fprintf(stream, "%c", e);
}

static void print_array(FILE *stream, Array *a)
{
	print_array_like(stream, a, '{', '}');
}

static void print_quot(FILE *stream, Array *a)
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

static void print_word(FILE *stream, String *w)
{
	const char *ptr;

	for (ptr = w->b; ptr != w->e; ptr++)
		fputc(*ptr, stream);
}

static void print_namespace_entry(void *ctxt, String *k, Value v)
{
	FILE *stream = ctxt;
	fprintf(stream, "{ ");
	print_word(stream, k);
	fprintf(stream, " ");
	print_value(stream, v);
	fprintf(stream, " } ");
}

static void print_namespace(FILE *stream, struct namespace *ns)
{
	fprintf(stream, "H{ ");
	namespace_visit(ns, print_namespace_entry, stream);
	fprintf(stream, "}");
}

void print_value(FILE *stream, Value v)
{
	Header *h;

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
			print_namespace(stream, v.ptr);
			break;

		case NAMESPACE_ENTRY:
			fprintf(stream, "~namespace entry~");
			break;

		case PRIMITIVE:
			fprintf(stream, "~primitive~");
			break;

		case STRING:
			print_string(stream, (String *) v.ptr);
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
			print_quot(stream, (Array *) v.ptr);
			break;

		case ARRAY:
			print_array(stream, (Array *) v.ptr);
			break;

		case CODE_POSITION:
			fprintf(stream, "~code position~");
			break;

		case CONTINUATION:
			print_continuation(stream, v.ptr);
			break;

		case FIXNUM:
			fprintf(stream, "~boxed fixnum?!~");
			break;

		case NIL:
			fprintf(stream, "()");
			break;
		}
		break;

	case TAG_FALSE:
		fprintf(stream, "f");
		break;
	}
}

static void red(FILE *stream)
{
	fprintf(stream, "\x1b[31m");
}

static void green(FILE *stream)
{
	fprintf(stream, "\x1b[32m");
}

static void yellow(FILE *stream)
{
	fprintf(stream, "\x1b[33m");
}

static void white(FILE *stream)
{
	fprintf(stream, "\x1b[37m");
}

static void print_stack(FILE *stream, VM *vm, Array *a)
{
	unsigned i;

	fprintf(stream, "\n--- Data stack:\n");
	for (i = 0; i < a->nr_elts; i++) {
		print_value(stream, array_get(a, i));
		fprintf(stream, "\n");
	}
}

static void print_continuation(FILE *stream, Continuation *k)
{
	unsigned f, i;

	print_stack(stream, global_vm, k->data_stack);

	fprintf(stream, "\n--- Call stack:\n");
	for (f = 0; f < k->call_stack->nr_elts; f++) {
		CodePosition *cp = as_ref(array_get(k->call_stack, f));

		green(stream);
		for (i = 0; i < cp->code->nr_elts; i++) {
			if (cp->position == i)
				red(stream);

			print_value(stream, array_get(cp->code, i));

			if (cp->position == i)
				yellow(stream);

			fprintf(stream, " ");
		}
		white(stream);
		fprintf(stream, "\n");
	}
}

//----------------------------------------------------------------
// Lexer

static bool more_input(String *in)
{
	return in->b != in->e;
}

static void step_input(String *in)
{
	in->b++;
}

/*
 * space = whitespace | comment
 * whitespace = (' ' | <tab>)+
 * comment = '#' [^\n]*
 */
static void consume_space(String *in)
{
	while (more_input(in)) {
		if (*in->b == '#') {
			do {
				step_input(in);
			} while (more_input(in) && *in->b != '\n');

		} else if (isspace(*in->b)) {
			do {
				step_input(in);
			} while (more_input(in) && isspace(*in->b));
		} else
			return;
	}
}

static bool scan_fixnum(String *in, Token *result)
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

static bool scan_string(String *in, Token *result)
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

static bool is_sym_char(char c)
{
	static const char *special = "()';\\|";
	return !isspace(c) && !strchr(special, c);
}

static bool scan_sym(String *in, Token *result)
{
	result->type = TOK_SYM;
	result->str.b = in->b;

	while (more_input(in) && is_sym_char(*in->b))
		step_input(in);

	result->str.e = in->b;
	return true;
}

static bool mk_punc(String *in, Token *result, char c, TokenType t)
{
	if (*in->b != c)
		return false;

	result->type = t;
	result->str.b = in->b;
	step_input(in);
	result->str.e = in->b;
	return true;
}

static Token scan(String *in)
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

	else if (mk_punc(in, '(', TOK_OPEN, result) ||
		 mk_punc(in, ')', TOK_CLOSE, result) ||
		 mk_punc(in, '\'', TOK_QUOTE, result) ||
		 mk_punc(in, '.', TOK_DOT, result))
		return true;

	else
		return scan_sym(in, result);
}

typedef struct {
	String *in;
	Token tok;
} TokenStream;

static void stream_init(String *in, TokenStream *ts)
{
	ts->in = in;
	ts->tok = scan(in);
}

static Token *peek(TokenStream *ts)
{
	return &ts->tok;
}

static void shift(TokenStream *ts)
{
	ts->tok = scan(ts->in);
}

//----------------------------------------------------------------
// Read

// false indicates end of input, errors call the error handler and jump back
// into the repl.
static bool read_list(TokenStream *ts, Value *result);
static bool read_quote(TokenStream *ts, Value *result);

static bool read(TokenStream *ts, Value *result)
{
	Token *tok = peek(ts);

	if (tok->type == TOK_EOF)
		return false;

	switch (tok.type) {
	case TOK_FIXNUM:
		shift(ts);
		*result = mk_fixnum(tok->fixnum);
		break;

	case TOK_STRING:
		shift(ts);
		*result = mk_string(tok->str);
		break;

	case TOK_SYM:
		shift(ts);
		*result = mk_symbol(tok->str);
		break;

	case TOK_OPEN:
		shift(ts);
		return read_list(ts, result);

	case TOK_QUOTE:
		shift(ts);
		return read_quote(ts, result);

	default:
		error("unexpected token");
	}

	return true;
}

static bool read_list(TokenStream *ts, Value *result)
{
	Token *tok = peek(ts);

	ListBuilder lb;
	lb_init(&lb);

	for (;;) {
		if (tok->type == TOK_DOT) {
			// FIXME: finish
			error("lazy programmer hasn't implemented dotted lists");

		} else if (tok->type == TOK_CLOSE) {
			shift(ts);
			*result = lb_get(&lb);
			return true;

		} else {
			bool r = read(ts, result);
			if (!read(ts, result))
				error("malformed list; unexpected eof");
			lb_append(&lb, *result);
		}
	}

	// Can't get here.
	return false;
}

static bool read_quote(TokenStream *ts, Value *result)
{
	ListBuilder lb;

	lb_init(&lb);
	lb_append(&lb, mk_symbol("quote"));

	if (!read(in, result))
		error("malformed quote; unexpected eof");

	lb_append(&lb, *result);
	*result = lb_get(&lb);
	return true;
}

//----------------------------------------------------------------
// Eval

#if 0
struct primitive {
	PrimFn fn;
};

struct dynamic_scope {
	struct list_head list;
	struct list_head definitions;
};

static void init_continuation(Continuation *k)
{
	k->data_stack = array_create();
	k->call_stack = array_create();
	k->catch_stack = array_create();
}

static void init_vm(VM *vm)
{
	memset(vm, 0, sizeof(*vm));
	vm->current_ns = namespace_create(NULL);
	vm->k = zalloc(CONTINUATION, sizeof(*vm->k));
	init_continuation(vm->k);
}

void def_primitive(VM *vm, char *name, PrimFn fn)
{
	struct primitive *p = alloc(PRIMITIVE, sizeof(*p));
	String k;

	string_tmp(name, &k);
	p->fn = fn;
	namespace_insert(vm->current_ns, &k, mk_ref(p));
}

static void def_word(String *w, Array *body)
{
	namespace_insert(global_vm->current_ns, w, mk_ref(body));
}

void push_call(Array *code)
{
	CodePosition *pc = zalloc(CODE_POSITION, sizeof(*pc));
	pc->code = code;
	pc->position = 0;
	global_vm->k->call_stack = array_push(global_vm->k->call_stack, mk_ref(pc));
}

void inc_pc(void)
{
	Array *s = global_vm->k->call_stack;

	if (s->nr_elts) {
		CodePosition *pc = as_type(CODE_POSITION, array_peek(s));
		if (++pc->position == pc->code->nr_elts)
			array_pop(global_vm->k->call_stack);
	}
}

static void eval_value(Value v)
{
	struct primitive *p;
	String *w;
	Header *h;
	Value def;

	switch (get_tag(v)) {
	case TAG_FIXNUM:
	case TAG_FALSE:
		PUSH(v);
		inc_pc();
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
		case FALSE_TYPE:
			PUSH(v);
			inc_pc();
			break;

		case WORD:
			w = as_ref(v);
			if (namespace_lookup(global_vm->current_ns, w, &def)) {
				switch (get_type(def)) {
				case PRIMITIVE:
					/*
					 * Primitives are responsible for
					 * incrementing the pc themselves
					 */
					p = as_ref(def);
					p->fn();
					break;

				case QUOT:
					inc_pc();
					push_call(as_ref(def));
					break;

				default:
					error("bad def");
				}

			} else {
				print_string(stderr, w);
				error("couldn't lookup word");
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

static bool more_code(void)
{
	return global_vm->k->call_stack->nr_elts > 0;
}

VM *global_vm = NULL;

Continuation *cc(VM *vm)
{
	Continuation *k = alloc(CONTINUATION, sizeof(*k));

	// Shallow copy only, users need to be aware of this
	k->data_stack = clone(vm->k->data_stack);

	// Deep copy is crucial, otherwise the code_positions will change
	k->call_stack = array_deep_clone(vm->k->call_stack);

	// FIXME: I'm worried we need deeper copy
	k->catch_stack = array_deep_clone(vm->k->catch_stack);

	return k;
}

void eval(VM *vm, Array *code)
{
	int r;
	Value v;
	CodePosition *pc;

	if (!code->nr_elts)
		return;

	// We push a continuation on the exception stack before the code
	// gets pushed.
	vm->k->catch_stack = array_push(vm->k->catch_stack, mk_ref(cc(vm)));
	push_call(code);
	do
		r = setjmp(vm->eval_loop);
	while (r);
	vm->handling_error = false;

	while (more_code()) {
		Array *s = vm->k->call_stack;
		pc = as_type(CODE_POSITION, array_peek(s));
		v = array_get(pc->code, pc->position);
		eval_value(v);
	}
}
#endif

static Value eval(Value sexp)
{
	return sexp;
}

//----------------------------------------------------------------
// Loop

static void throw(void)
{
	if (global_vm->handling_error) {
		fprintf(stderr, "error whilst handling error, aborting");
		abort();
	}

	if (global_vm->k->catch_stack->nr_elts) {
		global_vm->handling_error = true;
		fprintf(stderr, "jumping back to eval loop");
		global_vm->k = as_ref(array_pop(global_vm->k->catch_stack));
		longjmp(global_vm->eval_loop, -1);

	} else {
		// This shouldn't happen
		fprintf(stderr, "no error handler, exiting\n");
		exit(1);
	}
}

void error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	print_continuation(stderr, global_vm->k);
	throw();
}

static void load_file(VM *vm, const char *path)
{
	int fd;
	struct stat info;
	String input;
	Array *code;

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

// Read a string, and return a pointer to it.  Returns NULL on EOF.
const char *rl_gets()
{
	static char *line_read = NULL;

	/* If the buffer has already been allocated,
	   return the memory to the free pool. */
	if (line_read) {
		free(line_read);
		line_read = (char *)NULL;
	}

	line_read = readline("dmexec> ");

	if (line_read && *line_read)
		add_history(line_read);

	return line_read;
}

static int repl(VM *vm)
{
	const char *buffer;
	String input;

	global_vm = vm;
	for (;;) {
		buffer = rl_gets();
		if (!buffer)
			break;

		input.b = buffer;
		input.e = buffer + strlen(buffer);
		stream_init(&stream, &input);
		while (read(ts, &v))
			print(eval(vm, v));
	}

	return 0;
}

int main(int argc, char **argv)
{
	unsigned i;
	VM vm;

	GC_INIT();
	init_vm(&vm);
	def_basic_primitives(&vm);
	def_dm_primitives(&vm);

	//load_file(&vm, "prelude.dm");

	if (argc > 1)
		for (i = 1; i < argc; i++)
			load_file(&vm, argv[i]);
	else
		repl(&vm);

	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) get_memory_stats()->total_allocated);
	printf("heap size: %llu\n",
	       (unsigned long long) GC_get_heap_size());

	return 0;
}
