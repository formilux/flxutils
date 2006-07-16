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
 * File: main.c
 * Object: Signing filesystem
 * Version: 0.5
 * Date: Sun Dec 23 14:21:50 CET 2001
 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "signfs.h"
#include "others.h"

int CheckMode=0,Argc;
char *source_filename=0,**Argv;
int SingleFileSystem=0,NoDate=0,ShowDiff=0,Show=0xffff,HumanReadable=0;
char *Checkit=NULL;
char *BaseFS=NULL,*BaseFile=NULL,*BaseStrip=NULL;
int CurrentDir=1,OutputLevel=0;
char *Output=NULL;

int read_param(int argc, char **argv, char ***list) {
    int ignore=0,show=0xffff-SHOW_OTHERS;
    
    Argc=argc; Argv=argv;
    ++argv;
    while (argc > 1) {
      if (**argv == '-') {
        if (*(*argv+1) == '-') {
          if (!strcmp(*argv+2,"")) break;
          if (!strcmp(*argv+2,"help")) return (-1);
	  else if (!strcmp(*argv+2,"strip"))
	      if (*(argv+1)) BaseStrip=*(argc--,++argv);
	      else return (-1);
	  else if (!strcmp(*argv+2,"base"))
	      if (*(argv+1)) BaseFile=*(argc--,++argv);
	      else return (-1);
	  else if (!strcmp(*argv+2,"root"))
	      if (*(argv+1)) BaseFS=*(argc--,++argv);
	      else return (-1);
	  else if (!strcmp(*argv+2,"check"))          ShowDiff=0xffff;
	  else if (!strcmp(*argv+2,"ignore-perm"))    ignore|=(FLAG_MODE|FLAG_UID|FLAG_GID);
	  else if (!strcmp(*argv+2,"ignore-date"))    ignore|=FLAG_MTIME;
	  else if (!strcmp(*argv+2,"ignore-check"))   ignore|=FLAG_MD5SUM;
	  else if (!strcmp(*argv+2,"ignore-size"))    ignore|=FLAG_SIZE;
	  else if (!strcmp(*argv+2,"ignore-link"))    ignore|=FLAG_LINK;
	  else if (!strcmp(*argv+2,"new-only"))       { show-=SHOW_OLD; show|=SHOW_NEW ; }
	  else if (!strcmp(*argv+2,"old-only"))       { show-=SHOW_NEW; show|=SHOW_OLD ; }
	  else if (!strcmp(*argv+2,"show-all"))       { show|=SHOW_ALL ; }
	  else if (!strcmp(*argv+2,"human-readable")) { HumanReadable=1 ; }
	  else if (!strcmp(*argv+2,"output-level")) {
	      if (*(argv+1)) OutputLevel=atoi(*(argc--,++argv));
	      else return (-1);
	  }
          else return (-1);
        }
        else if (!strcmp(*argv+1,"h")) return (-1);
	else if (!strcmp(*argv+1,"o"))
	    if (*(argv+1)) Output=*(argc--,++argv);
	    else return (-1);
	else if (!strcmp(*argv+1,"b"))
	    if (*(argv+1)) BaseFile=*(argc--,++argv);
	    else return (-1);
	else if (!strcmp(*argv+1,"r"))
	    if (*(argv+1)) BaseFS=*(argc--,++argv);
	    else return (-1);
	else if (!strcmp(*argv+1,"c")) ShowDiff=0xffff;
        else if (!strcmp(*argv+1,"f"))
	    if (argv+1 && *(argv+1))
		source_filename=*(argc--,CheckMode=1,++argv);
	    else return (-1);
        else if (!strcmp(*argv+1,"x")) SingleFileSystem=1;
        else { 
	    printf("Bad option: %s\n",*argv); 
	    return (-1); 
	}
      } else {
        break;
      }
      argc--;
      ++argv;
    }
    *list=argv;
    Show=show;
    ShowDiff &= ~ignore;
    return (argc);
}

int syntax() {
    printf("signfs-0.5, copyright Benoit DOLEZ <benoit@ant-computing.com>\n");
    printf("Usage %s [options] [file] ...\n",Argv[0]);
    printf("  -x               : only designed file system\n");
    printf("  -b <dir>         : base during file reading\n"); 
    printf("  -r <dir>         : base during directory reading\n"); 
    printf("  --base <dir>     : base during file reading\n"); 
    printf("  --root <dir>     : base during directory reading\n"); 
    printf("  --strip <dir>    : base during showing result\n"); 
    printf("  -c --check       : check consistancy (use with -f)\n");
    printf("  -f <file>        : read old map from file ('-' for stdin)\n");
    printf("  -h --help        : this help\n");
    printf("  --ignore-date    : ignore date (use with -c)\n");
    printf("  --ignore-check   : ignore checksum (use with -c)\n");
    printf("  --ignore-perm    : ignore perm (mode and owner) (use with -c)\n");
    printf("  --ignore-size    : ignore size (use with -c)\n");
    printf("  --ignore-link    : ignore link (use with -c)\n");
    printf("  --show-all       : show same and difference (use with -c)\n");
    printf("  --only-new       : show only new files (use with -c)\n");
    printf("  --only-old       : show only old files (use with -c)\n");
    printf("  --human-readable : show long format for right and date\n");
    return (1);
}


int main(int argc, char **argv) {
    t_fileinfo data;
    char **begin;
    FILE *source; 

    bzero(&data,sizeof(data));
    if (read_param(argc,argv,&begin) <= 0)
	return (syntax());
    if (CheckMode) {
	// read table
	if (source_filename && strcmp(source_filename,"-")) {
	    if (!(source=fopen(source_filename,"ro")))
		return (fatal_error("Fichier source inexistant : %s\n",
				    source_filename));
	} else {
	    source=stdin;
	}
	read_file_fileinfo(&data,source);
	if (source != stdin) fclose(source);
    }
    read_directories_fileinfo(&data, begin);
    if (BaseStrip) 
	print_fileinfo(find_fileinfo(basename(BaseStrip),find_parent(BaseStrip,&data)),"",1,NULL);
    else  
	print_fileinfo(&data,"",1,NULL);
    delete_fileinfo(&data);
    return (0);
}



