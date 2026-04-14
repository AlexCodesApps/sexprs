SEXPRS_DISABLE_NAN_BOXING ?= OFF

.PHONY: all
all: build cmake

.PHONY: build
build: cmake
	cmake --build build

.PHONY: cmake
cmake:
	cmake -S . -B build -DSEXPRS_DISABLE_NAN_BOXING=$(SEXPRS_DISABLE_NAN_BOXING)

.PHONY: clean
clean:
	rm -r build

.PHONY: test
test: build
	ctest --test-dir build --output-on-failure
