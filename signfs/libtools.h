/*
 * File: libtools.c
 * Autors: Willy Tarreau <willy@ant-computing.com>
 */

/*
 * libtools.h : definition des macros et fonctions utilisees par les differents outils
 */
#ifndef _LIBTOOLS_H
#define _LIBTOOLS_H

#include <stdio.h>

/*
 * macros spécifiques aux logs
 */

/* définition des niveaux de log : ces flags peuvent etre combines par un "ou logique"
 * lors de l'appel de log_open() ou de log_setlevel().
 */
#define LOG_LALL	(~0)
#define LOG_LNONE	(0)

#define LOG_LFATAL	(1<<0)
#define LOG_LERR	(1<<1)
#define LOG_LWARN	(1<<2)
#define LOG_LSTAT       (1<<3)
#define LOG_LFUNC       (1<<4)
#define LOG_LPERF       (1<<5)
#define LOG_LDEBUG	(1<<6)
#define LOG_LAPP        (1<<7)
#define LOG_LSEC        (1<<8)
#define LOG_LOPEN	(1<<26)
#define LOG_LCLOSE	(1<<27)
#define LOG_LFILE	(LOG_LOPEN | LOG_LCLOSE)
#define LOG_LMALLOC	(1<<28)
#define LOG_LFREE	(1<<29)
#define LOG_LMEMORY	(LOG_LMALLOC | LOG_LFREE)
#define LOG_LSOCKET	(1<<30)
#define LOG_LCONNECT	(1<<31)

#define LOGLEVEL_NAMES	{"FTL","ERR","WRN","STA","FNC","PRF","DBG","APP", \
                         "","","","","","","","",\
                         "","","","","","","","",\
                         "", "","OPN","CLO","ALC","FRE","SKT","CON"}

/* imprime un log sur la position courante dans le fichier sous la forme "@<fichier:ligne>\n" */
#define LOG_DEBUG	Log(LOG_LDEBUG,"DBG <%s:%d>\n", __FILE__, __LINE__)


/*
 * Gestion des variables de configuration
 */

/* longueur maximale d'un identifiant */
#define CONF_IDLEN	50
#define CONF_LINELEN	256


/*
 * Traitements sur des chaines
 */

/* copie avec contrôle de bornes et ajout du zero terminal */
#define strcpy0(a,b)    strncpy0(a,b,sizeof(a)-1)

/*
 * symboles définis dans libtools.c
 */
extern unsigned int LogLevel;

int Abort(char *fmt, ...);
unsigned long Ascii2Long(char *from, int nb);
void LogDump(char *d, int start, int l);
void RawDump(char *d, int start, int l);
void LogOpen(char *file, unsigned int lev);
void LogClose();
int LogSetLevel(int and, int or);
int Log(int level, const char *fmt, ...);
int ConfRead(char *name);
char *ConfReadLine(FILE *file, char *ligne, int len);
void ConfSetStr(char *id, char *val);
void ConfSetInt(char *id, long int val);
char *ConfGetStr(char *id);
long int ConfGetInt(char *id);
char *ConfDvlpStr(char *out, char *in, int lmax);
long int ConfParse(char *arg);
char *strncpy0(char *dest, const char *src, int lmax);
char *strcut(char *s, int l);
char *base64toascii(char *ascii, char *base64);
char *asciitobase64(char *ascii, char *base64);
char *asciintobase64(char *ascii, int len, char *base64);
char *extracttoken(const char *from, const char *token, char *to, int len);
struct timeval *tv_delay(struct timeval *tv, int ms);
struct timeval *tv_wait(struct timeval *tv, int ms);
int tv_cmp(struct timeval *tv1, struct timeval *tv2);
int tv_past(struct timeval *tv);
unsigned long tv_delta(struct timeval *tv1, struct timeval *tv2);
struct timeval *tv_ms(int ms);
struct timeval *tv_delayfrom(struct timeval *tv, struct timeval *from, int ms);
struct timeval *tv_now(struct timeval *tv);
struct timeval *tv_set(struct timeval *tv, struct timeval *from);

#endif  /* _LIBTOOLS_H */
