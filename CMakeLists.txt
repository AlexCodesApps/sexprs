cmake_minimum_required(VERSION 3.10)
project(sexprs LANGUAGES C)
set(CMAKE_C_STANDARD_REQUIRED C99)
enable_testing()

option(SEXPRS_DISABLE_NAN_BOXING "SExprs disable NAN boxing" OFF)


add_library(sexprs INTERFACE)

add_library(sexprs_disable_nan_boxing)
target_sources(sexprs_disable_nan_boxing PRIVATE sexprs.c)
target_compile_definitions(sexprs_disable_nan_boxing PRIVATE SEXPRS_DISABLE_NAN_BOXING)

if (SEXPRS_DISABLE_NAN_BOXING)
	target_link_libraries(sexprs INTERFACE sexprs_disable_nan_boxing)
else()
	add_library(sexprs_nan_boxing)
	target_sources(sexprs_nan_boxing PRIVATE sexprs.c)
	target_link_libraries(sexprs INTERFACE sexprs_nan_boxing)
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	if (NOT SEXPRS_DISABLE_NAN_BOXING)
		add_executable(tests tests.c)
		target_link_libraries(tests PRIVATE sexprs_nan_boxing)
		add_test(NAME tests COMMAND tests)
	endif()
	add_executable(tests_disable_nan_boxing tests.c)
	target_link_libraries(tests_disable_nan_boxing PRIVATE sexprs_disable_nan_boxing)
	target_compile_definitions(tests_disable_nan_boxing PRIVATE SEXPRS_DISABLE_NAN_BOXING)
	add_test(NAME tests_disable_nan_boxing COMMAND tests_disable_nan_boxing)
endif()
