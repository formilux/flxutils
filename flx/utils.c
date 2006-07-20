#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>
#include <ctype.h>

#include "utils.h"

static char bigbuffer[BUFFLEN];

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
    char        *pbuff = bigbuffer;
	int         len = 0;
    
    while (*s && len < (BUFFLEN-1)) {
	if (strchr(toback, *s)) {
	    *pbuff++ = '\\';
	}
	*pbuff++ = *s++;
    }
	bigbuffer[len] = 0;
    return (bigbuffer);
}

/* escape unprintable char with its hexadecimal value (url encode form) */
char *escape_str(char *s) {
	char        *pbuff = bigbuffer;
	int         len = 0;

	while (*s && len < (BUFFLEN-4)) {
		if (flx_is_graph(*s)) {
			*pbuff++ = *s++;
			len++;
		}
		else {
			*pbuff++ = '%';
			*pbuff++ = DEC2HEX((*s >> 4) & 0xF);
			*pbuff++ = DEC2HEX(*s & 0xF);
			s++;
			len += 3;
		}
	}
	bigbuffer[len] = 0;
	return (bigbuffer);
}

/* unescape string from %xx string form */
char *unescape_str(char *s) {
	char        *pbuff = bigbuffer;
	int         len = 0;

	while (*s && len < (BUFFLEN-1)) {
		if (*s == '\\') {
			s++;
			*pbuff++ = *s++;
		}
		else if (*s == '%' && isxdigit(*(s+1)) && isxdigit(*(s+2))) {
			*pbuff++ = HEX2DEC(s[1])*16+HEX2DEC(s[2]);
			s += 3;
		}
		else {
			*pbuff++ = *s++;
		}
		len++;
	}
	bigbuffer[len] = 0;
	return (bigbuffer);
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

