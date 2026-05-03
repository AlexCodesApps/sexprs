#include "sexprs.h"
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#define STREAM_BUFFER_SIZE (8192)

typedef struct {
	char * cursor;
	char * end;
	char inner[STREAM_BUFFER_SIZE];
} StreamBuffer;

static int stream_peek(StreamBuffer * buffer, SExprStream stream) {
	if (buffer->cursor != buffer->end) {
		return *buffer->cursor;
	}
	int nbytes = stream.vtable->read(stream.ctx, buffer->inner, STREAM_BUFFER_SIZE);
	if (nbytes == 0)
		return EOF;
	buffer->cursor = buffer->inner;
	buffer->end = buffer->inner + nbytes;
	return *buffer->cursor;
}

static int stream_next(StreamBuffer * buffer, SExprStream stream) {
	if (buffer->cursor != buffer->end) {
		return *(buffer->cursor++);
	}
	int nbytes = stream.vtable->read(stream.ctx, buffer->inner, STREAM_BUFFER_SIZE);
	if (nbytes == 0)
		return EOF;
	buffer->cursor = buffer->inner;
	buffer->end = buffer->inner + nbytes;
	return *(buffer->cursor++);
}

typedef enum {
	T_LPAREN,
	T_RPAREN,
	T_INT,
	T_FLOAT,
	T_SYM,
	T_STR,
	T_NIL,
	T_APOSTROPHE,
	T_EOF,
	T_ERR
} TokenType;

typedef struct {
	TokenType type;
	union {
		double f;
		int i;
		char * sym;
		char * str;
		SExprParseResult err;
	} as;
} Token;

static Token token(TokenType t) { return (Token){t}; }

static Token token_err(SExprParseResult r) {
	Token t = token(T_ERR);
	t.as.err = r;
	return t;
}

static int lex_ws(StreamBuffer * b, SExprStream s) {
	int c;
	while ((c = stream_peek(b, s)) != EOF) {
		switch (c) {
		case ';':
			while ((c = stream_peek(b, s)) != EOF && c != '\n')
				stream_next(b, s);
			continue;
		case ' ':
		case '\n':
		case '\r':
		case '\t':
			stream_next(b, s);
			continue;
		default:
			return c;
		}
	}
	return c;
}

static char * realloc_buffer(SExprAllocator alloc, void * buf, size_t old,
							 size_t new) {
	return alloc.vtable->realloc_buffer(alloc.ctx, buf, old, new);
}

static void free_buffer(SExprAllocator alloc, void * buf, size_t old) {
	realloc_buffer(alloc, buf, old, 0);
}

static SExprParseResult lex_str(StreamBuffer * b, SExprParseOptions * opts, char ** out) {
	SExprStream stream = opts->stream;
	SExprAllocator alloc = opts->allocator;
	size_t size = 0;
	size_t cap = 4;
	char * buffer = realloc_buffer(alloc, NULL, 0, cap);
	stream_next(b, stream);
	char c;
	while ((c = stream_peek(b, stream)) != '"' && c != EOF) {
		stream_next(b, stream);
		if (c == '\\') {
			switch (stream_peek(b, stream)) {
			case 'n':
				c = '\n';
				break;
			case '\t':
				c = '\t';
				break;
			case '\r':
				c = '\r';
				break;
			default:
				realloc_buffer(alloc, buffer, cap, 0);
				return SEXPR_PARSE_UNEXPECTED_CHAR;
			}
			stream_next(b, stream);
		}
		if (size == cap - 1) {
			size_t ncap = cap * 2;
			char * nbuffer = realloc_buffer(alloc, buffer, cap, ncap);
			if (!nbuffer) {
				realloc_buffer(alloc, buffer, cap, 0);
				return SEXPR_PARSE_OOM;
			}
			buffer = nbuffer;
			cap = ncap;
		}
		buffer[size++] = c;
	}
	if (c == EOF) {
		realloc_buffer(alloc, buffer, cap, 0);
		return SEXPR_UNTERM_STRING;
	}
	stream_next(b, stream);
	buffer[size] = '\0';
	char * str = alloc.vtable->buffer_to_string(alloc.ctx, buffer);
	if (!str) {
		realloc_buffer(alloc, buffer, cap, 0);
		return SEXPR_PARSE_OOM;
	}
	*out = str;
	return SEXPR_PARSE_OK;
}

typedef struct {
	SExprParseOptions * opts;
	size_t nest_level;
} ParserState;

bool xisdigit(int c) { return '0' <= c && c <= '9'; }

#define MAX_DOUBLE_DIGITS (3 + DBL_MANT_DIG - DBL_MIN_EXP)

int xis_not_part_of_sym(char c) {
	switch (c) {
	case ' ':
	case '\t':
	case '\r':
	case '\n':
	case '(':
	case ')':
	case '"':
	case ',':
	case '\'':
		return true;
	default:
		return false;
	}
}

static SExprParseResult lex_sym_or_number(StreamBuffer * b, SExprParseOptions * opts, char ** out,
										  size_t * out_size, size_t * out_cap) {
	SExprStream s = opts->stream;
	SExprAllocator alloc = opts->allocator;
	size_t size = 0;
	size_t cap = 4;
	char * buff = realloc_buffer(alloc, NULL, 0, cap);
	if (!buff)
		return SEXPR_PARSE_OOM;
	do {
		if (size == cap - 1) {
			size_t new_cap = cap * 2;
			char * nbuff = realloc_buffer(alloc, buff, cap, new_cap);
			if (!nbuff) {
				realloc_buffer(alloc, buff, cap, 0);
				return SEXPR_PARSE_OOM;
			}
			cap = new_cap;
			buff = nbuff;
		}
		buff[size++] = stream_next(b, s);
	} while (!xis_not_part_of_sym(stream_peek(b, s)));
	buff[size] = '\0';
	*out = buff;
	*out_size = size;
	*out_cap = cap;
	return SEXPR_PARSE_OK;
}

static Token lex_token(StreamBuffer * b, SExprParseOptions * opts) {
	SExprStream s = opts->stream;
	SExprAllocator alloc = opts->allocator;
	int c = lex_ws(b, s);
	if (c == EOF) {
		stream_next(b, s);
		return token(T_EOF);
	}
	if (c == '(') {
		stream_next(b, s);
		return token(T_LPAREN);
	}
	if (c == ')') {
		stream_next(b, s);
		return token(T_RPAREN);
	}
	if (c == '\'' && opts->enable_quote_sym) {
		stream_next(b, s);
		return token(T_APOSTROPHE);
	}
	if (c == '"') {
		char * s;
		SExprParseResult r = lex_str(b, opts, &s);
		if (r != SEXPR_PARSE_OK)
			return token_err(r);
		Token t = token(T_STR);
		t.as.str = s;
		return t;
	}
	char * blob;
	size_t bsize;
	size_t bcap;
	SExprParseResult r = lex_sym_or_number(b, opts, &blob, &bsize, &bcap);
	if (r != SEXPR_PARSE_OK)
		return token_err(r);
	if (opts->nil_keyword && strcmp(opts->nil_keyword, blob) == 0) {
		free_buffer(alloc, blob, bcap);
		return token(T_NIL);
	}
	char * end;
	{
		errno = 0;
		long i = strtol(blob, &end, 10);
		if (end == blob + bsize) {
			free_buffer(alloc, blob, bcap);
			if (errno || i > INT_MAX) {
				return token_err(SEXPR_PARSE_OVERFLOW);
			}
			Token t = token(T_INT);
			t.as.i = i;
			return t;
		}
	}
	{
		errno = 0;
		double d = strtod(blob, &end);
		if (end == blob + bsize) {
			free_buffer(alloc, blob, bcap);
			if (errno)
				return token_err(SEXPR_PARSE_OVERFLOW);
			Token t = token(T_FLOAT);
			t.as.f = d;
			return t;
		}
	}
	char * sym = alloc.vtable->buffer_to_symbol(alloc.ctx, blob);
	if (!sym) {
		free_buffer(alloc, blob, bcap);
		return token_err(SEXPR_PARSE_OOM);
	}
	Token t = token(T_SYM);
	t.as.sym = sym;
	return t;
}

void sexpr_free(SExpr expr, SExprParseOptions * opts) {
	SExprAllocator alloc = opts->allocator;
	switch (sexpr_type(expr)) {
	case SEXPR_NIL:
	case SEXPR_INT:
	case SEXPR_FLOAT:
		break;
	case SEXPR_STRING:
		alloc.vtable->free_string(alloc.ctx, sexpr_as_string(expr));
		break;
	case SEXPR_CONS: {
		SExprCons * cons = sexpr_as_cons(expr);
		sexpr_free(cons->car, opts);
		sexpr_free(cons->cdr, opts);
		alloc.vtable->free_cons(alloc.ctx, sexpr_as_cons(expr));
		break;
	}
	case SEXPR_SYMBOL: {
		char * sym = sexpr_as_symbol(expr);
		if (sym != opts->enable_quote_sym)
			alloc.vtable->free_symbol(alloc.ctx, sym);
		break;
	}
	}
}

SExprParseResult parse_sexpr(StreamBuffer * buffer, SExprParseOptions * opts, size_t level, Token t,
							 SExpr * out);

SExprParseResult parse_rest_of_list(StreamBuffer * buffer, SExprParseOptions * opts, size_t level,
									SExpr * out) {
	SExprAllocator alloc = opts->allocator;
	Token t = lex_token(buffer, opts);
	if (t.type == T_RPAREN) {
		*out = NIL_SEXPR;
		return SEXPR_PARSE_OK;
	}
	SExprCons cons;
	SExprParseResult r = parse_sexpr(buffer, opts, level, t, &cons.car);
	if (r != SEXPR_PARSE_OK)
		return r;
	r = parse_rest_of_list(buffer, opts, level, &cons.cdr);
	if (r != SEXPR_PARSE_OK) {
		sexpr_free(cons.car, opts);
		return r;
	}
	SExprCons * consp = alloc.vtable->allocate_cons(alloc.ctx, cons);
	if (!consp) {
		sexpr_free(cons.car, opts);
		sexpr_free(cons.cdr, opts);
		return SEXPR_PARSE_OOM;
	}
	*out = cons_as_sexpr(consp);
	return SEXPR_PARSE_OK;
}

SExprParseResult parse_quote_sexpr(StreamBuffer * buffer, SExprParseOptions * opts, size_t level, SExpr * out) {
	SExpr expr;
	SExprParseResult r = parse_sexpr(buffer, opts, level, lex_token(buffer, opts), &expr);
	if (r != SEXPR_PARSE_OK)
		return r;
	SExprAllocator alloc = opts->allocator;
	SExprCons cons = { expr, NIL_SEXPR };
	SExprCons * tail = alloc.vtable->allocate_cons(alloc.ctx, cons);
	if (!tail) {
		sexpr_free(expr, opts);
		return SEXPR_PARSE_OOM;
	}
	cons = (SExprCons){ symbol_as_sexpr(opts->enable_quote_sym), cons_as_sexpr(tail) };
	SExprCons * head = alloc.vtable->allocate_cons(alloc.ctx, cons);
	if (!head) {
		alloc.vtable->free_cons(alloc.ctx, tail);
		sexpr_free(expr, opts);
		return SEXPR_PARSE_OOM;
	}
	*out = cons_as_sexpr(head);
	return SEXPR_PARSE_OK;
}

SExprParseResult parse_sexpr(StreamBuffer * buffer, SExprParseOptions * opts, size_t level, Token t,
							 SExpr * out) {
	if (opts->nest_limit && level >= opts->nest_limit)
		return SEXPR_PARSE_NEST_LIMIT_EXCEEDED;
	switch (t.type) {
	case T_LPAREN:
		return parse_rest_of_list(buffer, opts, level + 1, out);
	case T_RPAREN:
		return SEXPR_PARSE_UNEXPECTED_TOKEN;
	case T_INT:
		*out = int_as_sexpr(t.as.i);
		break;
	case T_FLOAT:
		*out = float_as_sexpr(t.as.f);
		break;
	case T_SYM:
		*out = symbol_as_sexpr(t.as.sym);
		break;
	case T_STR:
		*out = string_as_sexpr(t.as.str);
		break;
	case T_NIL:
		*out = NIL_SEXPR;
		break;
	case T_APOSTROPHE:
		return parse_quote_sexpr(buffer, opts, level + 1, out);
	case T_EOF:
		return SEXPR_PARSE_UNEXPECTED_EOF;
	case T_ERR:
		return t.as.err;
	}
	return SEXPR_PARSE_OK;
}

SExprParseResult sexpr_parse(SExprParseOptions * opts, SExpr * out) {
	if (opts->allocator.vtable == NULL)
		opts->allocator = sexpr_default_allocator();
	if (opts->stream.vtable == NULL)
		opts->stream = sexpr_FILE_stream(stdout);
	StreamBuffer buf;
	buf.cursor = buf.inner;
	buf.end = buf.inner;
	return parse_sexpr(&buf, opts, 0, lex_token(&buf, opts), out);
}

static char * realloc8(void * _, char * in, size_t _2, size_t size) {
	return realloc(in, size);
}

static char * return_str(void * ctx, char * in) { return in; }

static SExprCons * malloc_cons8(void * ctx, SExprCons cons) {
	SExprCons * out = malloc(sizeof(SExprCons));
	if (!out)
		return out;
	*out = cons;
	return out;
}

static void free_str(void * _, char * str) { free(str); }

static void free_cons(void * _, SExprCons * cons) { free(cons); }

static const SExprAllocatorVTable default_allocator_vtable = {
	realloc8, return_str, return_str, malloc_cons8, free_str, free_str, free_cons};

SExprAllocator sexpr_default_allocator(void) {
	return (SExprAllocator){NULL, &default_allocator_vtable};
}

static int buffer_stream_read(void * ctx, void * data, int size) {
	SExprBuffer * buffer = ctx;
	if (buffer->nleft == 0)
		return 0;
	ptrdiff_t towrite = buffer->nleft < size ? buffer->nleft : size;
	memcpy(data, buffer->ptr, towrite);
	buffer->nleft -= towrite;
	buffer->ptr += towrite;
	return towrite;
}

const static SExprStreamVTable buffer_stream_vtable = {
	buffer_stream_read,
};

SExprStream sexpr_buffer_stream(SExprBuffer * buffer) {
	return (SExprStream){buffer, &buffer_stream_vtable};
}

static int FILE_stream_read(void * ctx, void * data, int size) {
	return fread(data, 1, size, ctx);
}

static const SExprStreamVTable FILE_stream_vtable = {
	FILE_stream_read,
};

SExprStream sexpr_FILE_stream(FILE * file) {
	return (SExprStream){file, &FILE_stream_vtable};
}
