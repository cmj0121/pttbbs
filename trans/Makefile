include ../Makefile.in

SRC    := $(wildcard *.c)
OBJ    := $(subst .c,.o,$S(SRC))
BIN    := itoc_trans sob_trans transman

CFLAGS  := $(CFLAGS) -DPTTBBS_UTIL
LDFLAGS := $(LDFLAGS) $(COMMON_LIB)

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN)

itoc_trans sob_trans: $(MBBSD_UTIL_OBJ)

%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
