BIN       := fix-html
CFILES    := $(BIN).c
CFLAGS    := -s -O2 -std=c11 -Wall -Wextra -pipe

.PHONY: all clean test

all: $(BIN)

$(BIN): $(CFILES) version
	$(CC) $(CFLAGS) -DVER=$(shell ./version) -o $@ $(CFILES) -lgumbo -lmagic
	chmod 0711 $@

clean:
	$(RM) $(BIN)

test: $(BIN) run-tests
	./run-tests
