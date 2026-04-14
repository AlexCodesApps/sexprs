SEXPRS_DISABLE_NAN_BOXING ?= OFF

all: build cmake

build:
	cmake --build build

cmake:
	cmake -S . -B build -DSEXPRS_DISABLE_NAN_BOXING=${SEXPRS_DISABLE_NAN_BOXING}

clean:
	rm -r build

test: build
	ctest --test-dir build --output-on-failure

.PHONY: build, clean, test, all
