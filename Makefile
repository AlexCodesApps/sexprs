CFLAGS=-Wall -Werror -Wimplicit-fallthrough

build/sexprs.a: build/sexprs.o
	ar crs build/sexprs.a build/sexprs.o

build/sexprs.o: build/tests build/tests_disable_nan_boxing sexprs.c sexprs.h
	mkdir -p build
	cc -c sexprs.c -o build/sexprs.o -std=c99 ${CFLAGS} ${LDFLAGS}

build/tests: tests.c sexprs.c sexprs.h
	mkdir -p build
	cc tests.c sexprs.c -o build/tests -std=c99 ${CFLAGS} ${LDFLAGS}
	./build/tests

build/tests_disable_nan_boxing: tests.c sexprs.c sexprs.h
	mkdir -p build
	cc tests.c sexprs.c -o build/tests_disable_nan_boxing \
		-std=c99 ${CFLAGS} ${LDFLAGS} -DSEXPR_DISABLE_NAN_BOXING
	./build/tests_disable_nan_boxing

clean:
	rm -r build

test: build/tests

.PHONY: clean, tests
