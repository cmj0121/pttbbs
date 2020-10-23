include Makefile.in

CORE  := bbs.c announce.c read.c board.c brc.c mail.c record.c fav.c
ABUSE := captcha.c
ACC   := user.c acl.c register.c passwd.c emaildb.c
NET   := mbbsd.c io.c term.c telnet.c nios.c
TALK  := friend.c talk.c ccw.c
UTIL  := stuff.c kaede.c convert.c name.c syspost.c cache.c cal.c
UI    := menu.c vtuikit.c psb.c
PAGER := more.c pmore.c
PLUG  := ordersong.c angel.c timecap.c
CHESS := chess.c chc.c chc_tab.c ch_go.c ch_gomo.c ch_dark.c ch_reversi.c ch_conn6.c
GAME  := chicken.c gamble.c

SCREEN := screen.c

SRC := var.c admin.c assess.c edit.c xyz.c var.c vote.c voteboard.c comments.c \
		$(CORE) $(ABUSE) $(ACC) $(NET) $(TALK) $(UTIL) $(UI) $(PAGER) $(PLUG) $(CHESS) $(GAME) \
		$(SCREEN)
OBJ := $(subst .c,.o,$(SRC)) vers.o
LIB := libmbbsd.a
BIN := mbbsd

CFLAGS  := $(CFLAGS)
LDFLAGS := $(LDFLAGS) $(COMMON_LIB)

.PHONY: all clean

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ) $(LIB) vers.c

TESTSZ := testsz

$(TESTSZ): $(TESTSZ).c $(SRCROOT)/include/*.h
	$(CC) $(CFLAGS) -o $@ $<

vers.c:
	sh $(SRCROOT)/util/newvers.sh

$(BIN): $(LIB)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

$(LIB): $(OBJ)
	$(AR) -csr $@ $^
