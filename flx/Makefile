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
# Version: 0.7.2
# Date: Thu Jul 20 20:54:50 CET 2006
#


NAME=flx
VERSION=0.7.2
SRC=main.c arg.c utils.c md5.c flx.c fct1.c check.c sign.c \
    input.c input_fs.c input_file.c \
    output.c output_file.c
OBJ=$(SRC:.c=.o)
COPTS=-O2 -fno-strict-aliasing
CFLAGS= -Wall $(COPTS) -DMEM_OPTIM -DPROGRAM_NAME=\"$(NAME)\" -DPROGRAM_VERSION=\"$(VERSION)\" -U_FORTIFY_SOURCE
LDFLAGS=-L. -Wl,--gc-sections

all:	$(NAME)

$(NAME): $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(LDFLAGS) 
#	strip -R .note -R .comment signfs

clean:
	@$(RM) -vf $(NAME) $(OBJ) $(LIBTOOLS:.c=.o) *~ core *.bak

bkup:
	(cd .. ; tar zcvf $(NAME)-$(VERSION).tgz $(NAME)-$(VERSION)/*.{c,h} $(NAME)-$(VERSION)/[A-Z]* )

