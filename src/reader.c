#include "fix-html.h"

#include <errno.h>
#include <unistd.h>
#include <uchar.h>

// codepoint decoder
typedef enum : uint8_t {
	CP_OK,
	CP_BAD,
	CP_INC,
} codepoint_status;

typedef struct {
	codepoint_status status;
	uint8_t 		 len;
} codepoint_info;

static
codepoint_info next_codepoint(mbstate_t* const ps, const char* const s, size_t n) {
	char32_t c;

	// https://en.cppreference.com/c/string/multibyte/mbrtoc32
	const size_t len = mbrtoc32(&c, s, n, ps);

	switch(len) {
		case 1:	// ASCII
			switch(c) {
				// control characters
				case 0x0001 ... 0x0008:	// C0 controls
				case 0x000B:			// vert. tab
				case 0x000E ... 0x001F:	// C0 controls
				case 0x007F ... 0x009F:	// del + C1 controls
					return (codepoint_info){ CP_BAD, 1 };

				default:
					return (codepoint_info){ CP_OK, 1 };
			}

		case 2 ... 4:
			// check for non-characters
			return (codepoint_info){
				((c & 0xFFFE) != 0xFFFE && (c < 0xFDD0 || c > 0xFDEF)) ? CP_OK : CP_BAD,
				len
			};

		case (size_t)-2:	// incomplete utf-8 sequence
			return (codepoint_info){ CP_INC, n };

		default:
			return (codepoint_info){ CP_BAD, 1 };
	}
}

// constants
#define Kb	1024
#define Mb	(Kb * Kb)
#define MAX_INPUT_SIZE	(64 * Mb)
#define INPUT_BUFF_SIZE (64 * Kb)

// fill input buffer
static
size_t read_bytes(const int fd, char* buff, const char* const end) {
	char* p = buff;
	ssize_t n;

	do {
		while((n = read(fd, p, end - p)) < 0)
			if(errno != EINTR)
				die_errno("reading input");
	} while(n > 0 && (p += n) < end);

	return p - buff;
}

// dynamic buffer
typedef struct {
	size_t    len, cap;
	char*	  buff;
} buffer;

// append bytes to the dynamic buffer
static
void append(buffer* const dest, const char* const s, const size_t n) {
	if(n > 0) {
		if(dest->len + n > dest->cap) {
			if((dest->cap *= 2) > MAX_INPUT_SIZE)
				die("cannot handle input larger than %zuMb", (size_t)MAX_INPUT_SIZE / Mb);

			dest->buff = realloc(dest->buff, dest->cap);
		}

		memcpy(dest->buff + dest->len, s, n);
		dest->len += n;
	}
}

// append Unicode replacement character
#define append_repl_char(dest)	append((dest), u8"�", sizeof(u8"�") - 1)

// append with UTF-8 validation
static
size_t append_bytes(buffer* const dest, char* const src, const char* const end) {
	mbstate_t state = {0};	// reset state on each iteration
	const char* s = src;

	for(const char* p = src; p < end; ) {
		const codepoint_info r = next_codepoint(&state, p, end - p);

		switch(r.status) {
			case CP_OK:
				p += r.len;
				break;

			case CP_BAD:
				append(dest, s, p - s);
				append_repl_char(dest);
				s = (p += r.len);
				break;

			case CP_INC:
				append(dest, s, p - s);
				memmove(src, p, r.len);
				return r.len;
		}
	}

	append(dest, s, end - s);
	return 0;
}

// read the entire input from the given file descriptor
str read_input(const int fd) {
	// input buffer
	char src[INPUT_BUFF_SIZE];
	size_t offset = 0;

	// output buffer
	buffer dest = { .cap = INPUT_BUFF_SIZE, .buff = malloc(INPUT_BUFF_SIZE) };

	// read/append loop
	size_t n;

	while((n = read_bytes(fd, src + offset, src + INPUT_BUFF_SIZE)) > 0)
		offset = append_bytes(&dest, src, src + offset + n);

	// replace trailing incomplete sequence
	if(offset > 0)
		append_repl_char(&dest);

	// all done
	return (str){ realloc(dest.buff, dest.len), dest.len };
}
