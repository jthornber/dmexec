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
 * comment = ';' [^\n]*
 */
static void consume_space(String *in)
{
	while (more_input(in)) {
		if (*in->b == ';') {
			do {
				step_input(in);
			} while (more_input(in) && *in->b != '\n');

		} else if (isspace(*in->b)) {
			do {
				step_input(in);
			} while (more_input(in) && isspace(*in->b));
		} else
			break;
	}
}

static bool is_sym_char(char c)
{
	static const char *special = "()';\\|";
	return !isspace(c) && !strchr(special, c);
}

static Token scan_sym(String *in)
{
	Token tok;
	tok.type = TOK_SYM;
	tok.str.b = in->b;

	while (more_input(in) && is_sym_char(*in->b))
		step_input(in);

	tok.str.e = in->b;
	return tok;
}

// This may return a symbol
static Token scan_fixnum(String *in)
{
	int n = 0;
	Token tok;

	tok.type = TOK_FIXNUM;

	tok.str.b = in->b;
	while (more_input(in) && isdigit(*in->b)) {
		n *= 10;
		n += *in->b - '0';
		step_input(in);
	}
	tok.str.e = in->b;
	tok.fixnum = n;

	if (more_input(in) && is_sym_char(*in->b)) {
		// Symbols may begin with digits.
		while (more_input(in) && is_sym_char(*in->b))
			step_input(in);

		tok.str.e = in->b;
		tok.type = TOK_SYM;
	}

	return tok;
}

static Token scan_string(String *in)
{
	Token tok;

	tok.type = TOK_STRING;
	step_input(in);
	tok.str.b = in->b;

	// FIXME: support escapes
	while (more_input(in) && *in->b != '\"')
		step_input(in);

	tok.str.e = in->b;

	if (!more_input(in))
		error("bad string");

	step_input(in);
	return tok;
}

static bool is_punc(char c, TokenType *result)
{
	switch (c) {
	case '(':
		*result = TOK_OPEN;
		return true;

	case ')':
		*result = TOK_CLOSE;
		return true;

	case '\'':
		*result = TOK_QUOTE;
		return true;

	case '.':
		*result = TOK_DOT;
		return true;

	default:
		return false;
	}

	return false;
}

static Token scan(String *in)
{
	Token r;
	TokenType tt;

	if (!more_input(in))
		return (Token) {TOK_EOF};

	consume_space(in);

	if (!more_input(in))
		return (Token) {TOK_EOF};

	if (isdigit(*in->b))
		return scan_fixnum(in);

	else if (*in->b == '\"')
		return scan_string(in);

	else if (is_punc(*in->b, &tt)) {
		r.type = tt;
		r.str.b = in->b;
		step_input(in);
		r.str.e = in->b;
		return r;

	} else
		return scan_sym(in);
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

// FIXME: remove this
static Symbol *symbol_root = NULL;

static bool _read(TokenStream *ts, Value *result)
{
	Token *tok = peek(ts);

	if (tok->type == TOK_EOF)
		return false;

	switch (tok->type) {
	case TOK_FIXNUM:
		*result = mk_fixnum(tok->fixnum);
		shift(ts);
		break;

	case TOK_STRING:
		*result = mk_ref(string_clone(&tok->str));
		shift(ts);
		break;

	case TOK_SYM:
		*result = mk_symbol(&symbol_root, &tok->str);
		shift(ts);
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
			error("lazy programmer; no dotted lists");

		} else if (tok->type == TOK_CLOSE) {
			shift(ts);
			*result = lb_get(&lb);
			return true;

		} else {
			if (!_read(ts, result))
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
	lb_append(&lb, mk_symbol(&symbol_root, string_clone_cstr("quote")));

	if (!_read(ts, result))
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

static Value eval(VM *vm, Value sexp)
{
	return sexp;
}

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

static void print(FILE *stream, Value v)
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

		case NIL:
			fprintf(stream, "()");
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

#if 0
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
#endif
//----------------------------------------------------------------
// Loop

static void throw(void)
{
#if 0
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
#endif
	exit(1);
}

void error(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n");

	throw();
}
#if 0
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
#endif
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
	TokenStream stream;
	String input;
	Value v;

	//global_vm = vm;
	for (;;) {
		buffer = rl_gets();
		if (!buffer)
			break;

		input.b = buffer;
		input.e = buffer + strlen(buffer);
		stream_init(&input, &stream);
		while (_read(&stream, &v)) {
			printf("=> ");
			print(stdout, eval(vm, v));
			printf("\n");
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	unsigned i;
	VM vm;

	GC_INIT();
//	init_vm(&vm);
//	def_basic_primitives(&vm);
//	def_dm_primitives(&vm);

	//load_file(&vm, "prelude.dm");
#if 0
	if (argc > 1)
		for (i = 1; i < argc; i++)
			load_file(&vm, argv[i]);
	else
#endif
	repl(&vm);

	printf("\n\ntotal allocated: %llu\n",
	       (unsigned long long) get_memory_stats()->total_allocated);
	printf("heap size: %llu\n",
	       (unsigned long long) GC_get_heap_size());

	return 0;
}
