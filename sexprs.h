#ifndef SEXPRS_H
#define SEXPRS_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	SEXPR_NIL,
	SEXPR_INT,
	SEXPR_STRING,
	SEXPR_CONS,
	SEXPR_SYMBOL,
	SEXPR_FLOAT,
} SExprType;

typedef struct {
	uint64_t inner;
} SExpr;

typedef struct {
	SExpr car;
	SExpr cdr;
} SExprCons;

#define NIL_SEXPR ((SExpr){ SEXPR_QNAN })
#define SEXPR_QNAN ((uint64_t)0x7ffc000000000000)
#define SEXPR_SIGNBIT ((uint64_t)0x8000000000000000)

static inline double sexpr_as_float(SExpr expr) {
	double out;
	memcpy(&out, &expr.inner, sizeof(double));
	return out;
}

static inline void * sexpr_as_boxed(SExpr expr) {
	return (void *)((uintptr_t)expr.inner & ~(uintptr_t)(SEXPR_SIGNBIT | SEXPR_QNAN | 0x7));
}

static inline char * sexpr_as_string(SExpr expr) {
	return (char *)sexpr_as_boxed(expr);
}

static inline char * sexpr_as_symbol(SExpr expr) {
	return (char *)sexpr_as_boxed(expr);
}

static inline SExprCons * sexpr_as_cons(SExpr expr) {
	return (SExprCons *)sexpr_as_boxed(expr);
}

static inline int sexpr_as_int(SExpr expr) {
	return expr.inner & 0xFFFFFFFF;
}

static inline SExpr float_as_sexpr(double f) {
	SExpr expr;
	memcpy(&expr.inner, &f, sizeof(double));
	return expr;
}

static inline SExpr int_as_sexpr(int i) {
	return (SExpr){ (uint64_t)i | (1ULL << 49 | SEXPR_QNAN) };
}

static inline SExpr boxed_as_sexpr(void * boxed, SExprType type) {
	assert(((uintptr_t)boxed & 0x7) == 0);
	return (SExpr){ (uint64_t)(uintptr_t)boxed | (SEXPR_SIGNBIT | SEXPR_QNAN) | type };
}

static inline SExpr string_as_sexpr(char * str) {
	return boxed_as_sexpr(str, SEXPR_STRING);
}

static inline SExpr symbol_as_sexpr(char * sym) {
	return boxed_as_sexpr(sym, SEXPR_SYMBOL);
}

static inline SExpr cons_as_sexpr(SExprCons * cons) {
	return boxed_as_sexpr(cons, SEXPR_CONS);
}

static inline SExprType sexpr_type(SExpr expr) {
	if ((expr.inner & SEXPR_QNAN) != SEXPR_QNAN) {
		return SEXPR_FLOAT;
	}
	if (expr.inner & SEXPR_SIGNBIT) { // Boxed
		return (SExprType)(expr.inner & 0x7);
	}
	if (expr.inner & (1ULL << 49)) {
		return SEXPR_INT;
	}
	return SEXPR_NIL;
}

typedef struct {
	char * (*realloc_buffer)(void * ctx, char * buffer, size_t oldsize, size_t newsize);
	char * (*allocate_string)(void * ctx, char *);
	char * (*allocate_symbol)(void * ctx, char *);
	SExprCons * (*allocate_cons)(void * ctx, SExprCons);
	void (*free_string)(void * ctx, char *);
	void (*free_symbol)(void * ctx, char *);
	void (*free_cons)(void * ctx, SExprCons *);
} SExprAllocatorVTable;

typedef struct {
	// must always be aligned to 8 bytes
	void * ctx;
	const SExprAllocatorVTable * vtable;
} SExprAllocator;

typedef struct {
	int (*peek)(void * ctx);
	int (*next)(void * ctx);
	int (*err)(void * ctx);
} SExprStreamVTable;

typedef struct {
	void * ctx;
	const SExprStreamVTable * vtable;
} SExprStream;

typedef enum {
	SEXPR_PARSE_OK,
	SEXPR_PARSE_UNEXPECTED_CHAR,
	SEXPR_PARSE_UNEXPECTED_TOKEN,
	SEXPR_PARSE_UNEXPECTED_EOF,
	SEXPR_UNTERM_STRING,
	SEXPR_PARSE_OVERFLOW,
	SEXPR_PARSE_NEST_LIMIT_EXCEEDED,
	SEXPR_PARSE_OOM,
} SExprParseResult;


typedef struct SExprParseOptions {
	SExprAllocator allocator;
	SExprStream stream;
	SExprParseResult (* lex_str)(struct SExprParseOptions * opts, char ** out);
	const char * nil_keyword;
	size_t nest_limit;
} SExprParseOptions;

typedef struct {
	const char * ptr;
	ptrdiff_t nleft;
} SExprBuffer;

SExprAllocator sexpr_default_allocator(void);
SExprStream sexpr_buffer_stream(SExprBuffer * buffer);
SExprStream sexpr_FILE_stream(FILE * file);

/* PRE: LC_NUMERIC must be set to "C" or "POSIX" */
SExprParseResult sexpr_parse(SExprParseOptions opts, SExpr * out);

void sexpr_free(SExpr expr, SExprAllocator alloc);

#endif // SEXPRS_H
