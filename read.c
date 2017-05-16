#include "vm.h"

#include <ctype.h>
#include <string.h>

//----------------------------------------------------------------
// Lexer

static bool more_input(String *in)
{
	if (in->e < in->b)
		error("input overrun\n");

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

	if (!is_sym_char(*in->b))
		error("this isn't a symbol");

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

void stream_init(String *in, TokenStream *ts)
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

bool read_sexp(TokenStream *ts, Value *result)
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
		*result = mk_ref(mk_string(STRING, tok->str.b, tok->str.e));
		shift(ts);
		break;

	case TOK_SYM:
		*result = mk_ref(mk_string(SYMBOL, tok->str.b, tok->str.e));
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
			Value result2;
			if (!read_sexp(ts, &result2))
				error("malformed list; unexpected eof");
			lb_append(&lb, result2);
		}
	}

	// Can't get here.
	return false;
}

static bool read_quote(TokenStream *ts, Value *result)
{
	Value result2;
	ListBuilder lb;

	lb_init(&lb);
	lb_append(&lb, mk_ref(mk_string_from_cstr(SYMBOL, "quote")));

	if (!read_sexp(ts, &result2))
		error("malformed quote; unexpected eof");

	lb_append(&lb, result2);
	*result = lb_get(&lb);
	return true;
}

