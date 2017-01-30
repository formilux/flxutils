OBJS=wdd
include ../include/rules.make
CFLAGS+=-fomit-frame-pointer

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -W -Wall -U_FORTIFY_SOURCE -o $@ $<
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	#-if [ -n "$(SSTRIP)" ]; then $(SSTRIP) $@ ; fi
