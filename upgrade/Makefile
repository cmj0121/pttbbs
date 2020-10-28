include Makefile.in

SRC := $(wildcard r*.c)
OBJ := $(subst .c,.o,$(SRC))
BIN := $(subst .o,,$(OBJ))

CFLAGS  := $(CFLAGS) -DPTTBBS_UTIL
LDFLAGS := $(LDFLAGS) $(COMMON_LIB)

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN)

account: $(MBBSD_UTIL_OBJ)

%: %.c
	echo $^
	# $(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
