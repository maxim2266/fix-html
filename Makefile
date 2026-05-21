BIN       := fix-html
CFILES    := $(BIN).c
CFLAGS    := -s -O2 -std=c11 -Wall -Wextra

.PHONY: all clean

# local build
all: $(BIN)

$(BIN): $(CFILES) write.c version
	$(CC) $(CFLAGS) -DVER=$(shell ./version) -o $@ $(CFILES) -lgumbo -lmagic
	chmod 0711 $@

clean:
	$(RM) $(BIN)
