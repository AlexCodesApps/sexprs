#include "sexprs.h"
#include <assert.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

static int peek(SExprStream stream) { return stream.vtable->peek(stream.ctx); }

static int next(SExprStream stream) { return stream.vtable->next(stream.ctx); }

typedef enum {
	T_LPAREN,
	T_RPAREN,
	T_INT,
	T_FLOAT,
	T_SYM,
	T_STR,
	T_NIL,
	T_EOF,
	T_ERR,
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

static int lex_ws(SExprStream s) {
	int c;
	while ((c = peek(s)) != EOF) {
		switch (c) {
		case ';':
			while ((c = peek(s)) != EOF && c != '\n')
				next(s);
			continue;
		case ' ':
		case '\n':
		case '\r':
		case '\t':
			next(s);
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

static SExprParseResult lex_str(SExprParseOptions * opts, char ** out) {
	SExprStream stream = opts->stream;
	SExprAllocator alloc = opts->allocator;
	size_t size = 0;
	size_t cap = 4;
	char * buffer = realloc_buffer(alloc, NULL, 0, cap);
	next(stream);
	char c;
	while ((c = peek(stream)) != '"' && c != EOF) {
		next(stream);
		if (c == '\\') {
			switch (peek(stream)) {
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
			next(stream);
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
	next(stream);
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

static SExprParseResult lex_sym_or_number(SExprParseOptions * opts, char ** out,
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
		buff[size++] = next(s);
	} while (!xis_not_part_of_sym(peek(s)));
	buff[size] = '\0';
	*out = buff;
	*out_size = size;
	*out_cap = cap;
	return SEXPR_PARSE_OK;
}

static Token lex_token(SExprParseOptions * opts) {
	SExprStream s = opts->stream;
	SExprAllocator alloc = opts->allocator;
	int c = lex_ws(s);
	if (c == EOF) {
		next(s);
		return token(T_EOF);
	}
	if (c == '(') {
		next(s);
		return token(T_LPAREN);
	}
	if (c == ')') {
		next(s);
		return token(T_RPAREN);
	}
	if (c == '"') {
		char * s;
		SExprParseResult r = opts->lex_str(opts, &s);
		if (r != SEXPR_PARSE_OK)
			return token_err(r);
		Token t = token(T_STR);
		t.as.str = s;
		return t;
	}
	char * blob;
	size_t bsize;
	size_t bcap;
	SExprParseResult r = lex_sym_or_number(opts, &blob, &bsize, &bcap);
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

void sexpr_free(SExpr expr, SExprAllocator alloc) {
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
		sexpr_free(cons->car, alloc);
		sexpr_free(cons->cdr, alloc);
		alloc.vtable->free_cons(alloc.ctx, sexpr_as_cons(expr));
		break;
	}
	case SEXPR_SYMBOL:
		alloc.vtable->free_symbol(alloc.ctx, sexpr_as_symbol(expr));
		break;
	}
}

SExprParseResult parse_sexpr(SExprParseOptions * opts, size_t level, Token t,
							 SExpr * out);

SExprParseResult parse_rest_of_list(SExprParseOptions * opts, size_t level,
									SExpr * out) {
	SExprAllocator alloc = opts->allocator;
	Token t = lex_token(opts);
	if (t.type == T_RPAREN) {
		*out = NIL_SEXPR;
		return SEXPR_PARSE_OK;
	}
	SExprCons cons;
	SExprParseResult r = parse_sexpr(opts, level, t, &cons.car);
	if (r != SEXPR_PARSE_OK)
		return r;
	r = parse_rest_of_list(opts, level, &cons.cdr);
	if (r != SEXPR_PARSE_OK) {
		sexpr_free(cons.car, alloc);
		return r;
	}
	SExprCons * consp = alloc.vtable->allocate_cons(alloc.ctx, cons);
	if (!consp) {
		sexpr_free(cons.car, alloc);
		sexpr_free(cons.cdr, alloc);
		return SEXPR_PARSE_OOM;
	}
	*out = cons_as_sexpr(consp);
	return SEXPR_PARSE_OK;
}

SExprParseResult parse_sexpr(SExprParseOptions * opts, size_t level, Token t,
							 SExpr * out) {
	if (opts->nest_limit && level >= opts->nest_limit)
		return SEXPR_PARSE_NEST_LIMIT_EXCEEDED;
	switch (t.type) {
	case T_LPAREN:
		return parse_rest_of_list(opts, level + 1, out);
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
	if (opts->lex_str == NULL)
		opts->lex_str = lex_str;
	return parse_sexpr(opts, 0, lex_token(opts), out);
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

static int buffer_stream_peek(void * ctx) {
	SExprBuffer * buffer = ctx;
	if (buffer->nleft == 0)
		return EOF;
	return *buffer->ptr;
}

static int buffer_stream_next(void * ctx) {
	SExprBuffer * buffer = ctx;
	if (buffer->nleft == 0)
		return EOF;
	char c = *buffer->ptr++;
	--buffer->nleft;
	return c;
}

const static SExprStreamVTable buffer_stream_vtable = {
	buffer_stream_peek,
	buffer_stream_next,
};

SExprStream sexpr_buffer_stream(SExprBuffer * buffer) {
	return (SExprStream){buffer, &buffer_stream_vtable};
}

static int FILE_stream_peek(void * ctx) {
	char c = fgetc(ctx);
	ungetc(c, ctx);
	return c;
}

static int FILE_stream_next(void * ctx) { return fgetc(ctx); }

static const SExprStreamVTable FILE_stream_vtable = {
	FILE_stream_peek,
	FILE_stream_next,
};

SExprStream sexpr_FILE_stream(FILE * file) {
	return (SExprStream){file, &FILE_stream_vtable};
}
