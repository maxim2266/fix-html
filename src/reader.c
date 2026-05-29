#include "fix-html.h"

#include <errno.h>
#include <unistd.h>

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
