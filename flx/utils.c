#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>

#include "utils.h"

POOL_INIT(p2void);

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
    va_end(argp);
    write(2, "Fatal: ", 7);
    write(2, buff, strlen(buff));
    write(2, "\n", 1);
    exit (3);
}

int error(char *message, ...) {
    va_list argp;
    char    buff[BUFFLEN];
    
    va_start(argp,message);
    vsnprintf(buff,BUFFLEN,message,argp);
    va_end(argp);
    write(2, "Error: ", 7);
    write(2,buff,strlen(buff));
    write(2, "\n", 1);
    return (2);
}

void warning(char *message, ...) {
    va_list argp;
    char    buff[BUFFLEN];
    
    va_start(argp,message);
    vsnprintf(buff,BUFFLEN,message,argp);
    va_end(argp);
    write(2, "Warning: ", sizeof("Warning: "));
    write(2, buff, strlen(buff));
    write(2, "\n", 1);
    return ;
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

/* strchr but bybass backslash '\\' characters */
char *backslashed_strchr(char *s, char c) {
    while (*s) {
	if (*s == '\\' && *(s+1)) s++;
	else if (*s == c) return (s);
	s++;
    }
    return (NULL);
}

/* strchr with multiple characters but bypass '\\' characters */
char *backslashed_strmchr(char *s, char *mc) {
    char  *c;
    
    while (*s) {
	if (*s == '\\' && *(s+1)) s++;
	else 
	    for (c = mc; *c ; c++)
		if (*s == *c) return (s);
	s++;
    }
    return (NULL);
}

/* return the string with characters 'toback' backslashed */
char *backslashed_str(char *s, char *toback) {
    static char buff[BUFFLEN];
    char        *pbuff = buff;
    
    while (*s) {
	if (strchr(toback, *s)) {
	    *pbuff++ = '\\';
	}
	*pbuff++ = *s++;
    }
    *pbuff = 0;
    return (buff);
}

void *push_str_sorted(void *ptr, char *str) {
    void *c;
    
    if (!ptr || strcmp(str, SIMPLE_LIST_PTR(ptr)) < 0)
	SIMPLE_LIST_PUSH(ptr, str);
    else {
      for (c = ptr; SIMPLE_LIST_NEXT(c) &&
	     strcmp(SIMPLE_LIST_NEXT_PTR(c), str) < 0 ;
	  c = SIMPLE_LIST_NEXT(c));
      SIMPLE_LIST_PUSH(SIMPLE_LIST_NEXT(c), str);
    }
    return (ptr);
}

