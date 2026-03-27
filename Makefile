CFLAGS=-Wall -Werror -Wimplicit-fallthrough

build/sexprs.a: build/sexprs.o
	ar crs build/sexprs.a build/sexprs.o

build/sexprs.o: build/tests sexprs.c sexprs.h
	mkdir -p build
	cc -c sexprs.c -o build/sexprs.o -std=c99 ${CFLAGS} ${LDFLAGS}

build/tests: tests.c
	mkdir -p build
	cc tests.c sexprs.c -o build/tests -std=c99 ${CFLAGS} ${LDFLAGS}
	./build/tests

clean:
	rm -r build

test: build/tests

.PHONY: clean, tests
