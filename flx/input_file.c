#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <ctype.h>

#include "input_file.h"
#include "utils.h"
#include "flx.h"


#define STAT_NOTREAD          0x01
#define STAT_MULTI_FILES      0x02

#define OPT_SORTED            0x01
#define OPT_READ              0x02


char   **get_field1(char *line) {
    static char *tab[10];
    char        *pline = line;
    int         i = 0;
    
    while (*line && i < 9) {
	/* pass spaces */
	while (*line == ' ' || *line == '\t') line++;
	/* verify */
	if (!*line) break;
	/* backup pointer */
	tab[i] = pline;
	/* look for data */
	while (*line && *line != ' ' && *line != '\t') { 
	    /* pass '(' in value fields */
	    if (*line == '(') {
		*line++ = 0;
		while (*line && *line != ')') line++;
		break;
	    }
            /* pass backslashed characters */
            if (*line == '\\' && *(line+1)) { 
		*pline++ = *(line+1); 
		line += 2 ; 
	    }
			else if (*line == '%' && isxdigit(*(line+1)) && isxdigit(*(line+2))) {
				*pline++ = HEX2DEC(line[1])*16+HEX2DEC(line[2]);
				line += 3;
			}
            else {
		*pline++ = *line++ ;
	    }
	}
	if (*line) line++;
	*pline++ = 0;
	i++;
    }
    tab[i] = NULL;
    return (tab);
}

/* treat a line, build new tree and compile with current 
 * tab is a tab of char that finish with a NULL string
 * filename is a pointer address to a filename 
 * desc is the current desc pointer 
 */
t_file_desc *fill_from_signfs_line(char **tab, char **rpath, t_file_desc **desc) {
    char         *p1, temp[64];
    struct stat  *st;
    int          nfield, i;
    
    /* just want filename */
    if (!desc) {
	/* search the 7th position */
	for (i=0; (*rpath = tab[i]) && i < 7 ; i++);
	/* passe bad saved ./ and ../ */
	if (i == 7) {
	    p1 = NULL;
	    while (p1 != *rpath) {
		p1 = *rpath;
		if (!strncmp(*rpath, "./", 2))  *rpath += 2;
		if (!strncmp(*rpath, "../", 3)) *rpath += 3;
	    }
	}
	return (NULL);
    }
    /* want to complete data */
    if (!*desc)   { 
	*desc = MALLOC(sizeof(**desc));
	bzero(*desc, sizeof(**desc));
    }
    st = &((*desc)->stat);
    for(nfield = 0; (p1 = tab[nfield]); nfield++) {
	
	switch (nfield) {
	case 0: /* file type */
	    st->st_mode |= (*p1 == 'l' ? S_IFLNK : 
			    *p1 == 'd' ? S_IFDIR :
			    *p1 == 'c' ? S_IFCHR :
			    *p1 == 'b' ? S_IFBLK :
			    *p1 == 'f' ? S_IFIFO :
			    *p1 == 's' ? S_IFSOCK :
			    *p1 == '-' ? S_IFREG : 0);
	    break;
	case 1: /* file mode */
	    st->st_mode |= strtol(p1, NULL, 8); break; 
	case 2: /* user id */
	    st->st_uid = atoi(p1); break;
	case 3: /* group id */
	    st->st_gid = atoi(p1); break;
	case 4: /* size or major/minor for devices */
	    strncpy(temp, p1, 64); temp[63] = 0;
	    if ((p1 = strchr(temp, ','))) {
		*p1++ = 0; 
		st->st_rdev = atoi(temp)*256 + atoi(p1);
	    }
	    else
		st->st_size = atol(temp);
	    break;
	case 5: /* checksum */
	    if (p1[0] != '-') {
		for (i = 0 ; p1[i] ; i++) 
		    temp[i] = (unsigned char)
						(HEX2DEC(p1[i * 2]) * 16 + HEX2DEC(p1[i * 2 + 1]));
		(*desc)->md5 = MALLOC(i);
		memcpy((*desc)->md5, temp, i);
	    }
	    break;
	case 6: /* modification time */
	    st->st_mtime = (time_t)atol(p1); break;
	case 7: /* filename (p1 is path, p2 is filename) */
	    if (rpath) *rpath = p1;
	    break;
	case 8: /* link */
	    (*desc)->link = strdup(p1);
	    break;
	}
    }
    return (*desc);
}



int   input_file_read_files(t_file_status *desc, t_ft *tree, int cmax) {
    char  line[BUFFER_LENGTH], **tab;
    char  *filename, *path;
    int   count = 0;
    t_ft  *new;
    /* variable used for acceleration */
    char  oldpath[BUFFER_LENGTH] = "", *poldpath = oldpath;
    t_ft  *oldtree;
    
    if (!desc->fd || feof(desc->fd)) return (0);
    
    /* possibly have to mark current working directory has SORTING */
    strcpy(oldpath, desc->cpath);
    poldpath = oldpath + strlen(oldpath);
    oldtree = ft_get(tree, oldpath);
    SET(oldtree->status, READING);
    
    if (IS(desc->status, STAT_MULTI_FILES)) {
	SET(oldtree->status, SORTING);
    }

    /* read count entry in file */
    while (!cmax || count < cmax) {
	/* read one line from current fd */
	if (!fgets(line, BUFFER_LENGTH, desc->fd)) break;
	    
	line[ strlen(line)-1 ] = 0;
	/* split into field */
	tab = get_field1(line);
	
	/* just get filename */
	fill_from_signfs_line(tab, &path, NULL);
	if (!path) continue;
		    
	/* look for required string in path */
	/* remove leading '/' */
	filename = path + strlen(path) - 1;
	while (*filename == '/') *filename-- = 0;
	/* find filename into path */
	if (!(filename = strrchr(path, '/'))) filename = path;
	else  			          filename++;

	/* options tell to ignore '.' and '..' */
	if (IS(Options, GOPT_IGNORE_DOT) && IS_DOTF(filename)) 
	    continue;
	
	/* look for old definition */
	if (filename != path && (poldpath - path) == (filename - path - 1) && 
	    !strncmp(oldpath, path, filename - path - 1)) {
	    new = ft_get(oldtree, filename);
	    // SET(oldtree->status, READING);
	}
	else {
	    /* look for this object from base */
	    new = ft_get(tree, path);
	    UNSET(oldtree->status, READING);
	    oldtree = new->parent;
	    strcpy(oldpath, build_path(oldtree));
	    poldpath = oldpath + strlen(oldpath);
	    SET(oldtree->status, READING);
	}
	
	/* complete information from line */
	new->desc = fill_from_signfs_line(tab, NULL, (void*)&new->desc);

	/* mark tree as changed and completed */
	SET(new->status, FILLED);
	SET_PARENT(new, CHANGED);
	
	/* file counter */
	count++;
	if (cmax && count >= cmax) return (count);
    }
    /* end of file, close it */
    if (desc->fd != stdin) fclose(desc->fd);
    desc->fd = NULL;

    return (count);
}

FILE  *input_file_set_next_file(t_file_status *desc, t_ft *tree) {
    char *pcpath, *pbpath;
    t_ft *ctree;
    
    /* something to read or no more dir to read */
    if (desc->fd || !desc->dirnames) return (desc->fd);
    
    /* restore/build tree position for cpath */
    tree = ft_get(tree, desc->cpath);
    
    /* initialize pointer to end of path */
    pcpath = desc->cpath + strlen(desc->cpath);
    pbpath = desc->bpath + strlen(desc->bpath);

    while (1) {

	/* mark directory as unsorted */
	SET(tree->status, SORTING);
	/* get directory entries if never done */
	if (IS(desc->status, STAT_NOTREAD)) {
	    DIR            *dir;
	    struct dirent  *dirent;
	    struct stat    st;
	    
	    /* unset all previous flag */
	    UNSET(desc->status, STAT_MULTI_FILES);

	    if (!(dir = opendir(desc->bpath)))
		PFERROR("opendir(%s)", desc->bpath);
	    else {
		while ((dirent = readdir(dir))) {
		    
		    /* build path to work with it */
		    ADD_PATH(desc->bpath, pbpath, dirent->d_name);

		    if (stat(desc->bpath, &st)) {
			/* stat filename to find directory */
			PFERROR("stat(%s)", desc->bpath); 
		    }
		    /* check for specfic files */
		    else if (!strcmp(dirent->d_name, DUMPBASENAME)) {
			/* set multifiles (unsorted) flag */
			if (desc->filenames) SET(desc->status, STAT_MULTI_FILES);
			/* backup filename */
			SIMPLE_LIST_PUSH(desc->filenames, STRDUP(dirent->d_name));
		    }
		    /* if directory (without . and ..) */
		    else if (S_ISDIR(st.st_mode) && !IS_DOTF(dirent->d_name)) {
			/* backup path */
			PUSH_STR_SORTED(SIMPLE_LIST_PTR(desc->dirnames), 
							STRDUP(dirent->d_name));
			/* set flag SORTING to futur directory */
			ctree = ft_get(tree, dirent->d_name);
			SET(ctree->status, SORTING);
		    }
		}
		closedir(dir);
		/* remove all added filenames */
		*pbpath = *pcpath = 0;
	    }
	    UNSET(desc->status, STAT_NOTREAD);
	}
	
	/* look for all files to read */
	while (desc->filenames) {
	    char  *filename;
	    
	    /* get filename and build complet path */
	    filename = SIMPLE_LIST_POP(desc->filenames);
	    ADD_PATH(desc->bpath, pbpath, filename);
	    FREE(filename);
	    /* try to open and return */
	    if (!(desc->fd = fopen(desc->bpath, "r"))) PFERROR("fopen(%s)", desc->bpath);
	    else {
		/* remove filename from cpath */
		*pbpath = 0;
		return (desc->fd);
	    }
	    /* clean filename */
	    *pbpath = 0;
	}
	
	/* all files in this directory are read */
	UNSET(tree->status, SORTING);
	
	/* look for all directories marked */
	if (SIMPLE_LIST_PTR(desc->dirnames)) {
	    char  *filename;

	    filename = SIMPLE_LIST_POP(SIMPLE_LIST_PTR(desc->dirnames));
	    /* get new path and add it to current */
	    pcpath = ADD_PATH(desc->cpath, pcpath, filename);
	    pbpath = ADD_PATH(desc->bpath, pbpath, filename);
	    
	    /* push a new directory stack */
	    SIMPLE_LIST_PUSH(desc->dirnames, NULL);
	    /* seek tree with new path */
	    tree = ft_get(tree, filename);
	    
	    SET(desc->status, STAT_NOTREAD);
	    FREE(filename);
	    continue;
	}
	
	/* nothing to do return to parent */
	SIMPLE_LIST_POP(desc->dirnames);
	/* no more parent */
	if (!desc->dirnames) return (NULL);
	/* set to parent */
	tree = tree->parent;
	pcpath = REMOVE_DIR(desc->cpath, pcpath);
	*pcpath = 0;
	pbpath = REMOVE_DIR(desc->bpath, pbpath);
	*pbpath = 0;
	
    }
    return (desc->fd);
}

int   input_file_read(t_file_status *desc, t_ft *tree) {
    int   count = 0, cmax = COUNT_LEVEL1;
    
    /* force marking the base : ABNORMAL */
    SET(tree->status, CHANGED);
    /* file input loop */
    while (!cmax || count < cmax) {
	if (!input_file_set_next_file(desc, tree)) break;
	count += input_file_read_files(desc, tree, COUNT_LEVEL2);
    }
    return (count);
}

/* traduct a string options descriptions to flags
 */
int         input_file_fcntl(t_file_status *env, int cmd) {
    if (IS(cmd, IS_SORTED)) return (env ? IS(env->options, OPT_SORTED) : 1);
    return (0);
}


t_file_status  *input_file_open(char *pathname, char *opts) {
    t_file_status  *new;
    char           *filename;
    int            status;
    char           tmp[BUFFER_LENGTH];
    char           *ppath, *popts, *pvalue;
    int            options = (OPT_SORTED|OPT_READ);
    
    popts = opts ? strcpy(tmp, opts) : NULL;
    ppath = pathname ? strcpy(tmp + strlen(tmp) + 1, pathname) : NULL;

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
	else {
	    error("unknown option : %s", popts);
	    return (0);
	}
	popts = t;
    }
    
    new = MALLOC(sizeof(*new));
    bzero(new, sizeof(*new));
    filename = ppath;
    new->options = options;
    /* looking for separator before filename */
    if (ppath && (ppath = backslashed_strmchr(ppath, DELIM_LIST))) {
	status = *ppath;
	*ppath++ = 0;
    }
    
    if (!filename || !filename[0] || !strcmp(filename, "-")) { 
	/* special case for stdin */
	new->fd = stdin;
    }
    else {
	struct stat st;
	/* case for files, should stat them */
	if (stat(filename, &st)) {
	    PFERROR("stat(%s)", filename);
	    FREE(new);
	    return (NULL);
	}
	if (S_ISDIR(st.st_mode)) {
	    /* directory, directory treatment */
	    strcpy(new->bpath, define_cleaned_path(filename));
	    strcpy(new->cpath, "");
	    new->filenames = NULL;
	    new->dirnames = NULL;
	    SET(new->status, STAT_NOTREAD);
	    SIMPLE_LIST_PUSH(new->dirnames, NULL);
	}
	else {
	    /* can test and open */
	    if (!(new->fd = fopen(filename, "r"))) {
		PFERROR("open(%s)", filename);
		FREE(new);
		return (NULL);
	    }
	}
    }
    return (new);
}


t_file_status  *input_stdin_open(char *desc, char *opts) {
    if ((desc = backslashed_strmchr(desc, DELIM_LIST)))
	return (input_file_open(desc, opts));
    return (input_file_open("", opts));
}

t_file_status  *input_file_close(t_file_status *old) {
    if (old->fd && old->fd != stdin) fclose(old->fd); 
    while (old->dirnames) {
	while (SIMPLE_LIST_PTR(old->dirnames))
	    FREE(SIMPLE_LIST_POP(SIMPLE_LIST_PTR(old->dirnames)));
	SIMPLE_LIST_POP(old->dirnames);
    }
    while (old->filenames)
	FREE(SIMPLE_LIST_POP(old->dirnames));
    FREE(old);
    return (NULL);
}








