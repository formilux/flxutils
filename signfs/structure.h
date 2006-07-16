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
 * File: structure.h
 * Object: list and hash functions prototypes
 * Version: 0.3
 * Date: Sun Dec 23 14:21:50 CET 2001
 */


#ifndef __STRUCTURE_H__
#define __STRUCTURE_H__

typedef struct s_list t_list;

struct s_list {
    t_list *prev;
    t_list *next;
    char   *data;
}; 


typedef struct s_hash t_hash;
#define HASHSIZE   103

struct s_hash {
    t_list *tab[HASHSIZE];
    int    (*fct)();
};

void *list_put(t_list **,char *,int,int (*)());

#endif


