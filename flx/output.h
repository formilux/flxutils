#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include "source_type.h"

//#define DOFREE         (1<<0)
//#define DOFLUSH        (1<<1)
//#define DOINIT         (1<<2)
//#define GOSUBTREE      (1<<3)
//#define GOBASE         (1<<4)
//#define GOPARENT       (1<<5)

typedef struct s_db_output t_db_output;

struct s_db_output {
    void  *outputs;              /* list of output destination */
};

extern int         output_write(t_db_output *out, void *tree, int number);
extern t_db_output *output_alloc();
extern t_db_output *output_free(t_db_output *old);
extern int         output_add(t_db_output *out, char *desc, t_source_type *sourcestype);

int         ft_foreach(void *data, t_ft *tree, int flag, int (*fct_doit)());

int         ft_free(t_ft *tree, void *(*fct_free)(void *data, void *desc), void *data);
t_file_desc   *fct_free_file_desc(void *data, t_file_desc *desc);

#endif /* __OUTPUT_H__ */




