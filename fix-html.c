#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>

#include <gumbo.h>
#include <magic.h>

#ifndef VER
#error "program version constant is not defined"
#endif

// macro argument to string literal
#define XSTR(s)	STR(s)
#define STR(s)	#s

// attributes
#define NORETURN	static __attribute__((noinline,noreturn))
#define NOINLINE	static __attribute__((noinline))

// logging ----------------------------------------------------------------------------------------
static
unsigned log_level = 0u;

#define log_info(fmt, ...) ({	\
	if(log_level == 0u)	\
		warnx("[info] " fmt __VA_OPT__(,) __VA_ARGS__);	\
})

#define log_info_errno(fmt, ...) ({	\
	if(log_level == 0u)	\
		warn("[info] " fmt __VA_OPT__(,) __VA_ARGS__);	\
})

#define log_warn(fmt, ...) ({	\
	if(log_level <= 1u)	\
		warnx("[warn] " fmt __VA_OPT__(,) __VA_ARGS__);	\
})

#define log_warn_errno(fmt, ...) ({	\
	if(log_level <= 1u)	\
		warn("[warn] " fmt __VA_OPT__(,) __VA_ARGS__);	\
})

#define log_err(fmt, ...)		 warnx("[error] " fmt __VA_OPT__(,) __VA_ARGS__)
#define log_err_errno(fmt, ...)	 warn("[error] " fmt __VA_OPT__(,) __VA_ARGS__)

// termination
#define die(fmt, ...)		errx(EXIT_FAILURE, "[error] " fmt __VA_OPT__(,) __VA_ARGS__)
#define die_errno(fmt, ...)	err(EXIT_FAILURE, "[error] " fmt __VA_OPT__(,) __VA_ARGS__)

// string -----------------------------------------------------------------------------------------
typedef GumboStringPiece str;

#define str_lit(s)		((str){ s "", sizeof(s) - 1 })
#define str_null		((str){ 0 })

// output writers ---------------------------------------------------------------------------------
// output stream error check
#define fail_if(cond) ({ \
	if(cond) {  \
		fclose(stdout); \
		die_errno("writing output"); \
	}   \
})

// write out a byte
#define write_byte(b)   fail_if(putchar(b) == EOF)

// write out bytes
static
void write_bytes(const char* const s, const size_t n) {
	fail_if(n > 0 && fwrite(s, 1, n, stdout) < n);
}

// write out a string literal
#define write_lit(s)    write_bytes(s "", sizeof(s) - 1)

// write out C string
#define write_cstr(s)   fail_if(fputs((s), stdout) == EOF)

// white out HTML string
static
void write_html(const char* s) {
	if(!s)
		return;

	str subst;
	const char* p = s;

	for(;;) {
		switch(*p) {
			case 0:   write_bytes(s, p - s);     return;
			case '&': subst = str_lit("&amp;");  break;
			case '<': subst = str_lit("&lt;");   break;
			case '>': subst = str_lit("&gt;");   break;
			case '"': subst = str_lit("&quot;"); break;
			default:  ++p; continue;
		}

		write_bytes(s, p - s);
		write_bytes(subst.data, subst.length);
		s = ++p;
	}
}

// input reader -----------------------------------------------------------------------------------
#define Kb	1024
#define Mb	(Kb * Kb)
#define MAX_INPUT_SIZE	(256 * Mb)

// fill the given buffer
static
size_t fill_buffer(const int fd, uint8_t* const buff, const size_t size) {
	uint8_t* p = buff;
	uint8_t* const end = buff + size;
	ssize_t n;

	do {
		while((n = read(fd, p, end - p)) < 0)
			if(errno != EINTR)
				die_errno("reading input");
	} while(n > 0 && (p += n) < end);

	return p - buff;
}

// read the entire input from the given file descriptor
static
str read_input(const int fd) {
	size_t count = 0, size = MAX_INPUT_SIZE / Kb;
	uint8_t* buff = malloc(size);

	for(;;) {
		count += fill_buffer(fd, buff + count, size - count);

		if(count < size)
			return (str){ realloc(buff, count), count };

		if((size *= 2) > MAX_INPUT_SIZE)
			die("cannot handle input larger than %zuMb", (size_t)MAX_INPUT_SIZE / Mb);

		buff = realloc(buff, size);
	}
}

// Gumbo parser -----------------------------------------------------------------------------------
static
GumboOutput* parse(const str content) {
	GumboOptions options = kGumboDefaultOptions;

	return gumbo_parse_with_options(&options, content.data, content.length);
}

#include "write.c"

// MIME type --------------------------------------------------------------------------------------
static
bool mime_type_allowed(const char* const mime) {
	// list of MIME types we accept, apart from text/html
	// reason: libmagic is not perfect
	static
	const char* const allowed[] = {
		"text/plain",
		"text/xml",
		"text/css",
		"text/javascript",
		"application/xml",
		"application/xhtml+xml",
		"application/x-empty",
		"application/javascript",
		NULL
	};

	// check one by one
	for(unsigned i = 0; allowed[i]; ++i)
		if(strcmp(mime, allowed[i]) == 0)
			return true;

	return false;
}

static
void check_mime_type(const str s) {
	const int flags = MAGIC_RAW | MAGIC_NO_CHECK_ELF;
	const magic_t m = magic_open(flags | MAGIC_MIME_TYPE);

	if(!m)
		die_errno("cannot open MIME type detector");

	if(magic_load(m, NULL))
		die("cannot load MIME type database: %s", magic_error(m));

	// MIME type
	const char* const mime = magic_buffer(m, s.data, s.length);

	if(!mime)
		die("input: cannot detect MIME type: %s", magic_error(m));

	if(strcmp(mime, "text/html")) {
		if(!mime_type_allowed(mime))
			die("input: cannot process MIME type \"%s\"", mime);

		log_warn("input: detected MIME type is \"%s\", not \"text/html\"", mime);
	}

	// encoding
	if(magic_setflags(m, flags | MAGIC_MIME_ENCODING) < 0)
		die("failed to set MIME flags: %s", magic_error(m));

	const char* const enc =  magic_buffer(m, s.data, s.length);

	if(!enc)
		die("input: cannot detect encoding: %s", magic_error(m));

	if(strcasecmp(enc, "us-ascii") && strcasecmp(enc, "utf-8"))
		die("input: detected encoding is \"%s\", only ASCII and UTF-8 are supported", enc);

	magic_close(m);
}

// application ------------------------------------------------------------------------------------
// usage string
static
const char usage_string[] =
"Usage:\n"
"  %1$s [-q] [html-file]\n"
"  %1$s [-hv]\n"
"\n"
"Fix broken HTML files.\n"
"\n"
"Options:\n"
"  -q   Reduce logging level (may be given more than once).\n"
"  -h   Show this help and exit.\n"
"  -v   Show version and exit.\n";

// usage string display
NORETURN
void usage_exit(const char* exe) {
	const char* const s = strrchr(exe, '/');

	fprintf(stderr, usage_string, s ? (s + 1) : exe);
	exit(EXIT_FAILURE);
}

// option parser
static
void parse_options(int argc, char** argv) {
	opterr = 0;

	int c;

	while((c = getopt(argc, argv, "qhv")) != -1) {
		switch(c) {
			case 'q':	// log level
				++log_level;
				break;

			case 'h':	// help
				usage_exit(*argv);

			case 'v':	// version
				fwrite(XSTR(VER) "\n", sizeof(XSTR(VER)), 1, stderr);
				exit(EXIT_FAILURE);

			case '?':
				die("unrecognised option `-%c`", optopt);
		}
	}

	// check the number of remaining parameters
	switch(argc - optind) {
		case 0: // ok, data from STDIN
			break;

		case 1: // redirect STDIN
			if(!freopen(argv[optind], "r", stdin))
				die_errno("cannot open \"%s\"", argv[optind]);

			break;

		default:
			die("cannot process more than one input file (%d given)", argc - optind);
	}
}

// main
int main(int argc, char** argv) {
	// i/o buffering
	setvbuf(stderr, NULL, _IOLBF, 0);
	setvbuf(stdout, NULL, _IOFBF, 0);

	// options
	parse_options(argc, argv);

	// read input
	const str content = read_input(STDIN_FILENO);

	if(fclose(stdin))
		die_errno("reading input");

	if(content.length == 0)
		die("empty input");

	log_info("input: got %zu bytes", content.length);

	// check MIME type
	check_mime_type(content);

	// parse input
	GumboOutput* const doc = parse(content);

	if(doc->errors.length > 0)
		log_warn("input: detected %u HTML error(s)", doc->errors.length);

	// write output
	write_document(doc);

	// flush output buffer
	if(fclose(stdout))
		die_errno("writing output");

	// purists are welcome to uncomment these lines:
	// gumbo_destroy_output(&kGumboDefaultOptions, doc);
	// free((void*)content.data);

	return 0;
}
