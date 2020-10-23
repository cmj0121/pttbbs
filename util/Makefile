include Makefile.in

SRC := $(wildcard *.c)
OBJ := $(subst .c,.o,$(SRC))
BIN := bbsctl

LDFLAGS := $(COMMON_LIB) $(LDFLAGS) 

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ)

%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
