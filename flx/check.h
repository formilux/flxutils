

#ifndef __CHECK_H__
#define __CHECK_H__

#include "arg.h"

/* structure de définition des données */

typedef struct s_file_diff t_file_diff;

struct s_file_diff {
#define ONLY_SRC1 (1<<0)
#define ONLY_SRC2 (1<<1)
    unsigned int diff;
    t_file_desc  *src1;
    t_file_desc  *src2;
};

extern t_param flxcheck_poptions[];

extern int flxcheck_pfct(int opt, t_param *param, char **flag, char **argv) ;
extern int flxcheck_main(int argc, char **argv);


#endif /* __CHECK_H__ */

