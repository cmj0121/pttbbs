include Makefile.in

SUBDIR := common mbbsd

.PHONY: all clean help $(SUBDIR)

all: $(SUBDIR)		# build all necessary

clean: $(SUBDIR)	# clean-up temporary file

help:  				# show this message
	@printf "Usage: make [OPTION]\n"
	@printf "\n"
	@perl -nle 'print $$& if m{^[\w-]+:.*?#.*$$}' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?#"} {printf "    %-18s %s\n", $$1, $$2}'

$(SUBDIR): $(VAR_H)
	@ln -sf ../Makefile.in $@/Makefile.in
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(VAR_H):	# generate the basic variable header
	perl $(SRCROOT)/util/parsevar.pl < $(SRCROOT)/mbbsd/var.c > $(SRCROOT)/include/var.h

mbbsd: common
