OBJS=remountr remountw
include ../include/rules.make
CFLAGS+=-fomit-frame-pointer

remountw : remountr
	ln -s $^ $@

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	#$(SSTRIP) $@
