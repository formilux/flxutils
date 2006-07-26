#include <string.h>
#include "utils.h"
#include "flx.h"
#include "output.h"
#include "source_type.h"
#include "flx_fcntl.h"

int   Sorted = (INPUT_SORTED|OUTPUT_SORTED); /* input/output sorted status */

t_file_desc   *free_file_desc(t_file_desc *desc) {
    if (!desc) return (NULL);
    if (desc->md5) FREE(desc->md5);
    if (desc->link) FREE(desc->link);
    FREE(desc);
    return (NULL);
}

t_file_desc   *fct_free_file_desc(void *data, t_file_desc *desc) {
    return (free_file_desc(desc));
}

/* this function free file tree and return 1 if the free is completed 
 * it doesn't free the element (base)
 */
int         ft_free(t_ft *tree, void *(*fct_free)(void *data, void *desc), void *data) {
    t_ft  *next;
    
    /* nothing have change, do not free */
    if (IS(Sorted, OUTPUT_SORTED) && !IS(Sorted, INPUT_SORTED)) return (0);

    /* remove description */
    if (IS(tree->status, FILLED)) {
	if (tree->desc && fct_free) tree->desc = fct_free(data, tree->desc);
	UNSET(tree->status, FILLED);
    }
    /* check for recursion */
    if (!IS(tree->status, CHANGED) || IS(tree->status, SORTING) || !tree->subtree) {
	UNSET(tree->status, CHANGED);
	return (1);
    }
    /* look for first soon */
    tree = tree->subtree->next;
    
    while (!IS(tree->status, BASE)) {
	
	/* pass unchanged elements */
	if (!IS(tree->status, CHANGED)) { tree = tree->next; continue ; }
	
	/* unset filled and remove data */
	if (IS(tree->status, FILLED)) {
	    if (tree->desc && fct_free) tree->desc = fct_free(data, tree->desc);
	    UNSET(tree->status, FILLED);
	}
	/* stop if necessary */
	if (IS(Sorted, OUTPUT_SORTED) && IS(tree->status, SORTING))
	    return (0);

	/* check if having to look at subtree */
	if (tree->subtree && !IS_PDOTF(tree)) {
	    if (ft_free(tree, fct_free, data)) {
		if (tree->subtree == tree->subtree->next)
		    POOL_FREE(t_file_tree, tree->subtree);
	    }
	    else return (0);
	}
	/* free filename memory */
	if (tree->filename) FREE(tree->filename);
	/* unchain and go to the next */
	next = tree->next;
	UNSET(tree->status, CHANGED);
	if (fct_free) {
	    LIST_UNCHAIN(tree);
	    POOL_FREE(t_file_tree, tree);
	}
	tree = next;
    }
    return (1);
}
    
int         output_write(t_db_output *out, void *data, int number) {
    t_device     *dst;
    void         *current;
    int          valid = 1;
    
    for (current = out->outputs; current ; current = SIMPLE_LIST_NEXT(current)) {
	dst = SIMPLE_LIST_PTR(current);
	if (dst->opened->write)
	    valid = dst->opened->write(dst->env, data, number) && valid;
    }
    return (valid);
}

int         output_add(t_db_output *out, char *desc, t_source_type *sourcestype) {
    char         tmp[BUFFER_LENGTH], *ptmp, *otmp = NULL;
    int          i;
    t_device     *new;
    
    strcpy(tmp, desc);
    if ((ptmp = backslashed_strchr(tmp, ':'))) {
	*ptmp++ = 0;

	/* look for options */
	if ((otmp = strchr(tmp, '+'))) *otmp++ = 0;
	
	/* find corresponding source */
	for (i = 0; sourcestype[i].name; i++)
	    if (!strcmp(tmp, sourcestype[i].name)) break;
	if (!sourcestype[i].name) {
	    /* not found, error */
	    error("can't find source type %s (%s:%s)", tmp, tmp, ptmp); 
	    return (0);
	}
    }
    else {
	error("no source type specified");
	return (0);
    }

    /* allocate memory for new device */
    new = MALLOC(sizeof(*new));
    new->opened = &sourcestype[i];
    if (new->opened->open) {
	if (!(new->env = new->opened->open(ptmp, otmp))) {
	    error("can't open '%s'", desc);
	    FREE(new);
	    return (0);
	}
	if (new->opened->fcntl)
	    Sorted |= (new->opened->fcntl(new->env, IS_SORTED) ? OUTPUT_SORTED : 0); 
    }
    else
	new->env = NULL;
    SIMPLE_LIST_PUSH(out->outputs, new);
    return (1);
}

t_db_output *output_alloc() {
    t_db_output *out;

    out = MALLOC(sizeof(*out));
    bzero(out, sizeof(*out));
    return (out);
}

t_db_output *output_free(t_db_output *old) {
    t_device   *dst;
    
    while (old->outputs) {
	dst = SIMPLE_LIST_POP(old->outputs);
	if (dst->opened->close)
	    dst->opened->close(dst->env);
	FREE(dst);
    }
    FREE(old);
    return (NULL);
}

