#include "sexprs.h"
#include <math.h>
#include <stdio.h>

#undef NDEBUG
#include <assert.h>

void test_round_trip(void) {
	void * MAGIC_PTR = (void *)(uintptr_t)(0x0000ABCDEFABC000);
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

int main(void) {
	test_round_trip();
	test_tag();
	fprintf(stderr, "All tests passed.\n");
	return 0;
}
