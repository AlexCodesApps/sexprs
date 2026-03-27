# Simple S-Expr parser library written in C99

## Why?
I wanted to test how easy it was to write an s-expr parser.

## What does it do?
- It parses a limited subset of s-exprs that would work across different lisps.
- Currently lacks builtin multi-line strings, but does support
    support customizing the string lexer.
- Symbols are anything that don't otherwise make sense

## Basic Usage
```C
    const char * input = /* ... */;
    SExprBuffer buf = {
        .ptr = input,
        .nleft = strlen(input),
    };
    SExprParseOptions opts = {0};
    opts.stream = sexpr_buffer_stream(&buf);
    SExpr expr;
    SExprParseResult r = sexpr_parse(&opts, &expr);
    assert(r == SEXPR_PARSE_OK);
    /* ... */
    sexpr_free(expr, opts.allocator);
```

## Using SExprs
```C
/* Discriminate SExpr */
SExprType sexpr_type(SExpr expr);
/* can be
	SEXPR_NIL
	| SEXPR_INT
	| SEXPR_STRING
	| SEXPR_CONS
	| SEXPR_SYMBOL
	| SEXPR_FLOAT
*/

/* Cast Sexpr */
double sexpr_as_float(SExpr expr);
void * sexpr_as_boxed(SExpr expr);
char * sexpr_as_symbol(SExpr expr);
SExprCons * sexpr_as_cons(SExpr expr);
int sexpr_as_int(SExpr expr);

/* Lift to SExpr */
SExpr float_as_sexpr(double f);
SExpr int_as_sexpr(int i);
SExpr boxed_as_sexpr(void * boxed, SExprType type);
SExpr string_as_sexpr(char * str);
SExpr symbol_as_sexpr(char * sym);
SExpr cons_as_sexpr(SExprCons * cons);
```

## Parse Options
```C
/* Fields are to be 0-initialized as a good default */
typedef struct SExprParseOptions {
    /* Allocator to use. If unspecified,
        the field is set to sexpr_default_allocator()
    */
	SExprAllocator allocator;
    /* Stream to pull data from. If unspecified,
        the field is set to sexpr_FILE_stream(stdout)
    */
	SExprStream stream;
    /* String lexer. If unspecified,
        the field is set to an internal builtin.
    */
	SExprParseResult (*lex_str)(struct SExprParseOptions * opts, char ** out);
    /* Nil keyword alias for '(). If unspecified or set to NULL,
        it is excluded during parsing.
    */
	const char * nil_keyword;
    /* Max depth limit for SExprs. If unspecified or set to 0,
        it is excluded during parsing.
	size_t nest_limit;
} SExprParseOptions;


SExprParseResult sexpr_parse(SExprParseOptions * opts, SExpr * out);
```

### Allocator
The ``realloc_buffer`` and ``allocate_cons`` are intended allocate without any
surprising behavior, but the ``allocate_string`` and ``allocate_symbol`` are
functions that take in a buffer allocated with ``realloc_buffer``
and potentially do nothing, but could also do interning and the like.
The ``free_*`` functions don't have any tricks. They don't take a length
parameter as the lengths are statically known or request-able with ``strlen``.
```C
typedef struct {
	/* must always be aligned to 8 bytes */
	void * ctx;
	const SExprAllocatorVTable * vtable;
} SExprAllocator;


typedef struct {
	char * (*realloc_buffer)(void * ctx, char * buffer,
                                size_t oldsize, size_t newsize);
	char * (*allocate_string)(void * ctx, char *);
	char * (*allocate_symbol)(void * ctx, char *);
	SExprCons * (*allocate_cons)(void * ctx, SExprCons);
	void (*free_string)(void * ctx, char *);
	void (*free_symbol)(void * ctx, char *);
	void (*free_cons)(void * ctx, SExprCons *);
} SExprAllocatorVTable;


SExprAllocator sexpr_default_allocator(void);
```

### Stream
No asynchronous support, but somewhat self explanatory.
```C
typedef struct {
	void * ctx;
	const SExprStreamVTable * vtable;
} SExprStream;

typedef struct {
	int (*peek)(void * ctx);
	int (*next)(void * ctx);
} SExprStreamVTable;

/* For string input */
typedef struct {
	const char * ptr;
	ptrdiff_t nleft;
} SExprBuffer;

SExprStream sexpr_buffer_stream(SExprBuffer * buffer);
SExprStream sexpr_FILE_stream(FILE * file);
```

### Parse Results
```C
typedef enum {
	SEXPR_PARSE_OK, /* success */
	SEXPR_PARSE_UNEXPECTED_CHAR, /* unexpected char, unconsumed */
	SEXPR_PARSE_UNEXPECTED_TOKEN, /* unexpected token,
                                    (needs a reporting mechanism) */
	SEXPR_PARSE_UNEXPECTED_EOF, /* unexpected EOF */
	SEXPR_UNTERM_STRING, /* unterminated string */
	SEXPR_PARSE_OVERFLOW, /* number overflow */
	SEXPR_PARSE_NEST_LIMIT_EXCEEDED, /* maximum depth > nested depth option */
	SEXPR_PARSE_OOM, /* out of memory situation */
} SExprParseResult;
```

### Cleanup
If no allocator was specified to ``sexpr_parse``,
the default allocator will be written into the opts pointer
for use with this function.
```C
void sexpr_free(SExpr expr, SExprAllocator alloc);
```
