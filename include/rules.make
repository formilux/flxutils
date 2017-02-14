CC      ?= gcc
STRIP   ?= strip
OBJDUMP ?= objdump
SSTRIP  ?= sstrip
UCLIBC	?= uclibc

CC_ORIG := $(CC)

ifeq ($(NOLIBC),)
override CC := $(UCLIBC) $(CC) -Os
else
override CC := $(CC) -Os -nostdlib -include $(NOLIBC)
endif

CFLAGS=$(GCC_ARCH_SMALL) $(GCC_CPU_SMALL) $(GCC_OPT_SMALL) -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident
#-mpreferred-stack-boundary=2 -malign-jumps=0 -malign-loops=0 -malign-functions=0 -Os -march=i386 -mcpu=i386
LDFLAGS=-s -Wl,--gc-sections

all:	$(OBJS)

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $< -lgcc
	$(STRIP) -x --strip-unneeded -R .comment -R .note -R .note.ABI-tag $@
	$(STRIP) -R .eh_frame_hdr -R .eh_frame $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	$(OBJDUMP) -h $@ | grep -q '\.sbss[ ]*00000000' && $(STRIP) -R .sbss $@ || true
	#-if [ -n "$(SSTRIP)" ]; then $(SSTRIP) $@ ; fi

%-debug:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DDEBUG -o $@ $< -lgcc
	$(STRIP) -x --strip-unneeded -R .comment -R .note -R .note.ABI-tag $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	$(OBJDUMP) -h $@ | grep -q '\.sbss[ ]*00000000' && $(STRIP) -R .sbss $@ || true

clean:
	@rm -f *.[ao] *~ core
	@rm -f $(OBJS)
