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
# Version: 0.3
# Date: Sun Dec 23 14:21:50 CET 2001
#


NAME=signfs
SRC=main.c signfs.c structure.c
OBJ=$(SRC:.c=.o)
COPTS=-Os -march=i386
CFLAGS=$(COPTS) -Wall
LDFLAGS=-L. -ltools
LIBTOOLS=md5.c libtools.c others.c

$(NAME): libtools.a $(OBJ)
	$(CC) -o $(NAME) $(OBJ) $(LDFLAGS) 
	#strip -R .note -R .comment signfs

libtools.a: $(LIBTOOLS:.c=.o)
	$(AR) -r libtools.a $(LIBTOOLS:.c=.o)

clean:
	@$(RM) -vf $(NAME) $(OBJ) $(LIBTOOLS:.c=.o) *~ libtools.a core *.bak


