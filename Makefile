include Makefile.in

SUBDIR := common mbbsd util

.PHONY: all clean help $(SUBDIR)

all: $(SUBDIR)		# build all necessary

clean: $(SUBDIR)	# clean-up temporary file
	rm -f $(VAR_H)

help:  				# show this message
	@printf "Usage: make [OPTION]\n"
	@printf "\n"
	@perl -nle 'print $$& if m{^[\w-]+:.*?#.*$$}' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?#"} {printf "    %-18s %s\n", $$1, $$2}'

$(SUBDIR): $(VAR_H)
	@ln -sf ../Makefile.in $@/Makefile.in
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(VAR_H): $(VAR_C)	# generate the basic variable header
	perl $(SRCROOT)/util/parsevar.pl < $< > $@

mbbsd util: common
util: $(MBBSD_UTIL_OBJ)
$(MBBSD_UTIL_OBJ): $(VAR_C)
