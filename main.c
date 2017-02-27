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
		while (read_sexp(&stream, &v)) {
			print(stdout, eval(vm, v));
			printf("\n");
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	VM vm;

	mm_init(64 * 1024 * 1024);
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
	mm_exit();

	return 0;
}
