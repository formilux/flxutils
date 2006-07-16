/*
 * SignFS : Tool to have an image of the filesystem
 *
 * Copyright (C) 1999-2001, Benoit Dolez <benoit@meta-x.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

/*
 * File: signfs.h
 * Object: structs and defines
 * Version: 0.3
 * Date: Sun Dec 23 14:21:50 CET 2001
 */

#ifndef __DATA_H__
#define __DATA_H__

#include "md5.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MD5_Init ToolsMD5Init
#define MD5_Update ToolsMD5Update
#define MD5_Final ToolsMD5Final
#define MD5_DIGEST_LENGTH 16

#define FLAG_TYPE       0x0001
#define FLAG_MODE       0x0002
#define FLAG_UID        0x0004
#define FLAG_GID        0x0008
#define FLAG_SIZE       0x0010
#define FLAG_MD5SUM     0x0020
#define FLAG_MTIME      0x0040
#define FLAG_LINK       0x0080
#define FLAG_RDEV       0x0010
#define FLAG_ALL        0x00ff

#define FILLED_FILE     0x01
#define FILLED_FS       0x02

#define SHOW_OLD        0x0100
#define SHOW_NEW        0x0200
#define SHOW_OTHERS     0x0400
#define SHOW_ALL        (SHOW_OLD|SHOW_NEW|SHOW_OTHERS)


#define O4(a) (htonl(*(int*)(a)))
#define FILE_TYPE(a) (S_ISLNK(a) ?'l':S_ISDIR(a) ?'d': \
		      S_ISCHR(a) ?'c':S_ISBLK(a) ?'b': \
		      S_ISFIFO(a)?'f':S_ISSOCK(a)?'s': \
		      S_ISREG(a) ?'-':'?')

#define HEXTODEC(a)  (('0'<=(a) && (a)<='9')?(a)-'0':(a)-'a'+10)

#define SPD(d) (*d?d:".")
#define off(t,field)   ((size_t)&(((t*)NULL)->field))

#define SHOWDIFF(flag) ((ShowDiff & flag) != 0)
#define SHOW(flag)     ((Show & flag) != 0)



// filenames descriptions
typedef struct s_fileinfo t_fileinfo ;

struct s_fileinfo {
    t_fileinfo		*parent;
    void		*files;
    char		*filename;
    char		*link;
    unsigned char	have_md5;
    unsigned char	md5sum[MD5_DIGEST_LENGTH];
    uid_t		uid;
    gid_t		gid;
    off_t		size;
    dev_t		rdev;
    mode_t		mode;
    time_t		mtime;
    ulong		flag;
    ulong		fsflag;
    int			filled;
    t_fileinfo		*diff;
};

t_fileinfo *build_fileinfo(char *, char *,struct stat *, t_fileinfo *);
void       print_fileinfo(t_fileinfo *,char *,int,FILE *);
t_fileinfo *fill_fileinfo(char *, char *, struct stat *, t_fileinfo *);
t_fileinfo *find_parent(char *,t_fileinfo *);

t_fileinfo *find_fileinfo(char *filename, t_fileinfo *current);
int read_file_fileinfo(t_fileinfo *data, FILE *file);
int read_directory_fileinfo(t_fileinfo *data, char *directory);
int read_directories_fileinfo(t_fileinfo *data, char **argv);
int delete_fileinfo(t_fileinfo *data);

#endif



