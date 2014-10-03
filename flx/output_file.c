#include <stdlib.h>
#include <string.h>
#include "flx.h"
#include "output_file.h"
#include "input_file.h"
#include "check.h"
#include "output.h"

#define STAT_WRITESUBDIR (1<<0)
#define STAT_DOINIT      (1<<1)
#define STAT_VIEWSORTED  (1<<2)

#define OPT_SORTED            0x01
#define OPT_WRITE             0x02
#define OPT_LS                0x04

/* affiche le contenu d'un répertoire à partir de l'emplacement spécifié en
 * reprenant son nom 
 */
int   output_file_write_(t_file_status *desc, t_ft *tree, 
			 int number, char *(*desc_to_str)()) {
    char         *pcpath, *pbpath;
    int          count = 0;
    
    /* output initialization */
    if (IS(desc->status, STAT_DOINIT)) {
	UNSET(desc->status, STAT_DOINIT);
    }

    /* nothing have change, do not free */
    if (number > 0 && 
	IS(Sorted, OUTPUT_SORTED) && !IS(Sorted, INPUT_SORTED)) return (0);

    /* initialize path pointers */
    pcpath = desc->cpath + strlen(desc->cpath);
    pbpath = desc->bpath + strlen(desc->bpath);

    tree = ft_get(tree, desc->cpath);
    /* find a valid directory */
    while (!IS(tree->status, CHANGED) && SIMPLE_LIST_NEXT(desc->fdstack)) {
	pcpath = REMOVE_DIR(desc->cpath, pcpath); *pcpath = 0;
	pbpath = REMOVE_DIR(desc->bpath, pbpath); *pbpath = 0;
	desc->recursion--;
	tree = tree->parent;
    }
    
    /* start from changed subdirectory */
    if (!tree->subtree || !IS(tree->status, CHANGED) || IS(tree->status, SORTING)) {
	return (0);
    }
    tree = tree->subtree->next;

    while (desc->fdstack) {
	
	/* write all in the subdirectory */
	while (!IS(tree->status, BASE)) {

	    /* nothing to do, pass */
	    if (!IS(tree->status, CHANGED))  { tree = tree->next; continue; } 

	    /* the status have change, something to do with it, update path */
	    ADD_PATH(desc->cpath, pcpath, tree->filename);
	    
	    /* filename have been newly filled */
	    if (IS(tree->status, FILLED)) {
		char *str = desc_to_str(desc->cpath, pcpath, tree->desc);
		if (str) {
		    fprintf(desc->fd, "%s\n", str);
		    count++;
		}
	    }

	    /* stop recursion when an output is sorted and 
	     * and this directory isn't finished to fill */
	    if (number > 0 && 
		IS(desc->status, STAT_VIEWSORTED) && IS(tree->status, SORTING)) {
		*pcpath = 0;
		return (count);
	    }

	    /* treat sub directory, not for back pointers (. and ..) */
	    if (tree->subtree && !IS_DOTF(tree->filename)) {
		
		/* add backup file descriptor for this level */
		SIMPLE_LIST_PUSH(desc->fdstack, desc->fd);

		/* backup directory filename and new position */
		pcpath += strlen(pcpath);

		if (desc->recursion > 0) { 
		    FILE   *newfd;
		    
		    /* build new path, and new fd and so continue */
		    pbpath = ADD_PATH(desc->bpath, pbpath, tree->filename);
		    
		    if (mkdir_with_parent(desc->bpath, ~GET_UMASK() & 0777)) {
			/* open new fd for this directory */
			ADD_PATH(desc->bpath, pbpath, DUMPBASENAME);
			if (!(newfd = fopen(desc->bpath, "w")))
			    PFERROR("fopen(%s)", desc->bpath);
			else 
			    desc->fd = newfd;
		    }
		    /* remove filename from bpath */
		    *pbpath = 0; 
		    
		}
		
		/* recursion level (can be negative) */
		desc->recursion--;
		
		/* go subtree */
		tree = tree->subtree->next;

		continue;
	    }
	    
	    /* look for next */
	    tree = tree->next;
	}
	/* remove added filenames */
	*pcpath = 0;
	
	/* verify that all files are readed */
	if (number > 0 && 
	    IS(tree->parent->status, READING)) return (count);
	
	/* search old path */
	pcpath = REMOVE_DIR(desc->cpath, pcpath); *pcpath = 0;

	/* recursion level : up */
	desc->recursion++;
	/* if recursion level, can close old fd */
	if (desc->recursion > 0) {
	    pbpath = REMOVE_DIR(desc->bpath, pbpath); *pbpath = 0;
	    fclose(desc->fd);
	}

	/* get last fd */
	desc->fd = SIMPLE_LIST_POP(desc->fdstack);
	
	/* restore backuped position */
	tree = tree->parent->next;
    }
    return (count);
}

int   output_file_write(t_file_status *desc, t_ft *tree, int number) {
	if ((desc->options & OPT_LS) == OPT_LS)
		return (output_file_write_(desc, tree, number, show_filename));
	else
    return (output_file_write_(desc, tree, number, build_line));
}


/* traduct a string options descriptions to flags
 */
int         output_file_fcntl(t_file_status *env, int cmd) {
    return (0);
}


t_file_status  *output_file_open(char *pathname, char *opts) {
    t_file_status  *new;
    char           *filename;
    int            ret;
    char           tmp[BUFFER_LENGTH];
    char           *ppath, *popts, *pvalue;
    int            options = (OPT_SORTED|OPT_WRITE);
    
    popts = opts ? strcpy(tmp, opts) : NULL;
    ppath = pathname ? strcpy(tmp + strlen(tmp) + 1, pathname) : NULL;

    /* memory allocation for new element */
    new = MALLOC(sizeof(*new));
    bzero(new, sizeof(*new));
    new->recursion = 0;

    /* part to get source specific options */
    while (popts) {
	int  sign = 1;
	char *t;
	
	/* select an option and its value */
	if ((t = strchr(popts, '+'))) *t++ = 0;
	if ((pvalue = strchr(popts, '='))) *pvalue++ = 0;
	
	/* remove negative */
	while (*popts == '!') { sign = -sign ; popts++; }
	
	/* treat option */
	if (!*popts) continue;
	else if (!strcmp(popts, "sort"))  /* sorted flag */
	    options = (sign > 0 ? (options|OPT_SORTED) : (options&~OPT_SORTED));
	else if (!strcmp(popts, "level") && pvalue) /* recurse level */
	    new->recursion = atoi(pvalue);
		else if (!strcmp(popts, "ls"))    /* just list flag */
			options = options|OPT_LS;
	else {
	    error("unknown option : %s", popts);
	    FREE(new);
	    return (0);
	}
	popts = t;
    }

	new->options = options;

    filename = ppath;
    if (ppath)
	ppath = backslashed_strmchr(ppath, DELIM_LIST);

    SET(new->status, STAT_VIEWSORTED); /* default is sorted */
    SET(new->status, STAT_DOINIT);

    if (!filename || !filename[0] || !strcmp(filename, "-")) { 
	/* special case for stdout */
	new->fd = stdout;
    }
    else {
	struct stat st;
	/* case for files, should stat them */
	if (!(ret = stat(filename, &st)) && S_ISDIR(st.st_mode)) {
	    /* directory, directory treatment */

	    /* build base filename path */
	    strcpy(new->bpath, filename);
	    filename = new->bpath + strlen(new->bpath);
	    ADD_PATH(new->bpath, filename, DUMPBASENAME);
	    /* open the file */
	    if (!(new->fd = fopen(new->bpath, "w"))) {
		PFERROR("fopen(%s)", new->bpath);
		exit(3);
	    }
	    *filename = 0;
	}
	else {
	    /* can get confirmation */
	    /* can test and open */
	    if (!(new->fd = fopen(filename, "w"))) {
		PFERROR("fopen(%s)", filename);
		exit(3);
	    }
	}
    }
    /* initialise file descriptor stack */
    new->fdstack = NULL;
    SIMPLE_LIST_PUSH(new->fdstack, new->fd);
    
    return (new);
}

t_file_status  *output_file_close(t_file_status *old) {
    while (old->fdstack && old->recursion < 0) {
	SIMPLE_LIST_POP(old->fdstack);
	old->recursion--;
    }
    if (old->fd && old->fd != stdout) fclose(old->fd);
    while (old->fdstack) {
	old->fd = SIMPLE_LIST_POP(old->fdstack);
	fclose(old->fd);
    }
    FREE(old);
    return (NULL);
}

t_file_status  *output_stdout_open(char *desc, char *opts) {
    if ((desc = backslashed_strmchr(desc, DELIM_LIST)))
	return (output_file_open(desc, opts));
    return (output_file_open("", opts));
}



