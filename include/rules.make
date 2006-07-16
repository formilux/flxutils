CC=diet gcc
CFLAGS=$(GCC_ARCH_SMALL) $(GCC_CPU_SMALL) $(GCC_OPT_SMALL)
#-mpreferred-stack-boundary=2 -malign-jumps=0 -malign-loops=0 -malign-functions=0 -Os -march=i386 -mcpu=i386
LDFLAGS=-s

all:	$(OBJS)

%:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o $@ $<
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	objdump -h $@ | grep -q '\.sbss[ ]*00000000' && strip -R .sbss $@ || true
	-sstrip $@

%-debug:	%.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DDEBUG -o $@ $<
	strip -R .comment -R .note $@
	objdump -h $@ | grep -q '\.data[ ]*00000000' && strip -R .data $@ || true
	objdump -h $@ | grep -q '\.sbss[ ]*00000000' && strip -R .sbss $@ || true

clean:
	@rm -f *.[ao] *~ core
	@rm -f $(OBJS)
