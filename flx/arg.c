#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "flx.h"
#include "arg.h"


#define TUNDEF   1
#define TSHORT   2
#define TLONG    4

char    *Progname ;


/* show usage for program with ParamOptions global pointer 
 * message : message to see (stdarg form), NULL mean no message
 * exit with value 1
 */
void arg_usage(t_param *p, char *message, ...) {
    va_list argp;
    
    if (message) {
	va_start(argp, message);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, message, argp);
	fprintf(stderr, "\n");
	va_end(argp);
    }
    fprintf(stderr, "%s %s, %s\n", PROGRAM_NAME, PROGRAM_VERSION, COPYRIGHT);
    fprintf(stderr, "usage: %s [options] [...]\n", Progname);
    while(p && p->optnum) {
	fprintf(stderr, "\t%s\n", p->help);
	p++;
    }
    exit(1);
}

/* return programme name
 * pathprogname : argv[0]
 * (return) pointer to the programme name in this string
 */
char *progname(char *pathprogname) {
    if (Progname) return (Progname);
    if ((Progname = strrchr(pathprogname, '/'))) 
	Progname++;   
    else 
	Progname = pathprogname;
    return (Progname);
}

/* argument checking
 * argc, argv : main parameters 
 * (return): 1 if no error
 */
int arg_init(int argc, char **argv, t_param *paramoptions, int (*paramfct)()) {
    char    *flag;
    int     ft = TUNDEF, opt, ret, nbargc = 1;
    t_param *p = NULL;
    
    argc--; argv++; flag = *argv;
    while (argc > 0) {

	opt = -1;
	// printf("%d,%d: %s -> %s\n", argc, ft, *argv, flag);
	if (*flag == '-' && ft == TUNDEF)      ft = TSHORT; 
	else if (*flag == '-' && ft == TSHORT) ft = TLONG;
	else if (*flag == 0 && ft == TLONG)    break;
	else if (*flag == 0 && ft == TSHORT)   break;
	else if (ft == TSHORT) {
	    for (p = paramoptions; p->optnum && p->shortopt != *flag; p++);
	    opt = p->optnum;
	}
	else if (ft == TLONG) {
	    for (p = paramoptions; p->optnum && 
		     (!p->longopt || strcmp(flag, p->longopt)); p++);
	    opt = p->optnum;
	}
	if (opt == -1 && ft != TUNDEF ) { flag++; continue; }
	if (opt == 0 && ft == TSHORT) 
	    arg_usage(paramoptions, "Unkwnon short argument: -%c", *flag);
	if (opt == 0 && ft == TLONG) 
	    arg_usage(paramoptions, "Unkwnon long argument: --%s", flag);
	if (opt > 0 && p && argc < p->minarg)
	    arg_usage(paramoptions, "Missing argument: %s", p->help);
	
	if ((ret = paramfct(opt, p, &flag, argv)) == -1 && ft != TUNDEF)
	    arg_usage(paramoptions, "Bad argument: %s", p->help);
	else if (ret == -1) 
	    break;
	else {
	    argc -= ret; argv += ret;
	    nbargc += ret;
	}

	if (!*(flag+1) || ft != TSHORT) {
	    argc--; argv++;
	    nbargc++;
	    ft = TUNDEF;
	    flag = *argv;
	} 
	else
	    flag++;
    }
    return (nbargc);
}

