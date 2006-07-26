OBJS=starfire-diag tulip-diag mii-diag
include ../include/rules.make
CC=gcc
CFLAGS+=-momit-leaf-frame-pointer

all:	$(OBJS)

%-diag:	%-diag.o libmii.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	sstrip $@

%.o:	%.c
	$(CC) -DLIBMII $(CFLAGS) -c -o $@ $<

