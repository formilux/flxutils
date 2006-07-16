#
# SignFS : Tool to have an image of the filesystem
#
# Copyright (C) 1999-2001, Benoit Dolez <benoit@meta-x.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
#

#
# File: Makefile
# Version: 0.6
# Date: Sun Dec 23 14:21:50 CET 2001
#


NAME=flx
VERSION=0.6.8
SRC=main.c arg.c utils.c md5.c flx.c fct1.c check.c sign.c \
    input.c input_fs.c input_file.c \
    output.c output_file.c
OBJ=$(SRC:.c=.o)
COPTS=-O2
CFLAGS= -Wall $(COPTS) -DMEM_OPTIM -DPROGRAM_NAME=\"$(NAME)\" -DPROGRAM_VERSION=\"$(VERSION)\"
LDFLAGS=-L.

all:	$(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(LDFLAGS) 
#	strip -R .note -R .comment signfs

clean:
	@$(RM) -vf $(NAME) $(OBJ) $(LIBTOOLS:.c=.o) *~ core *.bak

bkup:
	(cd .. ; tar zcvf $(NAME)-$(VERSION).tgz $(NAME)-$(VERSION)/*.{c,h} $(NAME)-$(VERSION)/[A-Z]* )


# DO NOT DELETE

arg.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
arg.o: /usr/include/gnu/stubs.h /usr/include/bits/types.h
arg.o: /usr/include/libio.h /usr/include/_G_config.h
arg.o: /usr/include/bits/stdio_lim.h /usr/include/string.h arg.h
check.o: /usr/include/string.h /usr/include/features.h
check.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
check.o: /usr/include/stdio.h /usr/include/bits/types.h /usr/include/libio.h
check.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h flx.h
check.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
check.o: /usr/include/sys/types.h /usr/include/time.h
check.o: /usr/include/netinet/in.h /usr/include/limits.h
check.o: /usr/include/stdint.h /usr/include/bits/wordsize.h
check.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
check.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
check.o: /usr/include/bits/in.h /usr/include/endian.h
check.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h utils.h
check.o: /usr/include/malloc.h md5.h check.h arg.h input.h source_type.h
check.o: output.h output_file.h input_file.h flx_fcntl.h input_fs.h
fct1.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
fct1.o: /usr/include/gnu/stubs.h /usr/include/bits/types.h
fct1.o: /usr/include/libio.h /usr/include/_G_config.h
fct1.o: /usr/include/bits/stdio_lim.h /usr/include/unistd.h
fct1.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
fct1.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
fct1.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/fcntl.h
fct1.o: /usr/include/bits/fcntl.h /usr/include/dirent.h
fct1.o: /usr/include/bits/dirent.h /usr/include/string.h
fct1.o: /usr/include/stdlib.h flx.h /usr/include/netinet/in.h
fct1.o: /usr/include/limits.h /usr/include/stdint.h
fct1.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
fct1.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
fct1.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
fct1.o: /usr/include/endian.h /usr/include/bits/endian.h
fct1.o: /usr/include/bits/byteswap.h utils.h /usr/include/malloc.h md5.h
flx.o: /usr/include/string.h /usr/include/features.h /usr/include/sys/cdefs.h
flx.o: /usr/include/gnu/stubs.h utils.h /usr/include/sys/stat.h
flx.o: /usr/include/bits/types.h /usr/include/bits/stat.h
flx.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/malloc.h
flx.o: /usr/include/stdio.h /usr/include/libio.h /usr/include/_G_config.h
flx.o: /usr/include/bits/stdio_lim.h flx.h /usr/include/netinet/in.h
flx.o: /usr/include/limits.h /usr/include/stdint.h
flx.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
flx.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
flx.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
flx.o: /usr/include/endian.h /usr/include/bits/endian.h
flx.o: /usr/include/bits/byteswap.h md5.h
input.o: /usr/include/string.h /usr/include/features.h
input.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h utils.h
input.o: /usr/include/sys/stat.h /usr/include/bits/types.h
input.o: /usr/include/bits/stat.h /usr/include/sys/types.h
input.o: /usr/include/time.h /usr/include/malloc.h /usr/include/stdio.h
input.o: /usr/include/libio.h /usr/include/_G_config.h
input.o: /usr/include/bits/stdio_lim.h source_type.h flx.h
input.o: /usr/include/netinet/in.h /usr/include/limits.h
input.o: /usr/include/stdint.h /usr/include/bits/wordsize.h
input.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
input.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
input.o: /usr/include/bits/in.h /usr/include/endian.h
input.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h md5.h
input.o: input.h flx_fcntl.h
input_file.o: /usr/include/stdio.h /usr/include/features.h
input_file.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
input_file.o: /usr/include/bits/types.h /usr/include/libio.h
input_file.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
input_file.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
input_file.o: /usr/include/bits/confname.h /usr/include/string.h
input_file.o: /usr/include/sys/types.h /usr/include/time.h
input_file.o: /usr/include/stdlib.h /usr/include/dirent.h
input_file.o: /usr/include/bits/dirent.h input_file.h utils.h
input_file.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
input_file.o: /usr/include/malloc.h flx.h /usr/include/netinet/in.h
input_file.o: /usr/include/limits.h /usr/include/stdint.h
input_file.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
input_file.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
input_file.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
input_file.o: /usr/include/endian.h /usr/include/bits/endian.h
input_file.o: /usr/include/bits/byteswap.h md5.h flx_fcntl.h
input_fs.o: /usr/include/stdio.h /usr/include/features.h
input_fs.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
input_fs.o: /usr/include/bits/types.h /usr/include/libio.h
input_fs.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
input_fs.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
input_fs.o: /usr/include/bits/confname.h /usr/include/string.h
input_fs.o: /usr/include/sys/types.h /usr/include/time.h
input_fs.o: /usr/include/dirent.h /usr/include/bits/dirent.h input_fs.h
input_fs.o: utils.h /usr/include/sys/stat.h /usr/include/bits/stat.h
input_fs.o: /usr/include/malloc.h flx.h /usr/include/netinet/in.h
input_fs.o: /usr/include/limits.h /usr/include/stdint.h
input_fs.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
input_fs.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
input_fs.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
input_fs.o: /usr/include/endian.h /usr/include/bits/endian.h
input_fs.o: /usr/include/bits/byteswap.h md5.h flx_fcntl.h
main.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
main.o: /usr/include/gnu/stubs.h /usr/include/bits/types.h
main.o: /usr/include/libio.h /usr/include/_G_config.h
main.o: /usr/include/bits/stdio_lim.h /usr/include/unistd.h
main.o: /usr/include/bits/posix_opt.h /usr/include/bits/confname.h
main.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
main.o: /usr/include/sys/types.h /usr/include/time.h /usr/include/fcntl.h
main.o: /usr/include/bits/fcntl.h /usr/include/dirent.h
main.o: /usr/include/bits/dirent.h /usr/include/string.h
main.o: /usr/include/stdlib.h flx.h /usr/include/netinet/in.h
main.o: /usr/include/limits.h /usr/include/stdint.h
main.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
main.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
main.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
main.o: /usr/include/endian.h /usr/include/bits/endian.h
main.o: /usr/include/bits/byteswap.h utils.h /usr/include/malloc.h md5.h
main.o: check.h arg.h sign.h
md5.o: /usr/include/string.h /usr/include/features.h /usr/include/sys/cdefs.h
md5.o: /usr/include/gnu/stubs.h md5.h
output.o: /usr/include/string.h /usr/include/features.h
output.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h utils.h
output.o: /usr/include/sys/stat.h /usr/include/bits/types.h
output.o: /usr/include/bits/stat.h /usr/include/sys/types.h
output.o: /usr/include/time.h /usr/include/malloc.h /usr/include/stdio.h
output.o: /usr/include/libio.h /usr/include/_G_config.h
output.o: /usr/include/bits/stdio_lim.h flx.h /usr/include/netinet/in.h
output.o: /usr/include/limits.h /usr/include/stdint.h
output.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
output.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
output.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
output.o: /usr/include/endian.h /usr/include/bits/endian.h
output.o: /usr/include/bits/byteswap.h md5.h output.h source_type.h
output.o: flx_fcntl.h
output_file.o: flx.h /usr/include/sys/stat.h /usr/include/features.h
output_file.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
output_file.o: /usr/include/bits/types.h /usr/include/bits/stat.h
output_file.o: /usr/include/sys/types.h /usr/include/time.h
output_file.o: /usr/include/netinet/in.h /usr/include/limits.h
output_file.o: /usr/include/stdint.h /usr/include/bits/wordsize.h
output_file.o: /usr/include/bits/socket.h /usr/include/bits/sockaddr.h
output_file.o: /usr/include/asm/socket.h /usr/include/asm/sockios.h
output_file.o: /usr/include/bits/in.h /usr/include/endian.h
output_file.o: /usr/include/bits/endian.h /usr/include/bits/byteswap.h
output_file.o: utils.h /usr/include/malloc.h /usr/include/string.h
output_file.o: /usr/include/stdio.h /usr/include/libio.h
output_file.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h md5.h
output_file.o: output_file.h input_file.h flx_fcntl.h check.h arg.h output.h
output_file.o: source_type.h
sign.o: /usr/include/stdio.h /usr/include/features.h /usr/include/sys/cdefs.h
sign.o: /usr/include/gnu/stubs.h /usr/include/bits/types.h
sign.o: /usr/include/libio.h /usr/include/_G_config.h
sign.o: /usr/include/bits/stdio_lim.h input.h source_type.h flx.h
sign.o: /usr/include/sys/stat.h /usr/include/bits/stat.h
sign.o: /usr/include/sys/types.h /usr/include/time.h
sign.o: /usr/include/netinet/in.h /usr/include/limits.h /usr/include/stdint.h
sign.o: /usr/include/bits/wordsize.h /usr/include/bits/socket.h
sign.o: /usr/include/bits/sockaddr.h /usr/include/asm/socket.h
sign.o: /usr/include/asm/sockios.h /usr/include/bits/in.h
sign.o: /usr/include/endian.h /usr/include/bits/endian.h
sign.o: /usr/include/bits/byteswap.h utils.h /usr/include/malloc.h
sign.o: /usr/include/string.h md5.h output.h arg.h input_fs.h flx_fcntl.h
sign.o: input_file.h output_file.h
utils.o: /usr/include/string.h /usr/include/features.h
utils.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
utils.o: /usr/include/stdio.h /usr/include/bits/types.h /usr/include/libio.h
utils.o: /usr/include/_G_config.h /usr/include/bits/stdio_lim.h
utils.o: /usr/include/dirent.h /usr/include/bits/dirent.h
utils.o: /usr/include/unistd.h /usr/include/bits/posix_opt.h
utils.o: /usr/include/bits/confname.h /usr/include/sys/types.h
utils.o: /usr/include/time.h /usr/include/fcntl.h /usr/include/bits/fcntl.h
utils.o: utils.h /usr/include/sys/stat.h /usr/include/bits/stat.h
utils.o: /usr/include/malloc.h
