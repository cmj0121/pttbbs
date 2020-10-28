include Makefile.in

SRC := $(wildcard *.c)
OBJ := $(subst .c,.o,$(SRC))
BIN := loginc logind

LIB     := $(SRCROOT)/common/bbs/llibbbs.a $(SRCROOT)/common/osdep/libosdep.a $(SRCROOT)/common/sys/libsys.a
CFLAGS  := $(CFLAGS)
LDFLAGS := $(LDFLAGS) $(COMMON_LIB)


.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ)
