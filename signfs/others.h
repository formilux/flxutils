#include <sys/stat.h>
#include <sys/types.h>
#define BUFFLEN 256


char *right(mode_t mode);
char *dirname(char *);
char *basename(char *);
int  error(char *,...);
int  fatal_error(char *,...);
int  pferror(char *,...);
