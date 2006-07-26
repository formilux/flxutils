#ifndef __ARG_H__
#define __ARG_H__

#include <stdio.h>

typedef struct p_param t_param;

struct p_param {
    char shortopt;
    char *longopt;
    int  optnum;
    int  minarg;
    char *help;
};

extern char    *Progname ;

/* example: use of t_param
t_param sample_poptions[] = {
    { 'h', "help", 0xFFFF, 0, "this help" },
    { 0, NULL, 0, 0}
};

int sample_pfct(int opt, t_param *param, char **flag, char **argv) {
    if (opt == 0xFFFF) arg_usage(sample_poptions, NULL);
    return (param ? param->minarg : 0);
}

int main(int argc, char **argv) {
    int  ret;

    ret = arg_init(argc, argv, sample_poptions, sample_pfct);
    if (ret <= 0) return (1);
    argc -= ret; argv += ret;

    ...
}

*/

int   arg_init(int argc, char **argv, t_param *paramoptions, int (*paramfct)()) ;
char  *progname(char *);
void  arg_usage(t_param *p, char *message, ...);


#endif /* __ARG_H__ */



