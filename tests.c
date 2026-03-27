#include "sexprs.h"
#include <locale.h>
#include <math.h>
#include <stdio.h>

#undef NDEBUG
#include <assert.h>

void test_round_trip(void) {
	void * MAGIC_PTR = (void *)(uintptr_t)(0x0000ABCDEFABCDE8);
	assert(sexpr_as_string(string_as_sexpr(MAGIC_PTR)) == MAGIC_PTR);
	assert(sexpr_as_symbol(symbol_as_sexpr(MAGIC_PTR)) == MAGIC_PTR);
	assert(sexpr_as_cons(cons_as_sexpr(MAGIC_PTR)) == MAGIC_PTR);

	assert(sexpr_as_int(int_as_sexpr(0xABCDEFAB)) == 0xABCDEFAB);
	assert(sexpr_as_int(int_as_sexpr(0xFFFFFFFF)) == 0xFFFFFFFF);
	assert(sexpr_as_int(int_as_sexpr(0)) == 0);

	assert(isnan(sexpr_as_float(float_as_sexpr(NAN))));
	assert(sexpr_as_float(float_as_sexpr(INFINITY)) == INFINITY);
	assert(sexpr_as_float(float_as_sexpr(-INFINITY)) == -INFINITY);
	assert(sexpr_as_float(float_as_sexpr(12345678.0)) == 12345678.0);
	assert(sexpr_as_float(float_as_sexpr(0)) == 0);
}

void test_tag(void) {
	assert(sexpr_type(string_as_sexpr(NULL)) == SEXPR_STRING);
	assert(sexpr_type(symbol_as_sexpr(NULL)) == SEXPR_SYMBOL);
	assert(sexpr_type(cons_as_sexpr(NULL)) == SEXPR_CONS);
	assert(sexpr_type(int_as_sexpr(0)) == SEXPR_INT);
	assert(sexpr_type(float_as_sexpr(0.0)) == SEXPR_FLOAT);
	assert(sexpr_type(NIL_SEXPR) == SEXPR_NIL);
}

void test_no_crash(void) {
	const char * test = "(+ 12 (123.0* 4.0 \"5 fjl \nf\"))";
	SExprParseOptions opts = {0};
	SExprBuffer buf;
	buf.ptr = test;
	buf.nleft = strlen(test);
	opts.stream = sexpr_buffer_stream(&buf);
	opts.allocator = sexpr_default_allocator();
	SExpr expr;
	SExprParseResult r = sexpr_parse(opts, &expr);
	assert(r == SEXPR_PARSE_OK);
	assert(sexpr_type(expr) == SEXPR_CONS);
	SExprCons * cons = sexpr_as_cons(expr);
	assert(sexpr_type(cons->car) == SEXPR_SYMBOL);
	assert(strcmp(sexpr_as_symbol(cons->car), "+") == 0);
	assert(sexpr_type(cons->cdr) == SEXPR_CONS);
	cons = sexpr_as_cons(cons->cdr);
	assert(sexpr_type(cons->car) == SEXPR_INT);
	assert(sexpr_as_int(cons->car) == 12);
	assert(sexpr_type(cons->cdr) == SEXPR_CONS);
	cons = sexpr_as_cons(cons->cdr);
	assert(sexpr_type(cons->car) == SEXPR_CONS);
	assert(sexpr_type(cons->cdr) == SEXPR_NIL);
	cons = sexpr_as_cons(cons->car);
	assert(sexpr_type(cons->car) == SEXPR_SYMBOL);
	assert(sexpr_type(cons->cdr) == SEXPR_CONS);
	assert(strcmp(sexpr_as_symbol(cons->car), "123.0*") == 0);
	cons = sexpr_as_cons(cons->cdr);
	assert(sexpr_type(cons->car) == SEXPR_FLOAT);
	assert(sexpr_type(cons->cdr) == SEXPR_CONS);
	assert(sexpr_as_float(cons->car) == 4.0);
	cons = sexpr_as_cons(cons->cdr);
	assert(sexpr_type(cons->car) == SEXPR_STRING);
	assert(sexpr_type(cons->cdr) == SEXPR_NIL);
	assert(strcmp(sexpr_as_string(cons->car), "5 fjl \nf") == 0);
	sexpr_free(expr, opts.allocator);
}

int main(void) {
	setlocale(LC_NUMERIC, "C");
	test_round_trip();
	test_tag();
	test_no_crash();
	fprintf(stderr, "All tests passed.\n");
	return 0;
}
