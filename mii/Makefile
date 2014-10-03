OBJS=mii-diag
include ../include/rules.make
CC := $(CC_ORIG)
#CFLAGS+=-momit-leaf-frame-pointer

all:	$(OBJS)

%-diag:	%-diag.o libmii.o
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $^
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	#$(SSTRIP) $@

%.o:	%.c
	$(CC) -DLIBMII $(CFLAGS) -c -o $@ $<

