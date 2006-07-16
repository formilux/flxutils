OBJS=ifenslave-1.0.11 ifenslave-1.0.12 ifenslave
# include ../../include/rules.make
include ../include/rules.make
CC=gcc
CFLAGS+=-Wstrict-prototypes -I/usr/src/linux/include
#CFLAGS+=-Wstrict-prototypes -I/usr/src/linux/include -momit-leaf-frame-pointer

all:	$(OBJS)

clean:
	rm -vf $(OBJS) core *.[oa] *~

ifenslave:
	rm -f ifenslave
	ln -s ifenslave-1.0.12 ifenslave

ifenslave-1.0.11: ifenslave-1.0.11.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	# sstrip $@

ifenslave-1.0.12: ifenslave-1.0.12.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	# sstrip $@


%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

