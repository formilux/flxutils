#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include "others.h"


// fonction to return write in 'ls -l' format
char *right(mode_t m) {
    static char dsc[10];
    dsc[0]=(m&S_IRUSR)?'r':'-';
    dsc[1]=(m&S_IWUSR)?'w':'-';
    dsc[2]=(m&S_IXUSR)?'x':'-';
    dsc[3]=(m&S_IRGRP)?'r':'-';
    dsc[4]=(m&S_IWGRP)?'w':'-';
    dsc[5]=(m&S_IXGRP)?'x':'-';
    dsc[6]=(m&S_IROTH)?'r':'-';
    dsc[7]=(m&S_IWOTH)?'w':'-';
    dsc[8]=(m&S_IXOTH)?'x':'-';
    dsc[9]=0;
    return(dsc);
}

// return directory name of a [path/]filename string
// the return value is in static value
char *dirname(char *str) {
    static char tmp[8192],*p;
    strcpy(tmp,str);
    if ((p=strrchr(tmp,'/'))) return (*p=0,tmp);
    return (NULL);
}

// return the filename of a [path/]filename string
char *basename(char *str) {
    static char *p;
    if (!(p=strrchr(str,'/'))) return str;
    return (p+1);
}

int fatal_error(char *message, ...) {
    va_list argp;
    char    buff[BUFFLEN];
    
    va_start(argp,message);
    vsnprintf(buff,BUFFLEN,message,argp);
    write(2,buff,strlen(buff));
    va_end(argp);
    return (2);
}

int error(char *message, ...) {
    va_list argp;
    char    buff[BUFFLEN];
    
    va_start(argp,message);
    vsnprintf(buff,BUFFLEN,message,argp);
    write(2,buff,strlen(buff));
    va_end(argp);
    return (2);
}

int pferror(char *message, ...) {
    va_list argp;
    char    buff[BUFFLEN];
    
    va_start(argp,message);
    vsnprintf(buff,BUFFLEN,message,argp);
    perror(buff);
    va_end(argp);
    return (2);
}
