include ../Makefile.in

SRC    := $(wildcard *.c)
OBJ    := $(subst .c,.o,$S(SRC))
BIN    := itoc_trans sob_trans transman

CFLAGS  := $(CFLAGS)
LDFLAGS := $(LDFLAGS) -Lcommon/sys/libsys.a -Lcommon/bbs/libbbs.a -Lcommon/osdep/libosdep.a

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN)

%: %.o
	$(CC) $(CFLAGS) -o $@ $^
