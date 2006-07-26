OBJS=ifenslave
include ../../include/rules.make
CC=gcc
CFLAGS+=-Wstrict-prototypes -I/usr/src/linux/include -momit-leaf-frame-pointer

all:	$(OBJS)

ifenslave: ifenslave.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	# sstrip $@

%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

