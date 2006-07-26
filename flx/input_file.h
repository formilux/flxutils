#ifndef __INPUT_FILE_H__
#define __INPUT_FILE_H__

#include <stdio.h>

#include "utils.h"
#include "flx.h"
#include "flx_fcntl.h"

typedef struct s_file_status t_file_status;

struct s_file_status {
    FILE          *fd;                         /* pointer to current file */
    char          bpath[BUFFER_LENGTH];        /* path for real directory */
    char          cpath[BUFFER_LENGTH];        /* path for saving infos */
    void          *dirnames;                   /* stack for directory names */
    void          *filenames;                  /* stack for file names */
    int           status;                      /* reading status */
    void          *fdstack;                    /* output file descriptor stack */
    int           recursion, prev_recursion;   /* recursion requiered */
    int           options;                     /* options flags */
};

extern int            input_file_fcntl(t_file_status *env, int cmd);

extern int            input_file_read(t_file_status *spec, t_ft *tree);
extern t_file_status  *input_file_open(char *desc, char *opts);
extern t_file_status  *input_file_close(t_file_status *spec);

extern t_file_status  *input_stdin_open(char *desc, char *opts);

#endif /* __INPUT_FILE_H__ */


