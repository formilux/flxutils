OBJS=mktmp
include ../include/rules.make
#CFLAGS+=-momit-leaf-frame-pointer

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	#$(SSTRIP) $@
