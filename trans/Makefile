include ../Makefile.in

SRC    := $(wildcard *.c)
OBJ    := $(subst .c,.o,$S(SRC))
BIN    := itoc_trans sob_trans transman

CFLAGS  := $(CFLAGS)
LDFLAGS := $(LDFLAGS) \
			$(SRCROOT)/common/sys/libsys.a \
			$(SRCROOT)/common/bbs/libbbs.a \
			$(SRCROOT)/common/osdep/libosdep.a

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN)

%: %.o
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
