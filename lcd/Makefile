OBJS=lcdwrite lcdtee
include ../include/rules.make
CFLAGS+=-fomit-frame-pointer

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	sstrip $@
