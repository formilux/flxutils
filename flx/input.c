#include <string.h>
#include "utils.h"
#include "source_type.h"
#include "input.h"
#include "flx_fcntl.h"


/* get data from current input and return the number of data readed 
 */
int input_read(t_db_input *in, void *tree) {
    t_device *current;
    int      nb = 0;

    /* look for pointer initializing */
    while (!in->eof && in->inputs) {
	/* get current pointer to input driver */
	current = SIMPLE_LIST_PTR(in->inputs);
	
	/* run read function associated with current source */
	if (current->opened->read) {
	    if (!(nb = current->opened->read(current->env, tree))) {
		if (current->opened->close)
		    current->env = current->opened->close(current->env);
		current->opened = NULL;
		FREE(current);
		SIMPLE_LIST_POP(in->inputs);
		in->eof = (in->inputs == NULL);
	    }
	    else {
		return (nb);
	    }
	}
    }
    return (nb);
}

int input_add(t_db_input *in, char *src, t_source_type *sourcestype) {
    char         tmp[BUFFER_LENGTH], *ptmp, *otmp = NULL;
    int          sel;
    t_device     *new;
    
    strcpy(tmp, src);
    if ((ptmp = backslashed_strchr(tmp, ':'))) {
	*ptmp++ = 0;
	
	/* look for options */
	if ((otmp = strchr(tmp, '+'))) *otmp++ = 0;
	
	/* find corresponding source */
	for (sel = 0; sourcestype[sel].name; sel++)
	    if (!strcmp(tmp, sourcestype[sel].name)) break;
	if (!sourcestype[sel].name) {
	    /* not found, error */
	    error("can't find source type %s (%s:%s)\n", tmp, tmp, ptmp); 
	    return (0);
	}
    }
    else {
	sel = 0;
	ptmp = tmp;
    }
    
    /* allocate memory for new device */
    new = MALLOC(sizeof(*new));
    new->opened = &sourcestype[sel];
    /* call of device initialize specific function */
    if (new->opened->open) {
	if (!(new->env = new->opened->open(ptmp, otmp))) {
	    error("can't open '%s'", src);
	    FREE(new);
	    return (0);
	}
	if (new->opened->fcntl) {
	    if (in->inputs) {
		Sorted &= ~INPUT_SORTED;
	    }
	    else {
		Sorted &=  (~INPUT_SORTED|
			    ((new->opened->fcntl(new->env, IS_SORTED) ? INPUT_SORTED : 0))); 
	    }
	}
    }
    else
	new->env = NULL;
    /* add in current driver list */
    SIMPLE_LIST_FILE(in->inputs, new);
    in->eof = 0;
    return (1);
}


inline int input_eof(t_db_input *in) {
    return (in->eof);
}

t_db_input *input_alloc() {
    t_db_input *in;

    in = MALLOC(sizeof(t_db_input));
    bzero(in, sizeof(t_db_input));
    return (in);
}

t_db_input *input_free(t_db_input *old) {
    t_device *current;
    
    while (old->inputs) {
	current = SIMPLE_LIST_POP(old->inputs);
	current->opened->close(current->env);
	FREE(current);
    }
    FREE(old);
    return (NULL);
}
