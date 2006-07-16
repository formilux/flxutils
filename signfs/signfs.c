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
 * File: signfs.c
 * Object: Signing filesystem functions
 * Version: 0.5
 * Date: Sun Dec 23 14:21:50 CET 2001
 */

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>
#include "structure.h"
#include "signfs.h"
#include "others.h"

dev_t	SingleDev;
extern int SingleFileSystem,NoDate;
extern int CheckMode;
extern char *BaseFS,*BaseFile;
extern int ShowDiff,Show;
extern int HumanReadable;
extern int CurrentDir;
extern char *Output;

#define DIFF(f,field)  ((SHOWDIFF(f) && (datatemp.flag & (f))==(f) && \
                        data->field != datatemp.field)?(f):0)

// print the data to the standart output    
void print_fileinfo(t_fileinfo *data,char *filename,int recurs,FILE *output) {
    // recurs = 0 ne pas traiter la recursivité, écriture simple
    // recurs = 1 traiter la recursivité, écriture des différences possible
    // recurs = 2 ne pas traiter la récurisivité, écriture d'une différence
    int s,l,show=1;
    static char blk[8192];
    t_list *c;
    char   directory[8192];

    if (!filename || !data) return ;
    s=0;
    if (recurs == 2) {
	s+=sprintf(blk+s,"< ");
    }
    else if (ShowDiff != 0 && recurs == 1) {
	if (data->diff) {
	    print_fileinfo(data->diff,filename,2,output);
	    s+=sprintf(blk+s,"> ");
	} else {
	    if ((data->filled & (FILLED_FS|FILLED_FILE)) != (FILLED_FS|FILLED_FILE))
		if ((data->filled & FILLED_FS) && (Show & SHOW_NEW)!=0)
		    s+=sprintf(blk+s,"+ ");
		else if ((data->filled & FILLED_FILE) && (Show & SHOW_OLD)!=0)
		    s+=sprintf(blk+s,"- ");
		else show=0;
	    else if ((Show & SHOW_OTHERS) == 0) show=0;
	    else s+=sprintf(blk+s,"= ");
	}
    } 
    if ( ( show && data->filled) ) {
	if (SHOW(FLAG_TYPE)) 
	    s+=sprintf(blk+s,"%c",FILE_TYPE(data->mode));
	
	// s+=sprintf(blk+s,"%s ",right(data->mode));
	if (SHOW(FLAG_MODE)) {
	    if (HumanReadable)
		s+=sprintf(blk+s," %04o(%s) ",data->mode & 0007777,right(data->mode));
	    else
		s+=sprintf(blk+s," %04o ",data->mode & 0007777);
	}
	
	if (SHOW(FLAG_MODE))
	    s+=sprintf(blk+s,"%5d %5d ",data->uid,data->gid);
	if (SHOW(FLAG_SIZE)) { 
	    if (S_ISBLK(data->mode) || S_ISCHR(data->mode)) {
		char tmp[11];
		sprintf(tmp,"%d,%d",major(data->rdev),minor(data->rdev));
		s+=sprintf(blk+s,"%10s ",tmp);
	    } else {
		s+=sprintf(blk+s,"%10ld ",data->size);
	    }
	}
	if (SHOW(FLAG_MD5SUM)) {
	    if (data->have_md5) {
		s+=sprintf(blk+s,"%08x%08x",O4(data->md5sum),O4(data->md5sum+4));
		s+=sprintf(blk+s,"%08x%08x ",O4(data->md5sum+8),O4(data->md5sum+12));
	    } else {
		s+=sprintf(blk+s,"-------------------------------- ");
	    }
	}
	if (SHOW(FLAG_MTIME)) {
	    if (HumanReadable) {
		char *c;
		c=ctime(&(data->mtime));c[strlen(c)-1]=0;
		s+=sprintf(blk+s,"%10ld(%s) ",data->mtime,c);
	    }
	    else 
		s+=sprintf(blk+s,"%10ld ",data->mtime);
	}
	
	s+=sprintf(blk+s,"%s",*filename?filename:".");
	// s+=sprintf(blk+s,"%s",*filename?filename:"[current-directory]");
	
	if (SHOW(FLAG_LINK)) 
	    if (S_ISLNK(data->mode) && data->link) {
		s+=sprintf(blk+s," %s",data->link);
	    }
    }
    if (s > 0 && Output == NULL) { 
	// écriture sur la sortie standard
	write(1,blk,strlen(blk));
	write(1,"\n",1);
    } else if (s > 0) {            
	// écriture dans l'output
	fprintf(output,"%s\n",blk);
    } else if (Output != NULL) {   
	// écriture dans l'output y compris le fichier vide qui est la base
    }
    if (recurs == 1 && (c=data->files)) {
	strcpy(directory,filename);
	l=strlen(directory);
	if (l > 0 && directory[l-1] != '/')
	    strcpy(directory+l++,"/");
	while(c) {
	    strcpy(directory+l,((t_fileinfo*)(c->data))->filename);
	    print_fileinfo((void*)(c->data),directory,recurs,output);
	    c=c->next;
	}
    }
}

// file data struct with filename and stat
t_fileinfo *fill_fileinfo(char *filename, char *pfilename, 
		  struct stat *stat, t_fileinfo *data) {
    static MD5_CTX c;
    static char blk[8192];
    int fd,s,diff=0;
    static t_fileinfo datatemp;

    data->filename=strdup(pfilename);
    if (!stat) return (data);
    if (ShowDiff != 0) memcpy(&datatemp,data,sizeof(*data));
    data->filled|=FILLED_FS;
    data->fsflag=FLAG_ALL;
    data->mode=stat->st_mode;
    data->uid=stat->st_uid;
    data->gid=stat->st_gid;
    if (S_ISBLK(data->mode) || S_ISCHR(data->mode))
	data->rdev=stat->st_rdev;
    else if (!S_ISDIR(data->mode))
	data->size=stat->st_size;
    data->mtime=stat->st_mtime;
    if (S_ISREG(data->mode)) {
	if ((fd=open(SPD(filename),O_RDONLY)) < 0) {
	    pferror("open(%s) for checksum",SPD(filename));
	} else {
	    MD5_Init(&c);
	    while ((s=read(fd,blk,8192))>0) MD5_Update(&c,blk,s);
	    MD5_Final(data->md5sum,&c);
	    close(fd);
	    data->have_md5=1;
	}
    } else if (S_ISLNK(data->mode)) {
        int l;
	if ((l=readlink(SPD(filename),blk,8192)) < 0) {
	    pferror("readlink(%s)",SPD(filename));
	} else {
            blk[l]=0;
	    MD5_Init(&c);
	    MD5_Update(&c,blk,l);
	    MD5_Final(data->md5sum,&c);
	    data->have_md5=1;
	    data->link=strdup(blk);
	}
    }
    diff|=DIFF(FLAG_MODE,mode);
    diff|=DIFF(FLAG_TYPE,mode);
    diff|=DIFF(FLAG_UID,uid);
    diff|=DIFF(FLAG_GID,gid);
    diff|=DIFF(FLAG_SIZE,size);
    diff|=DIFF(FLAG_RDEV,rdev);
    diff|=DIFF(FLAG_MTIME,mtime);
    if (SHOWDIFF(FLAG_MD5SUM) && (datatemp.flag&FLAG_MD5SUM) && 
	((data->have_md5 == datatemp.have_md5 &&	
	  memcmp(data->md5sum,datatemp.md5sum,MD5_DIGEST_LENGTH)!=0) ||
	 data->have_md5 != datatemp.have_md5)) {
	diff|=FLAG_MD5SUM;
    }
    if (SHOWDIFF(FLAG_LINK) && (datatemp.flag&FLAG_LINK) && 
	strcmp(data->link,datatemp.link)) diff|=FLAG_LINK;
    if (SHOWDIFF(diff) && 
	(data->filled&(FILLED_FS|FILLED_FILE))==(FILLED_FS|FILLED_FILE)) {
	data->diff=malloc(sizeof(t_fileinfo));
	memcpy(data->diff,&datatemp,sizeof(t_fileinfo));
    }
    return (data);
}

// looking for directory in the current directory tree
t_fileinfo *find_parent(char *directory,t_fileinfo *current) {
    char *b,*r,bb[8192];
    t_list *c;
    int res;
    
    strcpy(r=bb,directory);
    while(current && *r) {
	b=r;
	if ((r=strchr(r,'/'))) { *r=0;r++; } 
	else return (current);
	res=1;
	for (c=current->files;c;c=c->next)
	    if ((res=strcmp(((t_fileinfo*)(c->data))->filename,b))>=0) break;
	if (res == 0) { // ok
	    // in the futur, check if directory
	    current=(void*)(c->data); 
	}
	else { // don't know, 
	    current=build_fileinfo(NULL,b,NULL,current);
	    current->mode |= S_IFDIR;
	}
    }
    return (current);
}

// find a file in the current directory
t_fileinfo *find_fileinfo(char *filename, t_fileinfo *current) {
    t_list *c;
    int res=1;

    if (!current) return (NULL);
    // fprintf(stderr,"recherche\n");
    for (c=current->files;c;c=c->next) {
	// fprintf(stderr,"%s <-> %s\n",((t_fileinfo*)(c->data))->filename,filename);
	if ((res=strcmp(((t_fileinfo*)(c->data))->filename,filename))>=0) break;
    }
    return (res==0?(t_fileinfo*)(c->data):NULL);
}

// build data from stat informations
t_fileinfo *build_fileinfo(char *filename, char *pfilename, 
		       struct stat *stat, t_fileinfo *parent) {
    t_fileinfo *data;

    if (!parent || !(data=find_fileinfo(pfilename,parent))) {
	// inexistant : creation requise
	data=malloc(sizeof(*data));
	bzero(data,sizeof(*data));
	data->parent=parent;
	fill_fileinfo(filename,pfilename,stat,data);
	if (parent)
	    list_put((void*)&(parent->files),(void*)data,off(t_fileinfo,filename),strcmp);
    } else {
	fill_fileinfo(filename,pfilename,stat,data);
    }
    return(data);
}

// had data from an other readed struct
void complete_fileinfo(t_fileinfo *data,t_fileinfo *n) {
    data->filename=n->filename;
    data->mode=n->mode;
    data->uid=n->uid;
    data->gid=n->gid;
    data->rdev=n->rdev;
    data->size=n->size;
    data->mtime=n->mtime;
    data->have_md5=n->have_md5;
    memcpy(data->md5sum,n->md5sum,MD5_DIGEST_LENGTH);
    data->link=n->link;
    data->flag=n->flag;
}

// get directory tree from file
int read_file_fileinfo(t_fileinfo *data, FILE *file) {
    char blk[8192],*b,*p,*f;
    int status,i,l,parenthesis;
    t_fileinfo *c;
    
    // reading line by line
    while(fgets(b=p=blk,8192,file)) {
	t_fileinfo *n=malloc(sizeof(*n));
	blk[strlen(blk)-1]=0;
	bzero(n,sizeof(*n));
	status=1;
	while(*p) {
	    parenthesis=0;
	    for (b=p;*p && ( *p!=' ' || parenthesis==1 ) ;p++) {
		if (*p == '(' && parenthesis==0) parenthesis=1;
		if (*p == ')' && parenthesis==1) parenthesis=0;
	    }
	    if (*p) 
		for (*p=0,p++;*p==' ';p++);
	    switch (status) {
	    case 1: 
		switch(*b) {
		case 'l':n->mode=S_IFLNK;break;
		case 'd':n->mode=S_IFDIR;break;
		case 'c':n->mode=S_IFCHR;break;
		case 'b':n->mode=S_IFBLK;break;
		case 'f':n->mode=S_IFIFO;break;
		case 's':n->mode=S_IFSOCK;break;
		case '-':n->mode=S_IFREG;break;
		default:break;
		}
		n->flag|=FLAG_TYPE;
		break;
	    case 2: 
		if ((f=strchr(b,'('))) { *f=0; }
		n->mode+=strtol(b,NULL,8); 
		n->flag|=FLAG_MODE; 
		break;
	    case 3: n->uid=atoi(b); n->flag|=FLAG_UID; break;
	    case 4: n->gid=atoi(b); n->flag|=FLAG_GID; break;
	    case 5: 
		if ((f=strchr(b,','))) {
		    *f=0;f++;
		    n->rdev=atoi(b)*256+atoi(f);
		} else {
		    n->size=atoi(b); 
		}
		n->flag|=FLAG_SIZE;
		break;
	    case 6: 
		n->have_md5=1;
		if (!strstr(b,"----"))
		    for (i=0;i<MD5_DIGEST_LENGTH;i++)
			n->md5sum[i]=(unsigned char)(HEXTODEC(b[i*2])*16+HEXTODEC(b[i*2+1]));
		else
		    n->have_md5=0;
		n->flag|=FLAG_MD5SUM;
		break;
	    case 7: 
		if ((f=strchr(b,'('))) { *f=0; }
		n->mtime=(time_t)atol(b); 
		n->flag|=FLAG_MTIME; 
		break;
	    case 8: 
		l=strlen(b);
		while (l>0 && b[l-1]=='/') b[--l]=0;
		n->parent=data;
		if (BaseFile) {
		    if (!strncmp(b,BaseFile,strlen(BaseFile)) && 
			(b[strlen(BaseFile)] == 0 || b[strlen(BaseFile)] == '/')) {
			b += strlen(BaseFile);
			if (*b == '/') b++;
			l = strlen(b);
		    }
		}
		if (l>0) {
		    if ((f=strrchr(b,'/'))) {
			n->parent=find_parent(b,data);
			f++;
		    } else {
			f=b;
		    }
		    n->filename=strdup(f);
		    c=list_put((void*)&(n->parent->files),(void*)n,
			       off(t_fileinfo,filename),strcmp);
		    n->filled|=FILLED_FILE;
		    if (c) { complete_fileinfo(c,n); free(n); n=c ; }
		}
		else {
		    n->filename="";
		    complete_fileinfo(data,n);
		    free(n);
		    n=data;
		    n->filled|=FILLED_FILE;
		}
		break;
	    case 9: n->link=strdup(b); n->flag|=FLAG_LINK; break;
	    default: break;
	    }
	    status++;
	}
    }
    return(1);
}

// reading data from filesystem
int read_directory_fileinfo(t_fileinfo *data, char *directory) {
    DIR *dir;
    t_fileinfo *tdata;
    struct dirent *dirent;
    struct stat stat;
    char *temp;
    int l;
    off_t position;

    l=strlen(directory);
    if (!(dir=opendir(SPD(directory)))) {
	pferror("opendir(%s)",SPD(directory));
	return(0);
    }
    if (*directory) {
      temp=malloc(strlen(directory)+1+NAME_MAX);
      memcpy(temp,directory,l);
      if (temp[l-1] != '/') strcpy(temp+l++,"/");
    } else {
      temp=malloc(NAME_MAX); 
      strcpy(temp,"");
    }
    while ((dirent = readdir(dir))) {
	if (dirent->d_name[0] == '.' && 
	    (dirent->d_name[1] == 0 ||
	     (dirent->d_name[1] == '.' && dirent->d_name[2] == 0))) continue;
	strcpy(temp+l,dirent->d_name);
	if (lstat(temp,&stat)) {
	    pferror("stat(%s)",temp);
	    continue;
	}
	tdata=build_fileinfo(temp,temp+l,&stat,data);
	if (S_ISDIR(stat.st_mode) &&  
	    !(SingleFileSystem && SingleDev!= stat.st_dev)) {
	    position=telldir(dir);
	    closedir(dir);
	    read_directory_fileinfo(tdata,temp);
	    dir=opendir(SPD(directory));
	    seekdir(dir,position);
	}
    }
    closedir(dir);
    free(temp);
    return(1);
}


// build directory tree from arg and filesystem
int read_directories_fileinfo(t_fileinfo *data, char **argv) {
    struct stat stat;
    char *temp="";
    char dfile[8192],*file;
    int l,dir=0;
    t_fileinfo *tdata,*pdata;

    if (*argv == NULL) {
	if (CurrentDir) {
	    if (lstat(".",&stat)) {
		pferror("stat(%s)",".");
		return(0);
	    }
	    SingleDev=stat.st_dev;
	    // logging for '.'
	    if (S_ISDIR(stat.st_mode)) {
		read_directory_fileinfo(data,".");
	    }
	}
    } else {
	while (*argv) {
	    strcpy(file=dfile,*argv);
	    l=strlen(file);
	    if (strcmp(file,"/")) {
		if (l>0 && file[l-1]=='/') { 
		    dir=1 ; 
		    file[--l]=0; 
		} else dir=0;
		if (BaseFS) {
		    // change root in the tree directory
		    l=strlen(BaseFS);
		    if (!strncmp(file,BaseFS,strlen(BaseFS)) && 
			(file[strlen(BaseFS)]==0 || file[strlen(BaseFS)]=='/')) {
			file += strlen(BaseFS);
			if (*file=='/') file++;
		    }
		}
	    }
	    if (lstat(dfile,&stat)) {
		error("lstat(%s)",dfile);
		return(0);
	    }
	    // keep the dev info for xdev option
            SingleDev=stat.st_dev;
	    if (*file) {
		if ((temp=dirname(file))) {  // looking for parent if it exist
		    pdata=find_parent(file,data);
		    // making directory
		    tdata=build_fileinfo(file,basename(file),&stat,pdata);
		} else {
		    pdata=data;
		    // making directory
		    tdata=build_fileinfo(file,file,&stat,pdata);
		}
	    } else {
		// if (!find_fileinfo(".",data))
		fill_fileinfo("","",&stat,data);
		tdata=data;
	    }
	    // had structure to data
	    if (S_ISDIR(stat.st_mode) || (dir && S_ISLNK(stat.st_mode))) {
		read_directory_fileinfo(tdata,dfile);
	    }
	    argv++;
	}
    }
    return(1);
}


// have to clean the fileinfo structure before existing
int delete_fileinfo(t_fileinfo *data) {
    return(1);
}

