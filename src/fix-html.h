#pragma once

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <err.h>

#include <gumbo.h>

// macro argument to string literal
#define XSTR(s)	STR(s)
#define STR(s)	#s

// attributes
#define NORETURN	static __attribute__((noinline,noreturn))
#define NOINLINE	static __attribute__((noinline))

// logging
#define die(fmt, ...)		errx(EXIT_FAILURE, "[error] " fmt __VA_OPT__(,) __VA_ARGS__)
#define die_errno(fmt, ...)	err(EXIT_FAILURE, "[error] " fmt __VA_OPT__(,) __VA_ARGS__)

// string
typedef GumboStringPiece str;

#define str_lit(s)		((str){ s "", sizeof(s) - 1 })
#define str_null		((str){ 0 })

static inline
str str_from_cstr(const char* const s) {
	return s ? ((str){ s, strlen(s) }) : str_null;
}

// input reader
str read_input(const int fd);

// write out HTML node
void write_node(const GumboNode* const node);
