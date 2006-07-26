#ifndef __INPUT_H__
#define __INPUT_H__

#include "source_type.h"

/* input structure */
typedef struct s_db_input t_db_input;

struct s_db_input {
    t_source_type *opened;       /* source access structure description */
    int  eof;                    /* marker for end of file */
    void  *inputs;               /* list of inputs */
};


extern int        input_read(t_db_input *in, void *tree);
extern int        input_eof(t_db_input *in);
extern t_db_input *input_alloc();
extern t_db_input *input_free(t_db_input *old);
extern int        input_add(t_db_input *in, char *desc, t_source_type *sourcestype);

#endif /* __INPUT_H__ */


