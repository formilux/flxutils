/* 
 * Signfs version 0.6, Copyright Benoit DOLEZ, 2002
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "flx.h"
#include "utils.h"

#include "md5.h"

/* function that complete desc from file system informations */
t_file_desc *complete_info_from_file(char *path, t_file_desc *desc, int flag) {
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
    return (desc);
}



/* function that compare 2 tree entry, complete checksum and link if needed */
int  files_are_the_same(t_file_desc *f1, t_file_desc *f2, int Diff, char *path) {
    int diff = 0;
    
    // if (!IS(f1->status, STATUS_FILLED) || !IS(f2->status, STATUS_FILLED))
    //	return (diff = DIFF_NOTFILLED, 0);
    
    if (DIFF(TYPE)  && (f1->stat.st_mode & S_IFMT) != (f2->stat.st_mode & S_IFMT))
	diff |= DIFF_TYPE; /* types differ */
    if (DIFF(MODE)  && (f1->stat.st_mode & 07777) != (f2->stat.st_mode & 07777))
	diff |= DIFF_MODE; /* modes differ */
    if (DIFF(OWNER) && (f1->stat.st_uid != f2->stat.st_uid || 
			f1->stat.st_gid != f2->stat.st_gid))
	diff |= DIFF_OWNER; /* uid or gid differ */
    if (DIFF(SIZE) && S_ISREG(f1->stat.st_mode) && S_ISREG(f2->stat.st_mode) &&
	f1->stat.st_size != f2->stat.st_size)
	diff |= DIFF_SIZE; /* sizes differ for regular files */ 
    if (DIFF(DEV) && ((S_ISBLK(f1->stat.st_mode) && S_ISBLK(f2->stat.st_mode)) ||
		      (S_ISCHR(f1->stat.st_mode) && S_ISCHR(f2->stat.st_mode))) &&
	f1->stat.st_rdev != f2->stat.st_rdev)
	diff |= DIFF_DEV; /* minor/major differ for device files */
    if (DIFF(TIME) && f1->stat.st_mtime != f2->stat.st_mtime) {
	if (DIFF(DIR) || !S_ISDIR(f1->stat.st_mode) || !S_ISDIR(f2->stat.st_mode))
	    if (DIFF(LDATE) || !(S_ISLNK(f1->stat.st_mode)))
	        diff |= DIFF_TIME; /* modification times diff */
    }
    if (DIFF(LINK) && S_ISLNK(f1->stat.st_mode) && S_ISLNK(f2->stat.st_mode)) {
	char temp[BUFFER_LENGTH];
	int  l;
	
	if (f1->link != f2->link) {
	    if (!f1->link || !f2->link) {
		if (!path) diff |= DIFF_LINK;
		else {
		    /* rebuild link and link's checksum */
		    if ((l = readlink(path, temp, BUFFER_LENGTH)) < 0) {
			PFERROR("readlink(%s)",path);
		    } else {
			temp[l] = 0;
			if (!f1->link) f1->link = strdup(temp);
			if (!f2->link) f2->link = strdup(temp);
			if (!f1->md5) f1->md5 = checksum_md5_from_data(temp, l);
			if (!f2->md5) f2->md5 = checksum_md5_from_data(temp, l);
		    }
		}
	    }
	    if (!(diff & DIFF_LINK) && strcmp(f1->link, f2->link)) 
		diff |= DIFF_LINK; /* links differ */
	}
    }
    if (DIFF(CHECKSUM) && S_ISREG(f1->stat.st_mode) && S_ISREG(f2->stat.st_mode)) {
	if (f1->md5 || f2->md5) {
	    if (!f1->md5 || !f2->md5) diff |= DIFF_CHECKSUM;
	    else if (memcmp(f1->md5, f2->md5, 16))
		diff |= DIFF_CHECKSUM; /* checksums differ */
	}
    }
    return (diff);
}

/* return the path describe by the structure */
char *build_path(t_ft *tree) {
    static char path[BUFFER_LENGTH];

    if (!tree || !tree->filename)
	path[0] = 0;
    else if (tree->parent && 
	     (IS_PDOTF(tree) || !tree->parent->filename))
	strcpy(path, tree->filename);
    else {
	build_path(tree->parent);
	strcat(path, "/");
	strcat(path, tree->filename);
    } 
    return (path);
}


/* free memory for an existant tree 
 * tree : tree to clean
 */
void free_tree(t_tree *tree) {
    t_tree *current, *todel;
    
    if (tree->link) FREE(tree->link);
    if (tree->checksum) FREE(tree->checksum);
    if (tree->filename) FREE(tree->filename);
    if ((current = tree->subtree)) {
	while (current != tree) {
	    todel = current;
	    current = current->next;
	    todel->prev->next = todel->next;
	    todel->next->prev = todel->prev;
	    free_tree(todel);
	}
    }
    if (tree->subtree) FREE(tree->subtree);
    FREE(tree);
}

/* browse only directories */
/* path : directory to treat
 * fct  : fonction that treat file and return a (new) pointer to data to recursive act
 * data : pointer for function fct
 */
int browse_over_path(char *path, PROTO_FS(*fct), void *data) {
    struct stat    stat;
    DIR            *dir;
    struct dirent  *dirent;
    off_t          current_dir;
    char           *new_filename;
    int            l;

    if (!path) return (0);

    /* initialise new_filename */
    new_filename = MALLOC((l = strlen(path)) + 1 + FILENAME_LENGTH);
    memcpy(new_filename, path, l);
    if (new_filename[l-1] != '/') strcpy(new_filename + l++, "/");

    if ((dir = opendir(path))) {
	/* each file of the directory */
	while ((dirent = readdir(dir))) {
	    /* build new filename */
	    strcpy(new_filename + l, dirent->d_name);
	    
	    /* get file informations */
	    if (lstat(new_filename, &stat)) {
		PFERROR("stat(%s)", new_filename);
		continue;
	    }

	    /* directory selection */
	    if (S_ISDIR(stat.st_mode)) {
		void *new_data;
		/* save current directory position */
		current_dir = telldir(dir);
		closedir(dir);
		
		/* fonction on file status */
		if ((new_data = fct(new_filename, new_filename+l, &stat, data))) {

		    browse_over_path(new_filename, fct, new_data);

		    /* if new pointer return, differs with old one, we should
		     * remove it before use (same, with filename NULL) */
		    if (new_data != data) fct(NULL, NULL, NULL, new_data);
		}
		/* restore directory position */
		dir = opendir(path);
		seekdir(dir, current_dir);
	    }
	    else {
		/* fonction on file status */
		fct(new_filename, new_filename+l, &stat, data);
	    }
	}
	closedir(dir);
    }
    else {
	PFERROR("opendir(%s)", path);
	return (0);
    }
    FREE(new_filename);
    return (1);
}


/* build an MD5 checksum from data in file */
char *checksum_md5_from_file(char *file) {
    int       fd;
    ssize_t   size;
    char      *checksum_md5 = NULL, blk[BUFFER_LENGTH];
    MD5_CTX   md5_ctx;
    
    if ((fd = open(file, O_RDONLY)) < 0 ) {
	PFERROR("open(%s) for MD5 checksum", file);
    } 
    else {
	MD5Init(&md5_ctx);
	while ((size = read(fd, blk, BUFFER_LENGTH)) > 0)
	    MD5Update(&md5_ctx, blk, size);
	close(fd);
        // if size = -1, there is a read error, don't do anything
        if (size == 0) { // last read is null
	    checksum_md5 = MALLOC(16);
	    MD5Final(checksum_md5, &md5_ctx);
        }
    }
    return (checksum_md5);
}

/* build an MD5 checksum from a string */
char *checksum_md5_from_data(char *data, int len) {
    char      *checksum_md5 = 0;
    MD5_CTX   md5_ctx;
    
    MD5Init(&md5_ctx);
    MD5Update(&md5_ctx, data, len);
    checksum_md5 = MALLOC(16);
    MD5Final(checksum_md5, &md5_ctx);
    return (checksum_md5);
}


/* remove ended '/' */
char *remove_ended_slash(char *str) {
    static char temp[BUFFER_LENGTH];
    int         l = strlen(str);
    
    strcpy(temp, str);
    while (temp[l - 1] == '/') temp[--l] = 0;
    return (temp);
}

/* return pointer to the end of the current field */
char *end_field(char *line) {
    while (*line && *line != ' ' && *line != '\t' && *line != '\n') {
	if (*line++ == '\\' && (*line == '\\' || 
				*line == ' ' ||
				*line == '\t'||
				*line == '\n'))
	    line++;
    }
    if (*line) *line++ = 0;
    return (*line ? line : 0);
}


/* create directory and parent directory if needed */
int mkdir_with_parent(char *pathname, mode_t mode) {
    struct stat st;
    char        tmp[BUFFER_LENGTH], *ptmp = tmp;
    
    if (*pathname == '/') *ptmp++ = *pathname++;
    while (pathname && *pathname) {
	while (*pathname && *pathname != '/') *ptmp++ = *pathname++;
	while (*pathname == '/') pathname++;
	*ptmp = 0;
	if (stat(tmp, &st)) {
	    /* file cannot be stat, try to make directory */
	    if (mkdir(tmp, mode)) {
		PFERROR("mkdir(%s)", tmp);
		return (0);
	    }
	}
	else if (!S_ISDIR(st.st_mode)) {
	    error("%s exist and isn't a directory", tmp);
	    return (0);
	}
	*ptmp++ = '/';
    }
    return (1);
}

/* return formatted info into a static string */
char *build_line(char *path, char *filename, t_file_desc *info) {
    struct stat *st = &(info->stat);
    static char blk[BUFFER_LENGTH], tmp[64];
    int         s;

    /* show informations */
    blk[s = 0] = 0;

    /* bad data */
    if (!path) { 
	sprintf(blk,"## anormal action : path=%s desc=%p", path, st); 
	return (blk); 
    }
    
    if (st) {
	/* display file type */
	s += sprintf(blk+s, "%c", FILE_TYPE(st->st_mode));
    
	/* display file mode */
	if (IS(Options, GOPT_HUMAN_READABLE))
	    s += sprintf(blk+s," %04o(%s) ", st->st_mode & 0007777, 
			 right(st->st_mode));
	else
	    s += sprintf(blk+s," %04o ", st->st_mode & 0007777);
	
	/* display user and group id */
	s += sprintf(blk+s,"%5d %5d ", st->st_uid, st->st_gid);
	
	/* display size of device info */
	if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode)) {
	    sprintf(tmp, "%d,%d", major(st->st_rdev), minor(st->st_rdev));
	    s += sprintf(blk+s, "%10s ", tmp);
	} else {
	    s += sprintf(blk+s, "%10ld ", S_ISDIR(st->st_mode) ? 0 : st->st_size );
	}
	
	/* display checksum */
	if (info->md5) {
	    int i;
	    for (i=0; i < 16; i++) s += sprintf(blk+s, "%02x", info->md5[i]);
	    s += sprintf(blk+s, " ");
	}
	else
	    s += sprintf(blk+s, "-------------------------------- ");
	
	/* display date */
	if (IS(Options, GOPT_HUMAN_READABLE)) {
	    strcpy(tmp, ctime(&(st->st_mtime))); tmp[strlen(tmp) - 1] = 0;
	    s += sprintf(blk+s, "%10ld(%s) ", st->st_mtime, tmp);
	}
	else 
	    s += sprintf(blk+s, "%10ld ", st->st_mtime);
	
	s += sprintf(blk+s, "%s", escape_str(path));
	
	if (S_ISLNK(st->st_mode) && info->link)
	    s += sprintf(blk+s, " %s", escape_str(info->link));
    }
    else {
	if (IS(Options, GOPT_HUMAN_READABLE))
	    s += sprintf(blk+s, "? 0000(-) % 5d % 5d % 10d -------------------------------- %10d(-) %s",0 , 0, 0, 0, 
			 escape_str(path));
	else 
	    s += sprintf(blk+s, "? 0000 % 5d % 5d % 10d -------------------------------- %10d %s",0 , 0, 0, 0, 
			  escape_str(path));
    }
    return (blk);
}

/* return formatted info into a static string */
char *show_filename(char *path, char *filename, t_file_desc *info) {
    return (path);
}


/* view files with recursive method */
void dump_tree_(t_ft *tree, int force) {
    static char   path[BUFFER_LENGTH];
    char          *ppath;

    ppath = path + strlen(path);
    while (1) {
	if (tree->filename) {
	    ADD_PATH(path, ppath, tree->filename);
	    printf("[%02x] %s\n", tree->status, path);
	    // UNSET(tree->status, CHANGED);
	}
	if (tree->subtree && (!IS_PDOTF(tree) || force)) {
	    dump_tree_(tree->subtree, 0);
	}
	tree = tree->next;
	if (IS(tree->status, BASE)) break;
    }
    *ppath = 0;
}

void dump_tree(t_ft *tree) {
    printf("** etat de l'arbre **\n");
    dump_tree_(tree, 1);
    printf("** end **\n");
}


/* find last good defined path (without ended '/') */
char *define_base_path(char *real) {
    static char tmp[BUFFER_LENGTH];
    char        *last, *breal, *lslash;

    lslash = last = tmp;
    while (*real) {
	/* pass multiple '/' */
	while (*real == '/') real++;
	if (!*real) break; /* end of pathname */
	breal = real;
	/* get directory name */
	while (*real && *real != '/') *last++ = *real++;
	/* last have no ended '/' */
	lslash = last;
	/* find . or .., restart */
	if (IS_DOTF(breal) || IS_DOTD(breal)) lslash = last = tmp;
	else                                  *last++ = *real;
    }
    /* remove last slash */
    *lslash = 0;
    return (tmp);
}

/* remove multiple '/' (and ended '/')  */
char *define_cleaned_path(char *real) {
    static char tmp[BUFFER_LENGTH];
    char        *ptmp = tmp;

    while (*real) {
	while ((*ptmp = *real)) { 
	    ptmp++; 
	    if (*real++ == '/') break; 
	}
	while (*real == '/') real++;
    }
    *ptmp = *real;
    /* do not remove ending '/' */
#if 1
    if (ptmp-1 > tmp && *(ptmp-1) == '/') *(ptmp-1) = 0; 
#endif
    return (tmp);
}



