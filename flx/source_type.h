#ifndef __SOURCE_TYPE_H__
#define __SOURCE_TYPE_H__

#include "flx.h"


/* struct definition for all type of source */
typedef struct s_source_type t_source_type;
struct s_source_type {
    char    *name;
    
    void    *(*open)(char *str_desc, char *opts);
    int     (*read)(void *desc, t_ft *tree);
    int     (*write)(void *desc, t_ft *tree, int number);
    void    *(*close)(void *desc);
    int     (*fcntl)(void *env, int cmd);
};


/* device environnement */
typedef struct s_device t_device;

struct s_device {
    t_source_type *opened;
    void          *env;
};


#endif /* __SOURCE_TYPE_H__ */
