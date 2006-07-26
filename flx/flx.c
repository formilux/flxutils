#include <string.h>
#include "utils.h"
#include "flx.h"


/* cette fonction ajoute et place un chainon dans la chaine t_file_tree (brother)
 * le pointeur pfilename est le pointeur à utiliser pour le nom, attention
 * aux allocations (il n'est pas logique qu'il contienne un '/' 
 * elle retourne un pointeur sur le nouveau chainon
 */
t_ft *ft_add(t_ft *base, char *pfilename, void *(*fct_init_desc)(), void *data) {
    t_ft   *new;
    int    ret = 1;

    /* base initialisation */
    if (!pfilename || !base) {
	new = POOL_ALLOC(t_file_tree);
	new->prev = new->next = new;
	/* be the root dir */
	new->parent = (!base) ? new : base;
	new->filename = NULL;
	new->desc = fct_init_desc ? fct_init_desc(data) : NULL;
	new->subtree = NULL;
	new->status = BASE; 
	return (new);
    }
    /* possibly pass first element */
    if (!base->filename) base = base->next;
    /* find best place */
    while (base->filename && (ret = strcmp(pfilename, base->filename)) > 0) 
	base = base->next;
    /* already exist */
    if (ret == 0) return (base);
    
    /* create new element and fill it */
    new = POOL_ALLOC(t_file_tree);
    /* data */
    new->status = 0;
    new->desc = NULL;
    new->filename = pfilename;
    
    /* description pointer initialization */
    new->desc = fct_init_desc ? fct_init_desc(data) : NULL;

    /* parent is the same for each soon, or be root dir */
    new->parent = (!base->parent) ? new : base->parent;
    
    /* special traitment for '.' and '..' */
    new->subtree = NULL;
    if (base && base->parent && IS_DOT(pfilename))  
	new->subtree = base->parent->subtree;
    if (base && base->parent && base->parent->parent && IS_DOTDOT(pfilename)) 
	new->subtree = base->parent->parent->subtree;
    
    /* place at the before current base */
    LIST_CHAIN(base->prev, new, base);
    return (new);
}

/* cette fonction supprime un chainon dans une chaine t_file_tree
 * elle retourne le pointeur sur le chainon précédent
 */
t_ft *ft_del(t_ft *old, void (*fct_free_desc)(void *data, void *desc), void *data) {
    t_ft  *prev;
    
    prev = LIST_UNCHAIN(old);
    if (old->filename) FREE(old->filename);
    if (old->desc) {
	if (fct_free_desc) fct_free_desc(data, old->desc);
	//	else warning("Possibly unfree memory");
    }
    if (old->subtree && !IS_PDOTF(old)) {
	t_ft *c;
	for (c = old->subtree->next; c != old->subtree; c = c->next)
	    c = ft_del(c, fct_free_desc, data);
	POOL_FREE(t_file_tree, old->subtree);
    }
    POOL_FREE(t_file_tree, old);
    return (prev);
}

/* return (and build) tree structure for matching path */
/* path : directory to search (and create)
 * tree : current directories structure
 * (return) : pointer to a new base tree
 */
t_ft *ft_get(t_ft *tree, char *path) {
    char          tfilename[BUFFER_LENGTH];
    char          *beginpath;
    int           len;

    /* no more subtree, break to current */
    while (path && *path) {
	/* the first level path filename */
	beginpath = path;
	if ((path = strchr(beginpath, '/'))) path++;
	len = (path ? (path - beginpath - 1) : strlen(beginpath));
	
	/* put filename in buffer */
	strncpy(tfilename, beginpath, len); tfilename[len] = 0;
	
	/* want subdir but don't exist, should create */
	if (!tree->subtree) tree->subtree = NEWDIR(tree);
	
	/* get it in current directory */
	tree = ft_add(tree->subtree, tfilename, NULL, NULL);
    
	/* new entry, allocate memory for filename */
	if (tree->filename == tfilename)
	    tree->filename = strdup(tree->filename);
	
    }
    return (tree);
}

