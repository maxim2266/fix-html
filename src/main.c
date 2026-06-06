#include "fix-html.h"

#include <unistd.h>
#include <locale.h>

#ifndef VER
#error "program version constant is not defined"
#endif

// input parser
static
GumboOutput* parse(const str content) {
	GumboOptions options = kGumboDefaultOptions;

	options.max_errors = 0;	// because we don't have access to those errors

	return gumbo_parse_with_options(&options, content.data, content.length);
}

// usage string
static
const char usage_string[] =
"Usage:\n"
"  %1$s [HTML-FILE]\n"
"  %1$s -i HTML-FILE\n"
"  %1$s [-hv]\n"
"\n"
"Read any HTML input and produce clean, well-formed HTML5 output.\n"
"\n"
"Options:\n"
"  -i         Modify input file in-place.\n"
"  -h/--help  Show this help and exit.\n"
"  -v         Show version and exit.\n";

// usage string display
NORETURN
void usage_exit(const char* exe) {
	const char* const s = strrchr(exe, '/');

	fprintf(stderr, usage_string, s ? (s + 1) : exe);
	exit(EXIT_FAILURE);
}

// settings
static
const char* file_name = NULL;	// file to modify in-place

// option parser
NOINLINE
void parse_options(int argc, char** argv) {
	int c;
	bool in_place = false;

	opterr = 0;

	while((c = getopt(argc, argv, ":ihv")) != -1) {
		switch(c) {
			case 'i':	// in-place modification
				in_place = true;
				break;

			case 'h':	// help
				usage_exit(*argv);

			case 'v':	// version
				fwrite(XSTR(VER) "\n", sizeof(XSTR(VER)), 1, stderr);
				exit(EXIT_FAILURE);

			case '?':
				if(argv[optind] && strcmp(argv[optind], "--help") == 0)
					usage_exit(*argv);
				else
					die("unrecognised option `-%c`", optopt);
		}
	}

	// check the number of remaining parameters
	switch(argc - optind) {
		case 0: // data from STDIN to STDOUT
			if(in_place)
				die("missing file name");

			break;

		case 1: // redirect STDIN
			if(!freopen(argv[optind], "r", stdin))
				die_errno("cannot open \"%s\" for reading", argv[optind]);

			if(in_place)
				file_name = argv[optind];

			break;

		default:
			die("cannot process more than one input file (%d given)", argc - optind);
	}

	// make sure input is not a TTY
	if(isatty(STDIN_FILENO))
		usage_exit(*argv);
}

// main
#define LOC	"C.utf8"

int main(int argc, char** argv) {
	// make STDERR line-buffered
	setvbuf(stderr, NULL, _IOLBF, 0);

	// locale
	if(!setlocale(LC_ALL, LOC))
		die("cannot set \"" LOC "\" locale");

	// options
	parse_options(argc, argv);

	// read input
	const str content = read_input(STDIN_FILENO);

	if(fclose(stdin))
		die_errno("reading input");

	if(content.length == 0)
		die("empty input");

	// parse input
	GumboOutput* const dom = parse(content);

	if(!dom || !dom->root)
		die("unparseable input");

	// write output
	if(!file_name)
		setvbuf(stdout, NULL, _IOFBF, 0);
	else if(!freopen(file_name, "w", stdout))
		die_errno("cannot open \"%s\" for writing", file_name);

	write_node(dom->document);

	// flush output buffer
	if(fclose(stdout))
		die_errno("writing output");

	// purists are welcome to uncomment these lines:
	// gumbo_destroy_output(&kGumboDefaultOptions, dom);
	// free((void*)content.data);

	return 0;
}
