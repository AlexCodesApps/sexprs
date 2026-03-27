#include "sexprs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

static ptrdiff_t read_s(SExprStream stream, void * out, ptrdiff_t size) {
	return stream.read(stream.ctx, out, size);
}

static int getchar_s(SExprStream stream) {
	int c;
	ptrdiff_t r = read_s(stream, &c, 1);
	if (r <= 0)
		return EOF;
	return c;
}

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

static Token token(TokenType t) {
	return (Token) { t };
}

static Token token_err(SExprParseResult r) {
	Token t = token(T_ERR);
	t.as.err = r;
	return t;
}

static int lex_ws(SExprStream s) {
	int c;
	while ((c = getchar_s(s)) != EOF) {
		switch (c) {
		case ';':
			while ((c = getchar_s(s)) != EOF && c != '\n')
				continue;
		case ' ':
		case '\n':
		case '\r':
		case '\t':
			continue;
		default:
			return c;
		}
	}
	return c;
}

static SExprParseResult lex_str(SExprParseOptions * opts, char ** out) {
	assert(false && "TODO");
}

static Token lex_token(SExprParseOptions * opts) {
	SExprStream stream = opts->stream;
	int c = lex_ws(stream);
	if (c == EOF)
		return token(T_EOF);
	if (c == '(')
		return token(T_LPAREN);
	if (c == ')')
		return token(T_RPAREN);
	if (c == '"') {
		char * s;
		SExprParseResult r = opts->lex_str(opts, &s);
		if (r != SEXPR_PARSE_OK)
			return token_err(r);
	}
	int (* is_tok)(char) = opts->is_token_c;
	if (is_tok(c)) {
		char tk[256];
		int tklen = 1;
		tk[0] = c;
		while (is_tok(c = getchar_s(stream))) {
			if (tklen == 255)
				return token_err(SEXPR_PARSE_OVERFLOW);
			tk[tklen++] = c;
		}
		tk[tklen] = '\0';
		if (opts->nil_keyword && strcmp(tk, opts->nil_keyword) == 0)
			return token(T_NIL);
		char * s = opts->allocator.vtable->allocate_symbol(opts->allocator.ctx, tk);
		if (!s)
			return token_err(SEXPR_PARSE_OOM);
		Token t = token(T_SYM);
		t.as.sym = s;
		return t;
	}
	return token_err(SEXPR_PARSE_UNEXPECTED_CHAR);
}

static int is_token_c(char c) {
	return ('a' <= c && c <= 'z')
		|| ('A' <= c && c <= 'Z')
		|| ('0' <= c && c <= '9')
		|| c == '_'
		|| c == '-';
}

SExprParseResult sexpr_parse(SExprParseOptions opts, SExpr * out) {
	if (opts.allocator.vtable == NULL)
		opts.allocator = sexpr_default_allocator();
	if (opts.stream.read == NULL)
		opts.stream = sexpr_FILE_stream(stdout);
	if (opts.is_token_c == NULL)
		opts.is_token_c = is_token_c;
	if (opts.lex_str == NULL)
		opts.lex_str = lex_str;
	assert(false && "TODO");
}

static void * malloc8(void * _, size_t size) {
	return malloc(size);
}

static char * realloc8(void * _, char * in, size_t _2, size_t size) {
	return realloc(in, size);
}

static char * strdup8(void * ctx, char * in) {
	return in;
}

static SExprCons * malloc_cons8(void * ctx, SExprCons cons) {
	SExprCons * out = aligned_alloc(8, sizeof(SExprCons));
	if (!out) return out;
	*out = cons;
	return out;
}

static void free_str(void * _, char * str) {
	free(str);
}

static void free_cons(void * _, SExprCons * cons) {
	free(cons);
}

static const SExprAllocatorVTable default_allocator_vtable = {
	realloc8,
	strdup8,
	strdup8,
	malloc_cons8,
	free_str,
	free_str,
	free_cons
};

SExprAllocator sexpr_default_allocator(void) {
	return (SExprAllocator) {
		NULL,
		&default_allocator_vtable
	};
}

static ptrdiff_t buffer_stream_read(void * ctx, void * out, ptrdiff_t size) {
	SExprBuffer * buffer = ctx;
	ptrdiff_t toread = size;
	if (buffer->nleft < size) {
		toread = buffer->nleft;
		buffer->nleft = 0;
	} else {
		buffer->nleft -= size;
	}
	memcpy(out, buffer->ptr, toread);
	buffer->ptr += toread;
	return toread;
}

SExprStream sexpr_buffer_stream(SExprBuffer * buffer) {
	return (SExprStream) {
		buffer,
		buffer_stream_read
	};
}

static ptrdiff_t FILE_stream_read(void * ctx, void * out, ptrdiff_t size) {
	FILE * file = ctx;
	size_t r = fread(out, 1, size, file);
	if (r == 0 && ferror(file))
		return -1;
	return r;
}

SExprStream sexpr_FILE_stream(FILE * file) {
	return (SExprStream) {
		file,
		FILE_stream_read
	};
}
