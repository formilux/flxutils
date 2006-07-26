#ifndef __INPUT_FS_H__
#define __INPUT_FS_H__

#include "utils.h"
#include "flx.h"
#include "flx_fcntl.h"

typedef struct s_fs_status t_fs_status;

struct s_fs_status {
    char         bpath[BUFFER_LENGTH];        /* current real path */ 
    char         cpath[BUFFER_LENGTH];        /* current path representation */
    off_t        off_dir;                     /* dir offset for backup/restore */
    int          status;                      /* action status */
    int          options;                     /* read or write options */
    void         *dirnames;                   /* stack for directory backup */
    dev_t        dev;                         /* base device number */
    int          depth;                       /* depth level */
};

extern int           input_fs_fcntl(t_fs_status *env, int cmd);

extern int           input_fs_read(t_fs_status *spec, t_ft *tree);
extern t_fs_status   *input_fs_open(char *desc, char *opts);
extern t_fs_status   *input_fs_close(t_fs_status *spec);

#endif /* __INPUT_FS_H__ */


