#include <stdio.h>
#include "input.h"
#include "output.h"
#include "source_type.h"
#include "arg.h"
#include "input_fs.h"
#include "input_file.h"
#include "output_file.h"

/* source type have the form :
 * [type[(type_options)]:]file[:origin1][=rewrite1],[origin2[=rewrite2]]
 * if type isn't set, type = 'fs'
 * the end of the string was treat by specific init function
 * type_options : [!]type_option[,type_options]
 * input fs specific options :
 *    - xdev : don't go across fs (default)
 *    - flw  : follow symlinks links
 *    - depth : directory depth to walk
 * output file tree specific options :
 *    - level=N : subdirectory level to create
 * output specific options :
 *    - long : human readable form
 */

/* command line is:
 * ./flx sign [options] [inputs]
 * options are :
 *   -h,--help             : this help
 */


static t_source_type InputTypes[] = { 
    { "fs",   
      (void*)input_fs_open,    (void*)input_fs_read,   NULL, (void*)input_fs_close,  
      (void*)input_fs_fcntl },
    { "file", 
      (void*)input_file_open,  (void*)input_file_read, NULL, (void*)input_file_close,
      (void*)input_file_fcntl },
    { "stdin",
      (void*)input_stdin_open, (void*)input_file_read, NULL, (void*)input_file_close,
      (void*)input_file_fcntl },
    { NULL }
};
static t_source_type OutputTypes[] = { 
    { "file", 
      (void*)output_file_open,    NULL, (void*)output_file_write, (void*)output_file_close, 
      (void*)output_file_fcntl },
    { "stdout",(void*)output_stdout_open, NULL, (void*)output_file_write, (void*)output_file_close,
      (void*)output_file_fcntl },
    { NULL }
};

static void *SignOutput = NULL, *SignInput = NULL;


#define O_HELP                   1
#define O_OUTPUT                 2
#define O_IGN_DOT                3
#define O_IGN_DEPTH              4

t_param flxsign_poptions[] = {
    { 'h', "help", O_HELP, 0,     
      "-h,--help           show this help"},
    { 'o', "output", O_OUTPUT, 1, 
      "-o,--output <file>  select output"},
    { 0, "ignore-dot", O_IGN_DOT, 0,
      "--ignore-dot        do not store '.' and '..'" },
    { 0, "no-depth", O_IGN_DEPTH, 0,
      "--no-depth          do not run across directories" },
    { 0,   NULL,   0 }
} ;

int flxsign_pfct(int opt, t_param *param, char **flag, char **argv) {
    if (opt == O_HELP)  arg_usage(flxsign_poptions, NULL);
    else if (opt == O_OUTPUT) {
	if (!SignOutput)  SignOutput = output_alloc();
	output_add(SignOutput, argv[1], OutputTypes);
    }
    else if (opt == O_IGN_DOT)      SET(Options, GOPT_IGNORE_DOT);
    else if (opt == O_IGN_DEPTH)    SET(Options, GOPT_IGNORE_DEPTH);
    else if (opt == -1) {
	if (!SignInput) SignInput = input_alloc();
	input_add(SignInput, *argv, InputTypes);
    }
    else return (-1);
    return (param ? param->minarg : 0);
}


int flxsign_main(int argc, char **argv) {
    t_ft Root = { &Root, &Root, &Root /* parent */, NULL /* subtree */ , 
		  NULL /* filename */, NULL /* desc */, BASE|CHANGED /* status */ };
    int  nb;
    
    if (!SignInput)  {
	SignInput = input_alloc();
	input_add(SignInput, "fs:.", InputTypes);
    }
    
    if (!SignOutput) {
	SignOutput = output_alloc();
	output_add(SignOutput, "stdout:", OutputTypes);
    }

    while ((nb = input_read(SignInput, &Root))) {
	output_write(SignOutput, &Root, nb);
	ft_free(&Root, (void*)fct_free_file_desc, NULL);
    }
    output_write(SignOutput, &Root, 0);
    ft_free(&Root, (void*)fct_free_file_desc, NULL);
    
    output_free(SignOutput);
    input_free(SignInput);
    return (0);
}


