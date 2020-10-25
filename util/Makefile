include Makefile.in

SRC := $(wildcard *.c)
OBJ := $(subst .c,.o,$(SRC))
BIN := $(subst .c,,$(SRC))

CFLAGS  := $(CFLAGS) -DPTTBBS_UTIL
LDFLAGS := $(COMMON_LIB) $(LDFLAGS)

# the binary that could NOT build success
SKIP_BIN := cleanpasswd reaper uhash_loader writemoney

BIN := $(filter-out $(SKIP_BIN),$(BIN))

BIN_WITHOUT_UTIL := userlist

$(filter-out $(BIN_WITHOUT_UTIL),$(BIN)): LDFLAGS := $(LDFLAGS) $(MBBSD_UTIL_OBJ)

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ)

%: %.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
