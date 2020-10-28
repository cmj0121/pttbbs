SUBDIR := bbs osdep sys

.PHONY: all clean help $(SUBDIR)

all: $(SUBDIR)		# build all necessary

clean: $(SUBDIR)	# clean-up temporary file

help:  				# show this message
	@printf "Usage: make [OPTION]\n"
	@printf "\n"
	@perl -nle 'print $$& if m{^[\w-]+:.*?#.*$$}' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?#"} {printf "    %-18s %s\n", $$1, $$2}'

$(SUBDIR):
	@ln -sf ../Makefile.in $@/Makefile.in
	$(MAKE) -C $@ $(MAKECMDGOALS)
