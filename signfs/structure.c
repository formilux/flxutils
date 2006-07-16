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
 * File: structure.c
 * Object: list and hash functions
 * Version: 0.3
 * Date: Sun Dec 23 14:21:50 CET 2001
 */

#include <malloc.h>
#include "structure.h"
#include "others.h"

#define STR(a)  #a

// put data in a sorted list
// if two elements are the same (fct() return 0) return the known data
// if fct==NULL insert at the beginning of the list
void *list_put(t_list **first,char *item,int off,int (*fct)()) {
    t_list *n,*c=*first;
    int res;
    
    // create new element
    if (!(n=malloc(sizeof(t_list)))) {
	pferror("malloc: ");
	return (0);
    }
    n->data=item;
    n->next=n->prev=NULL;
    if (!c) *first=n;
    else {
	res=1;
	if (fct)
	    while((res=fct(*(char**)(c->data+off),*(char**)(n->data+off)))<0) {
		if (!c->next) break;
		c=c->next;
	    }
	else
	    res=1;
	if (res == 0) // data is the same
	    return ((void*)(c->data));
	else if (res<0) { // insert at the end
	    c->next=n;
	    n->prev=c;
	} 
	else { // insert at the beginning
	    n->next=c;
	    n->prev=c->prev;
	    c->prev=n;
	    if (!n->prev) *first=n;
	    else  n->prev->next=n;
	}
    }
    return (NULL);
}

