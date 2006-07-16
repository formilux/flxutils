/* 
 * Signfs version 0.6, Copyright Benoit DOLEZ, 2002
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>

#include "flx.h"
#include "check.h"
#include "sign.h"
#include "utils.h"
#include "arg.h"


POOL_INIT(t_file_tree);

/* device for xdev option */
dev_t SingleDev = -1;
char  *DumpFilename = DUMPBASENAME;
int   RecursionLevel = 0;             /* follow recursion */
int   Diff = 0xFFFF;
int   Options = (SHOW_OLD|SHOW_NEW);
char  *Progname = NULL;

#define FLX_SIGN            1
#define FLX_CHECK           2


t_param FLX_poptions[] = {
    { 0, NULL, 0xFFFF, 0,
      "sign\t-h|<options> input [...]\n"
      "\t\tsign data" },
    { 0, NULL, 0xFFFF, 0,
      "check\t-h|<options>  src1 src2\n"
      "\t\tcheck difference between src1 and src2" },
    { 0, NULL, 0xFFFF, 0,
      "check\t-h|<options>  src1 [...] , src2 [...]\n"
      "\t\tcheck difference between srcs1 and srcs2" },
    { 0, NULL, 0, 0}
};


int main(int argc, char **argv) {
    int ret = 0, mode = 0;
    struct { int (*fctprm)(); void *dtaprm; int (*fctmain)(); } flx[] = {
	{ NULL, FLX_poptions, NULL },
	{ flxsign_pfct, flxsign_poptions, flxsign_main },
	{ flxcheck_pfct, flxcheck_poptions, flxcheck_main }
    };	

    /* look at parameter with program name */
    if (!strcmp(progname(argv[0]), "flxcheck"))     mode = FLX_CHECK ;
    else if (!strcmp(progname(argv[0]), "flxsign")) mode = FLX_SIGN ;
    else if (!strcmp(progname(argv[0]), "flx") && argc > 1) {
	if (!strcmp(argv[1], "check"))              mode = FLX_CHECK ;
	else if (!strcmp(argv[1], "sign"))          mode = FLX_SIGN ;
	else arg_usage(FLX_poptions, NULL);
	argc--; argv++;
    }
    else 
	arg_usage(FLX_poptions, NULL);

    ret = arg_init(argc, argv, flx[mode].dtaprm, flx[mode].fctprm);
    
    if (ret <= 0) exit(1);
    argc -= ret; argv += ret;
    
    /* execute program */
    return (flx[mode].fctmain(argc, argv));
}



