CC      ?= gcc
STRIP   ?= strip
OBJDUMP ?= objdump
SSTRIP  ?= sstrip
DIET	?= diet

CC_ORIG := $(CC)
override CC := $(DIET) $(CC)

CFLAGS=$(GCC_ARCH_SMALL) $(GCC_CPU_SMALL) $(GCC_OPT_SMALL)
#-mpreferred-stack-boundary=2 -malign-jumps=0 -malign-loops=0 -malign-functions=0 -Os -march=i386 -mcpu=i386
LDFLAGS=-s -Wl,--gc-sections

all:	$(OBJS)

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	$(OBJDUMP) -h $@ | grep -q '\.sbss[ ]*00000000' && $(STRIP) -R .sbss $@ || true
	-if [ -n "$(SSTRIP)" ]; then $(SSTRIP) $@ ; fi

%-debug:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DDEBUG -o $@ $<
	$(STRIP) -x --strip-unneeded -R .comment -R .note $@
	$(OBJDUMP) -h $@ | grep -q '\.data[ ]*00000000' && $(STRIP) -R .data $@ || true
	$(OBJDUMP) -h $@ | grep -q '\.sbss[ ]*00000000' && $(STRIP) -R .sbss $@ || true

clean:
	@rm -f *.[ao] *~ core
	@rm -f $(OBJS)
