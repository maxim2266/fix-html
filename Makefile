# files
BIN    := fix-html
CFILES := src/main.c src/writer.c src/reader.c

# compiler flags
WARN_CFLAGS := -Wall -Wextra -Wformat -Werror=format-security \
	       -Werror=implicit-function-declaration -Werror=int-conversion

override CFLAGS := -s -O2 -std=c11 -pipe $(WARN_CFLAGS) $(CFLAGS)

# targets
.PHONY: all static clean test install uninstall

all: $(BIN)

$(BIN): $(CFILES) version src/fix-html.h
	$(CC) $(CFLAGS) -DVER=$(shell ./version) -o $@ $(CFILES) -lgumbo

static: CFLAGS += -static-pie
static: $(BIN)

clean:
	$(RM) $(BIN)

test: $(BIN) run-tests
	./run-tests

# installation
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
DESTDIR ?=

install: $(BIN)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/$(BIN)
