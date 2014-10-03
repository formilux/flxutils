OBJS=ifenslave-1.0.11 ifenslave-1.0.12 ifenslave-1.1.0 ifenslave
include ../include/rules.make
CC := $(CC_ORIG)
CFLAGS+=-Wstrict-prototypes
#CFLAGS+=-D__uint32_t=uint32_t -D__uint16_t=uint16_t -D__uint8_t=uint8_t -Dcaddr_t=char*
#CFLAGS+=-Wstrict-prototypes -I/usr/src/linux/include -momit-leaf-frame-pointer

all:	$(OBJS)

ifenslave:
	rm -f ifenslave
	ln -s ifenslave-1.0.12 ifenslave

ifenslave%: ifenslave%.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	# sstrip $@

%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<

