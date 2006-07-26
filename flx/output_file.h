#ifndef __OUTPUT_FILE_H__
#define __OUTPUT_FILE_H__

#include "flx.h"
#include "input_file.h"
#include "flx_fcntl.h"

extern int    output_file_fcntl(t_file_status *env, int cmd);

extern int   output_file_write_(t_file_status *spec, t_ft *tree, int number, char *(*fct)());
extern int   output_file_write(t_file_status *spec, t_ft *tree, int number);
extern t_file_status  *output_file_open(char *desc, char *opts);
extern t_file_status  *output_file_close(t_file_status *spec);

extern t_file_status  *output_stdout_open(char *desc, char *opts);


#endif /* __OUTPUT_FILE_H__ */


