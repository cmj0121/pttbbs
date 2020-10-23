include Makefile.in

SRC := $(wildcard *.c)
OBJ := $(subst .c,.o,$(SRC))
BIN := loginc logind

LIB     := $(SRCROOT)/common/bbs/llibbbs.a $(SRCROOT)/common/osdep/libosdep.a $(SRCROOT)/common/sys/libsys.a
CFLAGS  := $(CFLAGS) -DFIONWRITE
LDFLAGS := $(LDFLAGS) \
				-L $(SRCROOT)/common/bbs   -lbbs \
				-L $(SRCROOT)/common/osdep -losdep \
				-L $(SRCROOT)/common/sys   -lsys


.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ)
