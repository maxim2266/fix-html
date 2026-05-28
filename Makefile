# files
BIN    := fix-html
CFILES := $(BIN).c

# compiler flags
WARN_CFLAGS := -Wall -Wextra -Wformat -Werror=format-security \
	       -Werror=implicit-function-declaration -Werror=int-conversion

BASE_CFLAGS := -s -O2 -std=c11 -pipe $(WARN_CFLAGS)

override CFLAGS := $(BASE_CFLAGS) $(CFLAGS)

# targets
.PHONY: all static clean test install uninstall

all: $(BIN)

$(BIN): $(CFILES) version
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
