/* 
 * Signfs version 0.6, Copyright Benoit DOLEZ, 2002
 */

/* command line is:
 * ./flx check [options] input1 input2
 * ./flx check [options] [inputs1] , [inputs2]
 * options are :
 *   -h,--help             : this help
 *   --ignore-mode         : ignore file mode
 *   --ignore-owner        : ignore owner (user, group)
 *   --ignore-dev          : ignore device type
 *   --ignore-size         : ignore file size
 *   --ignore-sum          : ignore checksum (md5)
 *   --ignore-date         : ignore date
 *   --ignore-link         : ignore link
 *   --ignore-ldate        : ignore links date
 *   --show-new            : show only new files
 *   --show-old            : show only removed files
 *   --show-all            : show all files (sames and differs)
 */


#include <string.h>
#include <stdio.h>

#include "flx.h"
#include "check.h"
#include "input.h"
#include "output.h"
#include "output_file.h"
#include "input_file.h"
#include "input_fs.h"

static POOL_INIT(t_file_diff);

#define O_HELP              1
#define O_SH_HR             5
#define O_SH_ALL            6
#define O_SH_NEW            7
#define O_SH_OLD            8
#define O_IGN_MODE          9
#define O_IGN_OWNER         10
#define O_IGN_SUM           11
#define O_IGN_SIZE          12
#define O_IGN_DATE          13
#define O_IGN_LINK          14
#define O_IGN_LDATE         15
#define O_REWRITE_SRC1      16
#define O_REWRITE_SRC2      17
#define O_OUTPUT            18
#define O_IGN_DOT           19

t_param flxcheck_poptions[] = {
    { 'h', "help", O_HELP, 0, 
      "-h,--help             show this help"},
    { 0, "ignore-mode", O_IGN_MODE, 0, 
      "--ignore-mode         ignore mode" },
    { 0, "ignore-owner", O_IGN_OWNER, 0, 
      "--ignore-owner        ignore uid/gid" },
    { 0, "ignore-sum", O_IGN_SUM, 0, 
      "--ignore-sum          ignore checksum" },
    { 0, "ignore-size", O_IGN_SIZE, 0, 
      "--ignore-size         ignore size" },
    { 'd', "ignore-date", O_IGN_DATE, 0, 
      "--ignore-date         ignore date" },
    { 0, "ignore-link", O_IGN_LINK, 0, 
      "-d,--ignore-link      ignore link" },
    { 0, "ignore-ldate", O_IGN_LDATE, 0, 
      "--ignore-ldate        ignore date on links" },
    { 0, "ignore-dot", O_IGN_DOT, 0,
      "--ignore-dot          do not compare '.' and '..'" },
    { 0, "show-all", O_SH_ALL, 0,
      "--show-all            show all files (same and differs)" },
    { 0, "only-new", O_SH_NEW, 0,
      "--only-new            show only new files"},
    { 0, "only-old", O_SH_OLD, 0,
      "--only-old            show only old files"},
    { 0, "human-readable", O_SH_HR, 0,
      "--human-readable      show long format"},
    { 0, "rw1", O_REWRITE_SRC1, 1, 
      "--rw1 <string>        string before first source path"},
    { 0, "rw2", O_REWRITE_SRC2, 1, 
      "--rw2 <string>        string before second source path"},
    { 0, "op", O_OUTPUT, 1, 
      "--op <string>         all output prefix"},
    { 0,   NULL,   0 }
} ;

static int   output_diff_fcntl(t_file_status *env, int cmd);
static int   output_diff_write(t_file_status *spec, t_ft *tree, int number);


static t_source_type InputTypes[] = { 
    { "fs",   
      (void*)input_fs_open,    (void*)input_fs_read,   NULL, (void*)input_fs_close,  
      (void*)input_fs_fcntl },
    { "file", 
      (void*)input_file_open,  (void*)input_file_read, NULL, (void*)input_file_close,
      (void*)input_file_fcntl },
    { "stdin",
      (void*)input_stdin_open, (void*)input_file_read, NULL, (void*)input_file_close,
      (void*)input_file_fcntl },
    { NULL }
};

static t_source_type OutputTypes[] = { 
    { "stdout",
      (void*)output_stdout_open, NULL, (void*)output_diff_write, (void*)output_file_close,
      (void*)output_diff_fcntl},
    { NULL } 
};



static void *InputSrc1, *InputSrc2, *Output;
static char *Rewrite_Src1 = NULL, *Rewrite_Src2 = NULL, *Output_Str = NULL;


/* cette fonction permet de repérer toutes les données communes à deux sources et de 
 * les inserer dans un autre arbre
 * cette fonction retourne le nombre d'éléments sortis des arbres sources
 */
int ft_find_diff(t_ft *src1, t_ft *src2, t_ft *dst) {
    int         count = 0, ret, diff;
    t_file_diff *desc;
    t_ft        *next1, *next2;
    
    /* on démarre du fils de chaque arbre en créant au besoin les
       éléments manquant dans l'arbre destination */
    
    if (!src1 || !src2 || !src1->subtree || !src2->subtree) return (0);
    
    if (dst && !dst->subtree) {
	dst->subtree = NEWDIR(dst);
    }

    src1 = src1->subtree->next; 
    src2 = src2->subtree->next;
    dst = dst->subtree->next;
    
    while (!IS(src1->status, BASE) && !IS(src2->status, BASE)) {
	/* look for next same data */
	while ((ret = strcmp(src1->filename, src2->filename))) {
	    if (ret < 0) src1 = src1->next;
	    if (ret > 0) src2 = src2->next;
	    /* no element found return */
	    if (IS(src1->status, BASE) || IS(src2->status, BASE)) 
		return (count);
	}
	
	/* backup next pointers */
	next1 = src1->next; next2 = src2->next;
	
	/* place dst to best previous position */
	while (!IS(dst->status, BASE) && strcmp(src1->filename, dst->filename) <= 0)
	    dst = dst->next;
	dst = dst->prev;
	
	/* only if they are filled */
	if (IS(src1->status, FILLED) && IS(src2->status, FILLED)) {
	    /* pass '.' and '..' when IGNORE_DOT option */
	    //REM if ((!IS(Options, GOPT_IGNORE_DOT) || !IS_DOTF(src1->filename)) &&
	    if ((diff = files_are_the_same(src1->desc, src2->desc, Diff, NULL)) || IS(Options, SHOW_ALL)) {
		/* should be place before current dst, but at the good place */
		dst = ft_add(dst, src1->filename, NULL, NULL);
		
		/* remove usage of c1->filename, in src1 if used by dst */
		if (src1->filename == dst->filename) 
		    dst->filename = strdup(src1->filename);
		
		/* else already exist */
		if (dst->desc) {
		    /* a test can be made with dst->desc ..., probably an
		     * error in source data */
		    warning("duplicate entry in sources");
		}
		else {
		    /* create difference description structure */
		    dst->desc = desc = POOL_ALLOC(t_file_diff);
		    desc->diff = diff;
		    desc->src1 = src1->desc; src1->desc = NULL;
		    desc->src2 = src2->desc; src2->desc = NULL;
		    
		    /* mark changed */
		    SET(dst->status, FILLED);
		    SET_PARENT(dst, CHANGED);
		    /* update counter */
		    count++;
		}
	    }
	    /* mark already seen */
	    UNSET(src1->status, FILLED);
	    UNSET(src2->status, FILLED);
	    
	    // printf("%s treated\n", src2->filename);
	    
	    // if (!src1->subtree) ft_del(src1, NULL, NULL);
	    // if (!src2->subtree) ft_del(src2, NULL, NULL);
	    
	}
	/* if directory, run into */
	if (src1->subtree && src2->subtree && 
	    (IS(src1->status, CHANGED) || IS(src2->status, CHANGED)) && 
	    !IS_PDOTF(src1) && !IS_PDOTF(src2)) {
	    /* get directory in dst or create it */
	    dst = ft_add(dst, src1->filename, NULL, NULL);
	    if (src1->filename == dst->filename) 
		dst->filename = strdup(dst->filename);
	    
	    count += ft_find_diff(src1, src2, dst);
	}
	
	/* go to next */
	src1 = next1; src2 = next2;
    }
    return (count);
}

/* cette fonction permet de déplacer le contenu d'un premier arbre dans un
 * deuxième arbre
 */
void    ft_move(t_ft *src, t_ft *dst) {
    t_ft  *next;
    int   ret;
    
    if (!src || !dst || !src->subtree) return;

    /* move full subtree */
    if (!dst->subtree && src->subtree) {
	t_ft *cur;
	
	/* change parent */
	src->subtree->parent = dst;
	for (cur = src->subtree->next; !IS(cur->status, BASE); cur = cur->next) {
	    if (IS_PDOTDOT(cur))
		cur->subtree = dst->parent->subtree;
	    cur->parent = dst;
	}
	/* change from src to dst */
	dst->subtree = src->subtree;
	src->subtree = NULL;
	return ;
    }

    src = src->subtree->next;
    dst = dst->subtree->next;

    while (!IS(src->status, BASE)) {
	/* backup next position */
	next = src->next;

	ret = 1;
	/* search for it in dst */
	while (!IS(dst->status, BASE) &&
	       (ret = strcmp(dst->filename, src->filename)) < 0)
	    dst = dst->next;

	if (ret == 0) {
	    /* element exist */
	    /* need to move description */
	    if (src->desc && IS(src->status, FILLED) && !dst->desc) {
		dst->desc = src->desc; 
		src->desc = NULL;
	    }
	    /* need to run into */
	    if (src->subtree) {
		if (IS_PDOT(src)) {
		    dst->subtree = dst->parent->subtree;
		    src->subtree = NULL;
		}
		else if (IS_PDOTDOT(src)) {
		    dst->subtree = dst->parent->parent->subtree;
		    src->subtree = NULL;
		}
		else {
		    ft_move(src, dst);
		}
	    }
	    ft_del(src, NULL, NULL);
	}
	else {
	    dst = dst->prev; /* go back to the previous one */
	    /* unchain from source tree */
	    LIST_UNCHAIN(src);
	    /* chain to destination tree */
	    LIST_CHAIN(dst, src, dst->next);

	    /* change parent hierarchy */
	    if (IS_PDOT(src))
		src->subtree = dst->parent->subtree;
	    else if (IS_PDOTDOT(src))
		src->subtree = dst->parent->parent->subtree;
	    else if (src->subtree) {
                t_ft   *cur;

		for (cur = src->subtree->next; !IS(cur->status, BASE); cur = cur->next) {
		    if (IS_PDOTDOT(cur))
			cur->subtree = dst->parent->subtree;
		}
	    }
	    src->parent = dst->parent;
	    
	    /* change destination current location */
	    dst = src;
	}
	src = next;
    }
}

/* cette fonction permet de déplacer le contenu de deux arbres dans un troisième
 */
void    ft_diff_move(t_ft *src, t_ft *dst, int opt) {
    t_ft        *next;
    t_file_diff *new = NULL;
    
    if (!src || !dst || !src->subtree) return;

    if (!dst->subtree) dst->subtree = NEWDIR(dst);
    
    src = src->subtree->next;
    dst = dst->subtree->next;

    while (!IS(src->status, BASE)) {
	/* backup next position */
	next = src->next;

	if (IS(src->status, FILLED) || src->subtree) {
	    /* should find or create a new element */
	    dst = ft_add(dst, src->filename, NULL, NULL);
	    if (src->filename == dst->filename) src->filename = NULL;
	}
	
	if (IS(src->status, FILLED) && src->desc) {
	    /* need to move description */
	    if (!(new = dst->desc)) {
		dst->desc = new = MALLOC(sizeof(*new));
		bzero(new, sizeof(*new));
		new->diff = 0xFFFF;
	    }
	    if (opt == ONLY_SRC1) new->src1 = src->desc;
	    if (opt == ONLY_SRC2) new->src2 = src->desc;
	    src->desc = NULL;
	    SET(dst->status, FILLED);
	    SET_PARENT(dst, CHANGED);
	    UNSET(src->status, FILLED);
	}
	
	/* need to run into */
	if (src->subtree) {
	    if (IS_PDOT(src))
		dst->subtree = dst->parent->subtree;
	    else if (IS_PDOTDOT(src))
		dst->subtree = dst->parent->parent->subtree;
	    else
		ft_diff_move(src, dst, opt);
	}
	
	ft_del(src, NULL, NULL);
	
	src = next;
    }
}

/* cette fonction permet d'initialiser la structure donnée en paramêtre
 */
void     ft_init(t_ft *d) {
    bzero(d, sizeof(*d));
    d->prev = d->next = d;
    d->parent = d;
    SET(d->status, BASE);
}

t_file_diff   *fct_free_diff_desc(void *data, t_file_diff *desc) {
    if (desc->src1) fct_free_file_desc(data, desc->src1);
    if (desc->src2) fct_free_file_desc(data, desc->src2);
    free(desc);
    return (NULL);
}

char  *build_diff_line(char *path, char *filename, t_file_diff *info) {
    static char tmp[BUFFER_LENGTH];
    static char opath[BUFFER_LENGTH];
    char        *s = tmp;
    char        *ppath;
    
    *s = 0;
    if (info->diff == 0) {
	ppath = opath;
	if (Output_Str)    ppath += sprintf(ppath, "%s/", Output_Str);
	if (Rewrite_Src1)  ppath += sprintf(ppath, "%s/", Rewrite_Src1);
	sprintf(ppath, "%s", path);
	s += sprintf(s, "= %s", build_line(opath, filename, info->src1));
	return (tmp);
    }

    if (info->src1 && IS(Options, SHOW_OLD)) {
	ppath = opath;
	if (Output_Str)    ppath += sprintf(ppath, "%s/", Output_Str);
	if (Rewrite_Src1)  ppath += sprintf(ppath, "%s/", Rewrite_Src1);
	sprintf(ppath, "%s", path);
	
	if (!info->src2) 
	    s += sprintf(s, "- %s", build_line(opath, filename, info->src1));
	else 
	    s += sprintf(s, IS(Options, SHOW_NEW)?"< %s\n":"< %s", 
			 build_line(opath, filename, info->src1));
    }
    if (info->src2  && IS(Options, SHOW_NEW)) {
	ppath = opath;
	if (Output_Str)    ppath += sprintf(ppath, "%s/", Output_Str);
	if (Rewrite_Src2)  ppath += sprintf(ppath, "%s/", Rewrite_Src2);
	sprintf(ppath, "%s", path);
	
	if (!info->src1) 
	    s += sprintf(s, "+ %s", build_line(opath, filename, info->src2));
	else 
	    s += sprintf(s, "> %s", build_line(opath, filename, info->src2));
    }
    if (*tmp) return (tmp);
    return (NULL);
}

int   output_diff_write(t_file_status *desc, t_ft *tree, int number) {
    return (output_file_write_(desc, tree, number, build_diff_line));
}

/* traduct a string options descriptions to flags
 */
int         output_diff_fcntl(t_file_status *env, int cmd) {
    return (0);
}



/* cette fonction se base sur les différents paramêtres pour
 * déterminer les différences entre les signatures de deux types de sources.
 * Aux types de sources sont associées des fonctions spécialisées. Cette 
 * définition est valable de la même manière pour la destination
 */
int flxcheck_main(int argc, char **argv) {
    /* tree for each source */  
    t_ft src1 = { &src1, &src1, &src1, NULL, NULL, NULL, BASE };
    t_ft src2 = { &src2, &src2, &src2, NULL, NULL, NULL, BASE };   
    /* tree for destination */
    t_ft dst  = { &dst, &dst, &dst, NULL, NULL, NULL, BASE|CHANGED };          
    t_ft tmp1, tmp2;   /* les bases temporaires */
    int  nb1 = 0, nb2 = 0;
    
    /* il faut initialiser les structures de manière à définir les 
     * fonctions associées aux sources */
    /* on initialise un premier type de source à partir d'un premier pointeur
     * sur arbre et vice-versa
     */
    if (!InputSrc1 || !InputSrc2) {
	arg_usage(flxcheck_poptions, "{source1 source2 | source1 [...] , source2 [...]}");
    }

    if (!Output) Output = output_alloc();
    output_add(Output, "stdout:", OutputTypes);

    /* il faut initialiser les bases temporaires à partir des bases originales */
    ft_init(&tmp1);
    ft_init(&tmp2);
    
    while (!input_eof(InputSrc1) || !input_eof(InputSrc2)) {
	/* la comparaison s'effectue sur les deux sources simultanément */

	/* on lit des données dans chacune des entrées vers un tampon */
	nb1 += input_read(InputSrc1, &src1);
	nb2 += input_read(InputSrc2, &src2);
	
	/* on compare les données deux à deux afin de compléter 'dst' */
	ft_find_diff(&src1, &src2, &dst);
	ft_find_diff(&src1, &tmp2, &dst);
	ft_find_diff(&tmp1, &src2, &dst);
	
	/* toutes les données identiques ont été détectée pour la partie 
	 * déterminée, on déplace le contenu de la source vers les tampons */
	ft_move(&src1, &tmp1);
	ft_move(&src2, &tmp2);


	/* visualiser les différences si possible */
	// ft_diff_move(&tmp1, &dst, ONLY_SRC1); 
	// ft_diff_move(&tmp2, &dst, ONLY_SRC2); 
	// output_write(output, &dst);
	// ft_free(&dst, (void*)fct_free_diff_desc, NULL);

	/* les sources sont vidées, il est possible de recommencer l'opération */
    }


    /* il n'y a plus de différences détectables
     * le reste sont des différences uniques */
    ft_diff_move(&tmp1, &dst, ONLY_SRC1); 
    ft_diff_move(&tmp2, &dst, ONLY_SRC2); 

    /* visualiser les différences */
    output_write(Output, &dst, 0);
    ft_free(&dst, (void*)fct_free_diff_desc, NULL);

    /* close all interface */
    input_free(InputSrc1);
    input_free(InputSrc2);
    output_free(Output);
    
    return (0);
}



int   flxcheck_pfct(int opt, t_param *param, char **flag, char **argv) {
    static int status = 1;  /* source 1 or source 2 */
    
    if (opt == O_HELP)  arg_usage(flxcheck_poptions, NULL);
    else if (opt == O_SH_ALL)    SET(Options, SHOW_SAME|SHOW_OLD|SHOW_NEW);
    else if (opt == O_SH_OLD)    UNSET(Options, SHOW_SAME|SHOW_NEW);
    else if (opt == O_SH_NEW)    UNSET(Options, SHOW_SAME|SHOW_OLD);
    else if (opt == O_IGN_MODE)  UNSET(Diff, DIFF_MODE);
    else if (opt == O_IGN_OWNER) UNSET(Diff, DIFF_OWNER);
    else if (opt == O_IGN_SUM)   UNSET(Diff, DIFF_CHECKSUM);
    else if (opt == O_IGN_SIZE)  UNSET(Diff, DIFF_SIZE);
    else if (opt == O_IGN_DATE)  UNSET(Diff, DIFF_TIME);
    else if (opt == O_IGN_LINK)  UNSET(Diff, DIFF_LINK);
    else if (opt == O_IGN_LDATE) UNSET(Diff, DIFF_LDATE);
    else if (opt == O_SH_HR)     SET(Options, GOPT_HUMAN_READABLE);
    else if (opt == O_REWRITE_SRC1) Rewrite_Src1 = strdup(*(argv+1));
    else if (opt == O_REWRITE_SRC2) Rewrite_Src2 = strdup(*(argv+1));
    else if (opt == O_OUTPUT)       Output_Str = strdup(*(argv+1));
    else if (opt == O_IGN_DOT)      SET(Options, GOPT_IGNORE_DOT);
    else if (opt == -1) {
	if (!strcmp(*argv, ",")) status = 3;
	else if (status == 1 || status == 2) {
	    /* add new source to source 1 */
	    if (!InputSrc1) InputSrc1 = input_alloc();
	    input_add(InputSrc1, *argv, InputTypes);

	    if (status == 1) { /* search alone ',' in others parameters */
		char **ptr = argv;
		while (*ptr && strcmp(*ptr, ",")) ptr++;
		if (*ptr) status = 2;
		else      status = 3;
	    }
	}
	else { /* status == 3 */
	    /* add new source to source 2 */
	    if (!InputSrc2) InputSrc2 = input_alloc();
	    input_add(InputSrc2, *argv, InputTypes);
	}
    }
    else return (-1);
    return (param ? param->minarg : 0);
}


