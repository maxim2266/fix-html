BIN       := fix-html
CFILES    := $(BIN).c
CFLAGS    := -s -Os -std=c11 -Wall -Wextra

.PHONY: all clean

# local build
all: $(BIN)

$(BIN): $(CFILES) version
	$(CC) $(CFLAGS) -DVER=$(shell ./version) -o $@ $(CFILES) -lgumbo
	chmod 0711 $@

clean:
	$(RM) $(BIN)
