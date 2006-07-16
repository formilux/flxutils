#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include "input_fs.h"
#include "flx.h"
#include "utils.h"

#define STAT_NOTREAD          0x01
#define STAT_TOBECONTINUED    0x02
#define STAT_DOINIT           0x04


#define OPT_XDEV             0x01
#define OPT_FOLLOW_LINKS     0x02
#define OPT_SORTED           0x04
#define OPT_READ             0x08

static t_file_desc *complete_info_from_fs(char *path, t_file_desc *desc) {
    struct stat    stat;

    /* try to stat it */
    if (lstat(path, &stat)) {
	PFERROR("stat(%s)", path);
	return (0);
    }
    if (!desc) { 
	/* have to create it */
	desc = MALLOC(sizeof(*desc));
	bzero(desc, sizeof(*desc));
    }
    /* copy stat informations */
    memcpy(&desc->stat, &stat, sizeof(stat));

    if (IS(Diff, DIFF_CHECKSUM)) {
        if (S_ISREG(stat.st_mode) && !desc->md5)     /* get md5 checksum */
            desc->md5 = checksum_md5_from_file(path);
        else if (S_ISLNK(stat.st_mode) && !desc->link) { 
	    /* get link and md5 associed */
            char temp[BUFFER_LENGTH];
            int  l;

            if ((l = readlink(path, temp, BUFFER_LENGTH)) < 0) {
                PFERROR("readlink(%s)", path);
            } else {
                temp[l] = 0;
                desc->link = strdup(temp);
                desc->md5 = checksum_md5_from_data(temp, l);
            }
        }
    } else
        desc->md5 = NULL;

    return (desc);
}

int desc_s_isdir_from_fs(char *filename, t_file_desc *desc) {
    if (!desc) {
	struct stat st;
	if (!filename) return (0);

	if (stat(filename, &st)) {
	    PFERROR("stat(%s)", filename);
	    return (0);
	}
	return (S_ISDIR(st.st_mode));
    }
    return (S_ISDIR(desc->stat.st_mode));
}

#define DESC_S_ISDIR(desc) ((desc)?(S_ISDIR(((t_file_desc*)(desc))->stat.st_mode)):0)
#define DESC_DEV(desc)     ((desc)?(((t_file_desc*)(desc))->stat.st_dev):0)


/* get file entries from filesystem
 * ics : input current status 
 * tree : tree to load
 * return the number of entries readed
 */
int  input_fs_read(t_fs_status *desc, t_ft *tree) {
    char *pcpath, *pbpath;
    int  count = 0, cmax = COUNT_LEVEL2;

    /* something to read or no more dir to read */
    if (!desc->dirnames) return (0);

    /* force to mark base ABNORMAL! */
    SET(tree->status, CHANGED);
    /* restore/build tree position for cpath */
    tree = ft_get(tree, desc->cpath);

    /* set base path pointers */
    pcpath = desc->cpath + strlen(desc->cpath);
    pbpath = desc->bpath + strlen(desc->bpath);

    /* read infos for wanted directory */
    if (IS(desc->status, STAT_DOINIT)) {

	if (!(tree->desc = complete_info_from_fs(desc->bpath, tree->desc)))
	    return (0);
	SET_PARENT(tree, CHANGED);
	SET(tree->status, FILLED);
	count++;
	
	/* remove init flag */
	UNSET(desc->status, STAT_DOINIT);

	/* set base device */
	desc->dev = DESC_DEV(tree->desc);

	/* this part is used to detect directory and directly */
	if (IS(Options, GOPT_IGNORE_DEPTH) ||
            !DESC_S_ISDIR(tree->desc) || desc->depth == 1) {
	    SIMPLE_LIST_POP(desc->dirnames);
	    return (count);
	}

        /* sub directory recursion */
        desc->depth--;
    }
    
    while (1) {

	/* get directory entries if never done */
	if (IS(desc->status, STAT_NOTREAD)) {
	    DIR            *dir;
	    struct dirent  *dirent = NULL;
	    t_ft           *new;
	    
	    /* mark directory as unsorted */
	    SET(tree->status, SORTING);
	    
	    if (!(dir = opendir(desc->bpath)))
		PFERROR("opendir(%s)", desc->bpath);
	    else {
		/* restore old stored position */
		if (IS(desc->status, STAT_TOBECONTINUED)) { 
		    seekdir(dir, desc->off_dir); 
		}

		/* look for each files */
		while ((!cmax || count < cmax)) {
		    
		    if (!(dirent = readdir(dir))) break;
		    
		    /* options tell to ignore '.' and '..' */
		    if (IS(Options, GOPT_IGNORE_DOT) && IS_DOTF(dirent->d_name)) 
			continue;

		    /* build path to work with it */
		    // ADD_PATH(desc->cpath, pcpath, dirent->d_name);
		    ADD_PATH(desc->bpath, pbpath, dirent->d_name);

		    /* create/get entry */
		    new = ft_get(tree, dirent->d_name);

		    /* get file informations */
		    new->desc = complete_info_from_fs(desc->bpath, new->desc);
		    SET_PARENT(new, CHANGED);
		    SET(new->status, FILLED);
		    
		    /* if directory (without . and ..) and not XDEV */
                    /* if not maximum directory level reached */
		    if (desc->depth != 1 && 
                        DESC_S_ISDIR(new->desc) && !IS_DOTF(dirent->d_name) &&
			(!IS(desc->options, OPT_XDEV) || desc->dev == DESC_DEV(new->desc))) {
			
			PUSH_STR_SORTED(SIMPLE_LIST_PTR(desc->dirnames), 
					STRDUP(dirent->d_name));
			/* set flag SORTING to futur directory */
			SET(new->status, SORTING);
		    }
		    count++;
		}
		*pcpath = *pbpath = 0;
		
		/* have finished to read ?? */
		if (dirent) { 
		    /* no break and return number */
		    desc->off_dir = telldir(dir);
		    closedir(dir);
		    SET(desc->status, STAT_TOBECONTINUED);
		    return (count);
		}
		closedir(dir);
	    }
	    UNSET(desc->status, STAT_NOTREAD|STAT_TOBECONTINUED);
	}
	
	/* all files in this directory are read */
	UNSET(tree->status, SORTING);
	
	/* look for all directories marked */
	if (SIMPLE_LIST_PTR(desc->dirnames)) {
	    char  *filename;

	    filename = SIMPLE_LIST_POP(SIMPLE_LIST_PTR(desc->dirnames));
	    /* get new path and add it to current */
	    pcpath = ADD_PATH(desc->cpath, pcpath, filename);
	    /* set new complet path */
	    pbpath = ADD_PATH(desc->bpath, pbpath, filename);

            /* decrease directory depth */
            if (desc->depth) desc->depth--;
	    
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

        /* increase directory depth */
        if (desc->depth) desc->depth++;

	/* no more parent */
	if (!desc->dirnames) return (count);
	/* set to parent */
	tree = tree->parent;
	pcpath = REMOVE_DIR(desc->cpath, pcpath);
	*pcpath = 0;
	pbpath = REMOVE_DIR(desc->bpath, pbpath);
	*pbpath = 0;
    }
    return (count);
}


/* traduct a string options descriptions to flags
 */
int         input_fs_fcntl(t_fs_status *env, int cmd) {
    if (IS(cmd, IS_SORTED)) return (env ? IS(env->options, OPT_SORTED) : 1);
    return (0);
}

/* initialise or add data to current input driver 
 */
t_fs_status *input_fs_open(char *pathname, char *opts) {
    t_fs_status  *new;
    char         tmp[BUFFER_LENGTH];
    char         *ppath, *popts, *pvalue, *filename;
    int          status = 0;
    int          options = (OPT_SORTED|OPT_XDEV|OPT_READ);
    int          depth = 0;
    
    popts = opts ? strcpy(tmp, opts) : NULL;
    ppath = pathname ? strcpy(tmp + strlen(tmp) + 1, pathname) : NULL;
    
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
	else if (!strcmp(popts, "xdev"))      /* do not recurse across fs */
	    options = (sign > 0 ? (options|OPT_XDEV) : (options&~OPT_XDEV));
	else if (!strcmp(popts, "depth"))      /* recursive level across directory */
	    depth = atoi(pvalue);
	else if (!strcmp(popts, "flw"))  /* follow links */
	    options = (sign > 0 ? (options|OPT_FOLLOW_LINKS) : (options&~OPT_FOLLOW_LINKS));
	else {
	    error("unknown option : %s", popts);
	    return (0);
	}
	popts = t;
    }
    
    new = MALLOC(sizeof(*new));
    bzero(new, sizeof(*new));
    
    new->dirnames = NULL;
    SIMPLE_LIST_PUSH(new->dirnames, NULL);
    new->status = STAT_NOTREAD | STAT_DOINIT;

    /* save special options */
    new->options = options;
    new->depth = depth;

    filename = ppath;
    /* looking for separator before filename */
    if (ppath && (ppath = backslashed_strmchr(ppath, DELIM_LIST))) {
	status = *ppath;
	*ppath++ = 0;
    }

    strcpy(new->bpath, define_cleaned_path(filename));
    if (!*new->bpath) strcpy(new->bpath, ".");

    /* have to remove sub parameters */
    // HERE
	
    if (status == '=' && ppath)
	strcpy(new->cpath, define_cleaned_path(ppath));
    else
	strcpy(new->cpath, define_base_path(filename));
    return (new);
}

/* free all used memory
 */
t_fs_status *input_fs_close(t_fs_status *old) {
    while (old->dirnames) {
	while (SIMPLE_LIST_PTR(old->dirnames))
	    FREE(SIMPLE_LIST_POP(SIMPLE_LIST_PTR(old->dirnames)));
	SIMPLE_LIST_POP(old->dirnames);
    }
    FREE(old);
    return (NULL);
}









