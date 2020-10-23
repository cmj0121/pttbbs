include Makefile.in

SRC := $(wildcard *.c)
OBJ := $(subst .c,.o,$(SRC))
LIB := libsys.a

.PHONY: all clean

all: $(LIB)		# build all necessary

clean: 			# clean-up temporary file
	rm -f $(LIB) $(OBJ)

help:  			# show this message
	@printf "Usage: make [OPTION]\n"
	@printf "\n"
	@perl -nle 'print $$& if m{^[\w-]+:.*?#.*$$}' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?#"} {printf "    %-18s %s\n", $$1, $$2}'

$(LIB): $(OBJ)
	$(AR) -csr $@ $^
