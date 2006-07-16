#ifndef __FLX_H__
#define __FLX_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "utils.h"
#include "md5.h"

#define COPYRIGHT "Copyright 2002, Benoit DOLEZ <bdolez@ant-computing.com>"

#define DUMPBASENAME     "formilux-sig.dat"

#define MD5_Init(ctx)                 ToolsMD5Init(ctx)
#define MD5_Update(ctx, data, len)    ToolsMD5Update(ctx, data, len)
#define MD5_Final(data, ctx)          ToolsMD5Final(data, ctx)

#define O4(a) (htonl(*(int*)(a)))

#define MATCH(f,m)  (!strcmp(f, m))

#define STATUS_REQUEST_DIR 0x0001
#define STATUS_FILLED      0x0002

#define SINGLE_DEV         0x0001
#define HAVE_TO_CHECK      0x0002
#define SHOW_SAME          0x0004
#define SHOW_NEW           0x0008
#define SHOW_OLD           0x0010
#define SHOW_ALL           (SHOW_SAME|SHOW_NEW|SHOW_OLD)
#define GOPT_HUMAN_READABLE     0x0020
#define GOPT_IGNORE_DOT         0x0040
#define GOPT_IGNORE_DEPTH       0x0080

#define FILE_TYPE(a) (S_ISLNK(a) ?'l':S_ISDIR(a) ?'d': \
                      S_ISCHR(a) ?'c':S_ISBLK(a) ?'b': \
                      S_ISFIFO(a)?'f':S_ISSOCK(a)?'s': \
                      S_ISREG(a) ?'-':'?')


#define F_TYPE          0x0001
#define F_MODE          0x0002
#define F_OWNER         0x0004
#define F_SIZE          0x0008
#define F_DEV           0x0010
#define F_CHECKSUM      0x0020
#define F_LINK          0x0040
#define F_TIME          0x0080
#define F_LDATE         0x0100

#define DIFF_TYPE       F_TYPE
#define DIFF_MODE       F_MODE
#define DIFF_OWNER      F_OWNER
#define DIFF_SIZE       F_SIZE
#define DIFF_DEV        F_DEV
#define DIFF_CHECKSUM   F_CHECKSUM
#define DIFF_LINK       F_LINK
#define DIFF_TIME       F_TIME
#define DIFF_LDATE      F_LDATE
#define DIFF_NOTFILLED  0x8000

#define DELIM_BASE    ':'
#define DELIM_CHANGE  '='
#define DELIM_NEXT    ','
#define DELIM_LIST    (char[]){ DELIM_BASE, DELIM_CHANGE, DELIM_NEXT, 0 }

#define DIFF(a) (IS(Diff, DIFF_##a))
// #define DIFF(a)       (Diif)



#define IS_DOTDOT(str)  (*(str) == '.' && *((str)+1) == '.' && *((str)+2) == 0)
#define IS_DOT(str)     (*(str) == '.' && *((str)+1) == 0)
#define IS_DOTF(str)    (*(str) == '.' && \
		         (*((str)+1) == 0 || \
			  (*((str)+1) == '.' && *((str)+2) == 0)))

#define IS_DOTD(str)    (*(str) == '.' && \
		         (*((str)+1) == '/' || \
			  (*((str)+1) == '.' && *((str)+2) == '/')))



#define IS_PDOT(ptr)     ((ptr)->parent && \
			  (ptr)->subtree == (ptr)->parent->subtree)
#define IS_PDOTDOT(ptr)  ((ptr)->parent && (ptr)->parent->parent && \
			  (ptr)->subtree == (ptr)->parent->parent->subtree)
#define IS_PDOTF(ptr)    (IS_PDOT(ptr) || IS_PDOTDOT(ptr))


#define ADD_PATH_DEFINE(pbegin, pend, filename) ({ \
     char *__ret; \
     if (filename) { \
       *pend = 0; \
       if (pend > pbegin && *(pend-1) != '/') strcpy(pend, "/") ; \
       strncat((pend), (filename), BUFFER_LENGTH - ((pend) - (pbegin)) - 2); \
       __ret = (pend) + strlen(pend); \
     } else __ret = (pend) ; \
     __ret; \
})

#define ADD_PATH(pbegin, pend, filename)  add_path(pbegin, pend, filename)

static inline char *add_path(char *pbegin, char *pend, char *filename) {
    return (ADD_PATH_DEFINE(pbegin, pend, filename));
}

#define REMOVE_DIR_DEFINE(pbegin, pend) ({ \
    char *__ret; \
    if (!(pend = strrchr(pbegin, '/'))) __ret = (pbegin); \
    else \
       if (pend == pbegin) __ret = (pend+1); \
       else __ret = (pend); \
    __ret; \
})

#define REMOVE_DIR(pbegin, pend)   remove_dir(pbegin, pend)

static inline char *remove_dir(char *pbegin, char *pend) {
    return (REMOVE_DIR_DEFINE(pbegin, pend));
}

#define COUNT_LEVEL1     100        /* number with all directory */
#define COUNT_LEVEL2     100        /* maximum in a directory */
#define NEWDIR(parent)   ft_add(parent, NULL, NULL, NULL);

/* structure definition for file tree */
typedef struct s_file_tree t_file_tree ;
typedef t_file_tree        t_ft;


struct s_file_tree {
    t_file_tree  *prev, *next; /* others file in same directory */
    t_file_tree  *parent;      /* parent directory */
    t_file_tree  *subtree;     /* pointer to sub directory */
    char         *filename;    /* filename (without path) */
    void         *desc;        /* pointer from specific data */



#define FILLED      0x01
#define CHANGED     0x02
#define BASE        0x04
#define WRITTEN     0x08
#define SORTING     0x10
#define SORTED      0x20
#define READING     0x40
    int          status;       /* status for file */
};

#define SET_MARK_CHANGED(ptr) set_parent(ptr, CHANGED)
#define SET_PARENT(ptr, flag) set_parent(ptr, flag) 

static inline void set_parent(t_file_tree *cur, int flag) {
    while (cur) {
	if (IS(cur->status, flag)) break;
	SET(cur->status, flag);
	if (cur->parent == cur) break;
	cur = cur->parent;
    }
}



/* structure de définition des informations */

typedef struct s_file_desc t_file_desc ;

struct s_file_desc {
    struct stat    stat;
    unsigned char  *md5;
    unsigned char  *link;
};

#define FCT_DOIT3_PROTO(name) int (*name)(void *data, int order, \
					    t_file_tree *src1, \
					    t_file_tree *src2, \
					    t_file_tree *dst)


typedef struct s_tree t_tree;

struct s_tree {
    t_tree       *prev;
    t_tree       *next;
    char         *filename;
    t_tree       *subtree;
    t_tree       *parent;
    u_int        status;
    struct stat  stat;
    u_char       *checksum;
    char         *link;
};

typedef struct s_fctdata t_fctdata;

struct s_fctdata {
    void *(*fct)();
    void *data;
};


typedef struct s_dtree t_dtree;

struct s_dtree {
    t_tree *tree1;
    t_tree *tree2;
};

#define PROTO_FS(name)    void *(name)(char *path, char *filename, struct stat *stat, void *data)
#define PROTO_FILE(name)  void *(name)(char *line, void *data)
#define PROTO_SAVE(name)  void *(name)(char *path, t_tree *new, void *data)


extern int browse_over_path(char *path, void *(*fct)(), void *data) ;
extern char *checksum_md5_from_file(char *file) ;
extern char *checksum_md5_from_data(char *data, int len) ;
extern char *end_field(char *line) ;
extern int mkdir_with_parent(char *pathname, mode_t mode) ;
extern char *build_line(char *path, char *filename, t_file_desc *info) ;
extern void dump_tree(t_ft *tree) ;
extern int dump_diff_tree(t_dtree *tree);
extern int files_are_the_same(t_file_desc *f1, t_file_desc *f2, int Diff, char *path) ;
extern char *build_path(t_ft *tree);
extern t_tree *get_line(char *line) ;
extern void free_tree(t_tree *tree);
t_tree *get_from_signfs_line(char *line, char **rpath) ;
char *remove_ended_slash(char *str) ;

extern int browse_over_tree_from_path(char *path, void *(*add_fct)(), void *(*srch_fct)(), void *data) ;
extern t_tree *build_new_tree_from_fs(char *path, char *filename, struct stat *stat, void *data) ;
extern t_tree *build_check_tree_from_fs(char *path, char *filename, struct stat *stat, void *data) ;
extern void *save_tree(char *path, t_tree *new, t_tree *tree) ;
extern void *check_before_saving_tree(char *path, t_tree *new, t_dtree *dtree) ;
extern void *build_new_tree_from_mfile(char *path, char *filename, struct stat *stat, void *tree) ;
extern void build_new_tree_from_file(char *path, void *(*fct)(), void *tree) ;
extern void *build_check_tree_from_mfile(char *path, char *filename, struct stat *stat, void *tree) ;

extern t_ft *get_tree(char *path, t_ft *tree) ;
extern t_ft *get_path(char *path, t_ft *tree) ;
extern t_ft *get_parent_tree(char *path, t_ft *data) ;

int   signfs_read_file(char *path, t_tree *tree);
int   signfs_read_fs(char *path, t_tree *tree);
int   signfs_dump_tree(t_tree *tree, char *output, int level);


extern t_ft *ft_add(t_ft *base, char *pfilename, void *(*fct_init_desc)(), void *data);
extern t_ft *ft_del(t_ft *old, void (*fct_free_desc)(void *data, void *desc), void *data);
extern t_ft *ft_get(t_ft *tree, char *path);

extern char *define_base_path(char *path);
extern char *define_cleaned_path(char *path);


/* device for xdev option */
extern dev_t SingleDev;
extern char  *DumpFilename;
extern int   RecursionLevel;
extern int   Diff;
extern int   Options, Sorted;

POOL_INIT_PROTO(t_file_tree);

#endif /* __FLX_H__ */
