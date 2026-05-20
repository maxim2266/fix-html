#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <gumbo.h>

#ifndef VER
#error "program version constant is not defined"
#endif

// macro argument to string literal
#define XSTR(s)	STR(s)
#define STR(s)	#s

// attributes
#define NORETURN	static __attribute__((noinline,noreturn))

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
		die_errno("output failed"); \
	}   \
})

// write out a byte
#define write_byte(b)   fail_if(putchar(b) == EOF)

// write out bytes
static inline
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
			case 0:   write_bytes(s, p - s);    return;
			case '&': subst = str_lit("&amp;"); break;
			case '<': subst = str_lit("&lt;");  break;
			case '>': subst = str_lit("&gt;");  break;
			default:  ++p; continue;
		}

		write_bytes(s, p - s);
		write_bytes(subst.data, subst.length);
		s = ++p;
	}
}

// write out HTML attribute
static
void write_attr(const char* s, const char q) {
	if(!s)
		return;

	// substitution string
	str qs;

	switch(q) {
		case '"':  qs = str_lit("&quot;"); break;
		case '\'': qs = str_lit("&#39;"); break;
		default:   qs = str_null; break;	// must never happen
	}

	// encode
	str subst;
	const char* p = s;

	for(;;) {
		switch(*p) {
			case 0:   write_bytes(s, p - s);    return;
			case '&': subst = str_lit("&amp;"); break;
			case '<': subst = str_lit("&lt;");  break;
			case '>': subst = str_lit("&gt;");  break;

			default:
				if(*p != q) {
					++p;
					continue;
				} else
					subst = qs;
		}

		write_bytes(s, p - s);
		write_bytes(subst.data, subst.length);
		s = ++p;
	}
}


	}
}





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

// main
int main(int argc, char** argv) {
	// make STDERR line-buffered
	setvbuf(stderr, NULL, _IOLBF, 0);

	// parse options
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
	if(argc - optind > 1)
		die("cannot process more than one input file (%d given)", argc - optind);

	// just for now
	write_html("\"a > b\"\n");
	write_attr("\"a > b' \"\n", '"');

	log_info("It works!");
	return 0;
}
