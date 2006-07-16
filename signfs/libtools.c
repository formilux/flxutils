/*
 * File: libtools.c
 * Autors: Willy Tarreau <willy@ant-computing.com>
 */

/*
 * libtools.c : fonctions utilisees par les differents outils
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "libtools.h"

#define CONFPARSE ConfParse

#ifdef CONFPARSE
extern long int CONFPARSE(char *str);
#endif

static char *loglevel_names[] = LOGLEVEL_NAMES;
static FILE *log = NULL;
unsigned int LogLevel = 0;

typedef struct s_varlist t_varlist;
/* structure locale pour les variables */
struct s_varlist {
    char id[CONF_IDLEN];
    char *val;
    t_varlist *next;
};

static t_varlist varlist = { "", NULL, NULL };


/* Arreter avec un ABORT apres avoir affiche un message d'erreur sur STDERR.
 * Cette fonction est de type int pour pouvoir etre inseree dans une expression.
 * Elle ne retourne jamais.
 */
int Abort(char *fmt, ...) {
    va_list argp;
    struct timeval tv;
    struct tm *tm;

    va_start(argp, fmt);

    gettimeofday(&tv, NULL);
    tm=localtime(&tv.tv_sec);
    fprintf(stderr, "[ABT] %03d %02d%02d%02d pid=%d, cause=",
	    tm->tm_yday, tm->tm_hour, tm->tm_min, tm->tm_sec, getpid());
    vfprintf(stderr, fmt, argp);
    fflush(stderr);
    va_end(argp);
    abort();
    return 0;	// juste pour eviter un warning a la compilation
}

/* cette fonction extrait la valeur numérique correspondant à la chaine ASCII
 * de <nb> chiffres à la position <from>
 */
unsigned long Ascii2Long(char *from, int nb) {
    unsigned long ret=0;
    while (nb-- && *from)
	ret=ret*10+(*from++-'0');
    return ret;
}


/* effectue un dump hexa de la structure pointee par <d>, a partir de la position
 * <start> et pour <l> octets
 */
void LogDump(char *d, int start, int l) {
    int i,j;
    if (log == NULL)
	Abort("Tentative d'ecriture dans un log non ouvert\n");

    for (i=start & -16;i<start+l;i+=16) {
	fprintf(log,"%04x: ",i);
	for (j=0; j<16; j++) {
	    if ((i+j>=start) && (i+j<start+l))
		fprintf(log,"%02x ",(unsigned char)d[i+j]);
	    else
		fprintf(log,".. ");
	    if (j==7) fprintf(log,"- ");
	}
	fprintf(log,"  ");
	for (j=0; j<16; j++) {
	    if ((i+j>=start) && (i+j<start+l) && isprint(d[i+j]))
		fputc(d[i+j],log);
	    else
		fputc('.',log);
	}
	fputc('\n',log);
    }
    fflush(log);
}

 
/* effectue un dump de la structure pointee par <d>, a partir de la position
 * <start> et pour <l> octets
 */
void RawDump(char *d, int start, int l) {
    if (log == NULL)
	Abort("Tentative d'ecriture dans un log non ouvert\n");

    fwrite(d+start,l,1,log);
}

/*
 * ouvre un log vers le fichier <file> en niveau <lev>.
 * Si <file> est NULL, le log est envoyé vers <stderr>.
 * Le fichier est créé s'il n'existe pas. Dans les cas contraire, les données
 * sont ajoutées à la fin.
 * Pour le niveau de log, voir l'explication dans la fonction LogSetLevel()
 * ci-dessous.
 */
void LogOpen(char *file, unsigned int lev) {
    FILE *oldlog=log;

    if (oldlog == stderr && file != NULL) {
	int oldlevel;
	oldlevel=LogSetLevel(LOG_LALL, LOG_LSTAT);
	Log(LOG_LSTAT,"Redirection des logs vers le fichier <%s>.\n", file);
	LogSetLevel(LOG_LNONE, oldlevel);
    }
    else if (oldlog != NULL && log != stderr) {
	int oldlevel;
	oldlevel=LogSetLevel(LOG_LALL, LOG_LSTAT);
	Log(LOG_LWARN|LOG_LSTAT,"!!!!!!! Attention : reouverture du log dans le fichier <%s>.\n", file);
	LogSetLevel(LOG_LNONE, oldlevel);
	fclose(oldlog);
    }
    if (file == NULL || ((log=fopen(file, "a+")) == NULL))
	log=stderr;  /* si on ne peut pas ouvrir le log, on envoie sur stderr */
	
    LogLevel = lev;

    if (log != NULL && log != stderr) {
	char logstr[32*4+1+1], *p;
	int l,printed=0,oldlevel;
	p=logstr;
	*p++='[';
	for (l=0; l<32; l++) {
	    if (LogLevel & (1<<l)) {
		if (printed++)
		    *p++=',';
		strncpy(p, loglevel_names[l], (logstr+sizeof(logstr)-p));
		p+=strlen(p);
	    }
	}
	*p++=']'; *p++=0;

	oldlevel=LogSetLevel(LOG_LALL, LOG_LSTAT);
	Log(LOG_LSTAT,"########### ouverture du log, pid %d, %s %s ##############\n",
	    getpid(),
	    printed?(printed==1?"niveau":"niveaux"):"AUCUN NIVEAU ACTIF", printed?logstr:"");
	LogSetLevel(LOG_LNONE, oldlevel);
    }
    if (file != NULL && log == stderr)
	Log(LOG_LERR,"!!!!!!! Erreur sur l'ouverture du fichier de log <%s>.\n",file);
}

/* ferme le log */
void LogClose() {
    if (log != NULL && log != stderr)
	fclose(log);
}

/* change le niveau de trace. Effectue un ET logique sur le niveau courant puis un OU logique.
 * Exemples :
 *    LogSetLevel(0, LOGLSOCKET);   => ne logue que les sockets
 *    LogSetLevel(-1, LOGLSOCKET);   => ajoute les logs de sockets aux logs courants
 *    LogSetLevel( ~LOGLSOCKET, 0);  => supprime les logs de sockets des logs courants
 *    LogSetLevel( ~LOGLMEMORY, LOGLFILE);  => supprime les logs memoire et ajoute les logs fichiers
 * On retourne le niveau précédent;
 */
int LogSetLevel(int and, int or) {
    int oldlevel;
    LogLevel = ((oldlevel=LogLevel) & and) | or;
    return oldlevel;
}

/*
 * envoie une trace au format de printf().
 * Ce log n'est affiché que si le niveau passé en paramètre correspond au niveau courant.
 * Si le niveau LOGLPID est actif, le pid est préfixé. Si le niveau LOGLDATE
 * est actif, la date est préfixée.
 * La sortie subit toujours un fflush() avant le retour.
 * on retourne toujours 0, juste pour pouvoir inserer la fonction dans une expression.
 */
int Log(int level, const char *fmt, ...) {
    va_list argp;
    struct timeval tv;
    struct tm *tm;
    char logstr[2048], *p;

    if ((level & (LogLevel | LOG_LFATAL)) == 0)
	return 0;

    *(p=logstr)='\0';
    va_start(argp, fmt);
    if (log) {
	int l,printed=0;
	*p++='[';
	for (l=0; l<32; l++) {
	    if ((level & (LogLevel)) & (1<<l)) {
		if (printed++)
		    *p++=',';
		strncpy(p, loglevel_names[l], (logstr+sizeof(logstr)-p));
		p+=strlen(p);
	    }
	}
	*p++=']';
	gettimeofday(&tv, NULL);
	tm=localtime(&tv.tv_sec);
#ifdef HAVE_SNPRINTF
	snprintf(p, (logstr+sizeof(logstr)-p), " %03d %02d%02d%02d.%03d ", tm->tm_yday, tm->tm_hour, tm->tm_min, tm->tm_sec,
		 ((tv.tv_usec)/1000)%1000);
#else
	sprintf(p, " %03d %02d%02d%02d.%03ld ", tm->tm_yday, tm->tm_hour, tm->tm_min, tm->tm_sec, ((tv.tv_usec)/1000)%1000);
#endif
	p+=strlen(p);
    }
#ifdef HAVE_SNPRINTF
    vsnprintf(p, (logstr+sizeof(logstr)-p), fmt, argp);
#else
    vsprintf(p, fmt, argp);
#endif
    write(fileno(log?log:stderr), logstr, strlen(logstr));
    va_end(argp);
    return 0;
}


/**************************************************************\
 *
 * Outils de gestion de configuration
 *
\**************************************************************/


/* recherche d'une variable par son nom d'identificateur.
   Le pointeur sur la structure est donné en retour. Si le nom n'est pas trouvé,
   NULL est retourné.
*/

static t_varlist *ptrvar(char *name) {
    t_varlist *p;

    p=varlist.next;
    while (p) {
	if (!strcmp(p->id,name))
	    return p;
	p=p->next;
    }
    return NULL;
}

/*
 * Lecture du fichier de configuration dont le nom est passé dans <name>.
 * Le contenu de ce fichier est ajouté à la configuration existante.
 * On retourne le nombre d'erreurs rencontrées.
 */
int ConfRead(char *name) {
    char *p;
    FILE *cnf;
    char ligne[CONF_LINELEN];
    char tmpval[256];
    int err=0;

    if ((cnf=fopen(name, "r"))==NULL) {
	Log(LOG_LWARN, "Impossible d'ouvrir le fichier de configuration <%s>.\n",name);
	return -1;
    }

    while (ConfReadLine(cnf, ligne, CONF_LINELEN)) {
	if ((p=strchr(ligne,'=')) != NULL) {
	    *p++=0;
	    ConfDvlpStr(tmpval,p,sizeof(tmpval));
	    ConfSetStr(ligne,tmpval);
	}
	else {
	    Log(LOG_LERR, "Erreur de syntaxe dans le fichier de configuration <%s>, près de <%s>.\n", name, ligne);
	    err++;
	}
    }
    fclose(cnf);
    return err;
}


/* lecture d'une ligne NON VIDE dans le fichier <file>. Celle-ci est
   stockée dans <ligne>, d'une longueur maximale <len>, et la valeur
   retournée est le pointeur sur cette chaine. A la fin du fichier,
   <ligne> est vidée et la valeur retournée est NULL.
   Les lignes commencant par ';' ou '#' ne sont pas retournées.
*/
char *ConfReadLine(FILE *file, char *ligne, int len) {
	char *ret;

	/* recherche la prochaine ligne non vide */
	do {
	    if ((ret=fgets(ligne, len, file)) == NULL)
		break;
	   /* passe tous les premiers séparateurs */
	    while (/* *ret &&*/ (*ret==' ' || *ret == '\t' || *ret=='\n' || *ret=='\r'))
		ret++;
	    if (*ret=='#' || *ret==';')
		*ret=0;	/* marque la fin immédiate */
	} while (!*ret);

	if (!ret) { /* fin de fichier */
		*ligne='\0';
		return NULL;
	}

	/* suppression des sauts de lignes et retours inutiles en fin de ligne */
	ret=ligne+strlen(ligne)-1;
	while (ret>=ligne && (*ret=='\r' || *ret=='\n'))
	    *ret--=0;

	return ligne;
}


/* affecte la valeur <val> à la variable <id>.
 * Si la variable n'existe pas, elle est créée.
 */
void ConfSetStr(char *id, char *val) {
    t_varlist *p;

    if ((p=ptrvar(id)) == NULL) {  /* nouvelle variable */
	if ((p=malloc(sizeof(t_varlist)))==NULL)
	    Abort("setvar():malloc()");
	strcpy0(p->id,id);
	p->val=NULL;
	/* insertion dans la liste */
	p->next=varlist.next;
	varlist.next=p;
    }
    if (p->val) free(p->val);
    p->val=(char *)malloc(strlen(val)+1);
    strcpy(p->val,val);
}

/* affecte la valeur entiere <val> à la variable <id>.
 * Si la variable n'existe pas, elle est créée.
 */
void ConfSetInt(char *id, long int val) {
    char entier[32];
    sprintf(entier,"%ld",val);
    ConfSetStr(id,entier);
}

/* retourne la valeur de la variable ou de la var d'environnement ou "" */
char *ConfGetStr(char *id) {
    t_varlist *p;
    char *v;

    /*printf("demande de la variable <%s> : ",id);*/
    if ((p=ptrvar(id)) != NULL)
	v=p->val;
    else if (!(v=getenv(id))) {
	v="";
    }
    return v;
}

/* retourne la valeur entiere de la variable (interne ou env) ou 0 */
long int ConfGetInt(char *id) {
#ifdef CONFPARSE
    return CONFPARSE(ConfGetStr(id));
#else
    return atol(ConfGetStr(id));
#endif
}

/* développe une chaine de caractères contenant des variables sous la forme $(var) en une
   chaine dans laquelle les variables sont remplacées par leur valeur. La chaine de résultat
   fera au plus <lmax> caractères.
*/

char *ConfDvlpStr(char *out, char *in, int lmax) {
    char *res;
    char *p1, *p2;
    char var[CONF_LINELEN];
    char tmpvar[CONF_LINELEN];
    int pardepth;

    res=out;
    while (lmax && *in) {
	while (lmax && *in && (*in!='$'))
	    *out++=*in++;
	if (*in=='$') {
	    switch (*++in) {
	    case '$' :  /* certains caractères sont réservés. C'est donc un code escape */
		if (lmax) {
		    *out++=*in++;
		    lmax--;
		}
		break;
	    case '(' :  /* parenthese : c'est une variable */
		p1=++in;
		pardepth=1;
		while (*in && ((*in != ')') || (pardepth>1)) && (in-p1+1<sizeof(var))) {
		  if (*in == ')')
		    pardepth--;
		  else if (*in == '(')
		    pardepth++;
		  in++;
		}
		strncpy0(var,p1,in-p1);		/* verifier la longueur .... */
		/*var[in-p1]=0;*/
		p2=ConfGetStr(ConfDvlpStr(tmpvar,var,sizeof(tmpvar)));  /* le nom de la variable peut etre variable lui-meme */
		if (strlen(p2)>lmax) {
		    strncpy(out,p2,lmax);
		    *(out+=lmax)=0;
		}
		else {
		    strcpy(out,p2);
		    *(out+=strlen(p2))=0;
		}
		if (*in) in++;	/* passe la parenthese fermante */
		break;
	    case '{' : /* accolade : commande */
		p1=++in;
		while (*in && (*in!='}') && (in-p1+1<sizeof(var)))
		    in++;
		strncpy0(var,p1,in-p1);		/* verifier la longueur .... */
		/*var[in-p1]=0;*/
		/* mettre dans p2 le resultat de la commande nommée par <var> */
		p2=ConfGetStr(var);
		if (strlen(p2)>lmax) {
		    strncpy(out,p2,lmax);
		    *(out+=lmax)=0;
		}
		else {
		    strcpy(out,p2);
		    *(out+=strlen(p2))=0;
		}
		if (*in) in++;	/* passe l'accolade fermante */
		break;
	    default  : /* autre: non traite */
		if (lmax) { *out++='$'; lmax--; }
		if (lmax) { *out++=*in; lmax--; }
	    }
	}
    }
    *out=0;
    return res;
}

 
/********************************************************************
 * fonctions d'analyse et de résolution d'expressions arithmétiques *
 ********************************************************************/


#define PARSESTR ConfGetStr

#define NEXTCHAR(a)	((*a)++)
#define PREVCHAR(a)	((*a)--)

#ifdef PARSESTR
extern char *PARSESTR(char *);
#endif

enum {
    OP_SL=1, OP_LE, OP_LT,
    OP_SR, OP_GE, OP_GT,
    OP_EQ, OP_LG,
    OP_NE, OP_NL, OP_NG,
    OP_QN, OP_CO,
    OP_BO, OP_LO,
    OP_BA, OP_LA,
    OP_BX, OP_LX,
    OP_PW, OP_MU, OP_DI,
    OP_MI, OP_PL,
    OP_NO, OP_CP
};

static int expression(char **arg);

#ifndef HAVE_INTPOW
static int intpow(a,b) {
    int c=1;
    if (b>0) {
	while (b--)
	    c*=a;
	return c;
    }
    else if (b==0)
	return 1;
    else
	return a==1;  /* (1/a^n = 1) if (a==1), else 0 */
}
#endif

static int readchar(char **arg, int c) {
    if (c==**arg) {
	NEXTCHAR(arg);
	return 1;
    }
    else
	return 0;
}

static int readalnum(char **arg) {
    if (isalnum(**arg) || (**arg=='_') || (**arg=='.')) {
	NEXTCHAR(arg);
	return 1;
    }
    else
	return 0;
}

static int readnum(char **arg) {
    if (isdigit(**arg)) {
	NEXTCHAR(arg);
	return 1;
    }
    else
	return 0;
}

static int readspace(char **arg) {
    if (isspace(**arg)) {
	NEXTCHAR(arg);
	return 1;
    }
    else
	return 0;
}

static int readany(char **arg) {
    if (**arg==0)
	return 0;
    NEXTCHAR(arg);
    return 1;
}

static int readeol(char **arg) {
    if (**arg=='\n') {
	NEXTCHAR(arg);
	return 1;
    }
    else
	return 0;
}

/* skips all consecutive comments and spaces. returns no-zero if at least one comment was read */
static int skipcomments(char **arg) {
    int comm=0;
    while (readspace(arg));
    while (**arg=='#') {
	while (!readeol(arg))
	    readany(arg);
	while (readspace(arg));
	comm=1;
    }
    return comm;
}

/* returns the work read */
static char *readword(char **arg) {
    static char token[100];
    int  tklen=0;

    skipcomments(arg);
    while (readalnum(arg) && (tklen<sizeof(token)-1))
	token[tklen++]=*(*arg-1);
    token[tklen]=0;
    return token;
}

static int fetch(char **arg, int c) {
    skipcomments(arg);
    return readchar(arg,c);
}

/* returns 1 if the token can be fetched, otherwise 0. */
static int fetchstr(char **arg, char *tok) {
    char *newarg=*arg;
    while (*tok && *newarg==*tok) {
	newarg++;
	tok++;
    }
    /* not everything matched */
    if (*tok)
	return 0;
    /* everything matched so let's update arg */
    *arg=newarg;
    return 1;
}


/* returns 1 if the operator can be fetched, otherwise 0. */
static int fetchop(char **arg, int op) {
    char *oldarg=*arg;
    int ret;
    /* it's important to match first the longest operators to avoid prefixes collision */

    if (fetch(arg,'+'))
	ret = op==OP_PL;
    else if (fetch(arg,'/'))
	ret = op==OP_DI;
    else if (fetch(arg,'-'))
	ret = op==OP_MI;
    else if (fetch(arg,'~'))
	ret = op==OP_CP;
    else if (fetch(arg,'?'))
	ret = op==OP_QN;
    else if (fetch(arg,':'))
	ret = op==OP_CO;
    else if (fetch(arg,'<')) { /* <<, <=, <>, < */
	if (fetch(arg,'='))
	    ret = op==OP_LE;
	else if (fetch(arg,'<'))
	    ret = op==OP_SL;
	else if (fetch(arg,'>'))
	    ret = op==OP_LG;
	else
	    ret = op==OP_LT;
    }
    else if (fetch(arg,'>')) {  /* >>, >=, > */
	if (fetch(arg,'='))
	    ret = op==OP_GE;
	else if (fetch(arg,'>'))
	    ret = op==OP_SR;
	else
	    ret = op==OP_GT;
    }
    else if (fetch(arg,'!')) { /* !<, !>, !=, ! */
	if (fetch(arg,'<'))
	    ret = op==OP_NL;
	else if (fetch(arg,'>'))
	    ret = op==OP_NG;
	else if (fetch(arg,'='))
	    ret = op==OP_NE;
	else
	    ret = op==OP_NO;
    }
    else if (fetch(arg,'|')) {  /* ||, | */
	if (fetch(arg,'|'))
	    ret = op==OP_BO;
	else
	    ret = op==OP_LO;
    }
    else if (fetch(arg,'^')) {  /* ^^,^ */
	if (fetch(arg,'^'))
	    ret = op==OP_BX;
	else
	    ret = op==OP_LX;
    }
    else if (fetch(arg,'&')) {  /* &&, & */
	if (fetch(arg,'&'))
	    ret = op==OP_BA;
	else
	    ret = op==OP_LA;
    }
    else if (fetch(arg,'*')) {  /* **, * */
	if (fetch(arg,'*'))
	    ret = op==OP_PW;
	else
	    ret = op==OP_MU;
    }
    else if (fetchstr(arg,"=="))
	ret = op==OP_EQ;
    else
	ret = 0;

    if (!ret)
	*arg=oldarg; /* operator not matched */
    return ret;
}

static int terme(char **arg) {
    int a;
    if (fetchop(arg, OP_MI))
	a=-terme(arg);
    else if (fetchop(arg, OP_CP))
	a=~terme(arg);
    else {
	if (fetch(arg, '(')) {
	    a=expression(arg);
	    fetch(arg, ')');
	}
	else if (isdigit(**arg))
	    a=atol(readword(arg));
	else
#ifdef PARSESTR
	    a=ConfParse(PARSESTR(readword(arg)));
#else
	    a=0;
#endif	    

	if (fetchop(arg, OP_PW)) /* exponentiation */
	    a=intpow(a,terme(arg));
    }
    return a;
}

static int produit(char **arg) {
    int a,b;
    a=terme(arg);
    do {
	if (fetchop(arg, OP_MU)) {
	    a*=terme(arg);
	}
	else if (fetchop(arg, OP_DI)) {
	    b=terme(arg);
	    if (b)
		a/=b;
	    else
		a=0;  /* dividing anything by 0 will always return 0 */
	}
	else  
	    break;
    } while (1);
    return a;
}


static int shifter(char **arg) {
    int a;
    a=produit(arg);
    do {
	if (fetchop(arg, OP_SL))
	    a<<=produit(arg);
	else if (fetchop(arg, OP_SR))
	    a>>=produit(arg);
	else  
	    break;
    } while (1);
    return a;
}

static int log_conj(char **arg) {
    int a;
    a=shifter(arg);
    while (fetchop(arg, OP_LA))
	a&=shifter(arg);
    return a;
}

static int log_disj(char **arg) {
    int a;
    a=log_conj(arg);
    do {
	if (fetchop(arg, OP_LO))
	    a|=log_conj(arg);
	else if (fetchop(arg, OP_LX))
	    a^=log_conj(arg);
	else  
	    break;
    } while (1);
    return a;
}

static int somme(char **arg) {
    int a;
    a=log_disj(arg);
    do {
	if (fetchop(arg, OP_PL))
	    a+=log_disj(arg);
	else if (fetchop(arg, OP_MI))
	    a-=log_disj(arg);
	else 
	    break;
    } while (1);   
    return a;
}

static int comp(char **arg) {
    int a;
    a=somme(arg);
    do {
	if (fetchop(arg, OP_EQ))
	    a=(a==somme(arg));
	else if (fetchop(arg, OP_LE) || fetchop(arg, OP_NG))
	    a=(a<=somme(arg));
	else if (fetchop(arg, OP_GE) || fetchop(arg, OP_NL))
	    a=(a>=somme(arg));
	else if (fetchop(arg, OP_LG) || fetchop(arg, OP_NE))
	    a=(a!=somme(arg));
	else if (fetchop(arg, OP_LT))
	    a=(a<somme(arg));
	else if (fetchop(arg, OP_GT))
	    a=(a>somme(arg));
	else  
	    break;
    } while (1);
    return a;
}

static int boolean(char **arg) {
    if (fetchop(arg, OP_NO))
	return !boolean(arg);
    else
	return comp(arg);
}


static int bool_conj(char **arg) {
    int a;
    a=boolean(arg);
    while (fetchop(arg, OP_BA))
	    a = a && boolean(arg);
    return a;
}

static int bool_disj(char **arg) {
    int a;
    a=bool_conj(arg);
    do {
	if (fetchop(arg, OP_BO))
	    a = a || bool_conj(arg);
	//	else if (fetchop(arg, OP_BX))
	//	    a = a ^^ bool_conj(arg);
	else  
	    break;
    } while (1);
    return a;
}


static int expression(char **arg) {
    int cond, exp1, exp2;
    cond=bool_disj(arg);
    if (fetchop(arg, OP_QN)) {
	if (fetchop(arg, OP_CO)) {  /* missing expr means keep cond if true */
	    exp1=cond;
	    exp2=expression(arg);
	}
	else {
	    exp1=expression(arg);
	    if (fetchop(arg, OP_CO))
		exp2=expression(arg);
	    else
		exp2=0;	/* missing colon means 0 */
	}
	return cond ? exp1 : exp2;
    }
    else
	return cond;
}

/**** la seule fonction exportée ****/
long int ConfParse(char *arg) {
    return expression(&arg);
}
#undef PARSESTR
#undef NEXTCHAR
#undef PREVCHAR

 
/**********************************************************
 * fonctions de traitements sur les chaines de caractères *
 **********************************************************/

/* strncpy avec 0 final. src peut etre NULL.
   Attention: <lmax> caracteres sont copiés, et un zéro terminal est
   ajouté APRES. <dest> doit donc pouvoir accepter lmax+1 caractères.
*/

char *strncpy0(char *dest, const char *src, int lmax) {
	register char *p;
	p=dest;
	if (src)
		while (lmax-- && (*p++=*src++));
	*p=0;
	return dest;
}


/* retourne un pointeur sur la chaine S tronquée à l caractères maximum */
char *strcut(char *s, int l) {
	char *p=s;

	while (*p && l) {
		p++; l--;
	}
	*p=0;
	return s;
}

static char base64tab[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int char64to6bits(char c) {
    return (c=='/'?63:
	    c=='+'?62:
	    (c>='0' && c<='9')?(c-'0'+52):
	    (c>='a' && c<='z')?(c-'a'+26):
	    (c>='A' && c<='Z')?(c-'A'+0):
	    0);
}

/* WARNING !!! no boundary checking is done. The caller *MUST* have enough
 * space to store the result : outsize = (insize+3)*4/3
 */
char *asciitobase64(char *ascii, char *base64) {
    char *dest=base64;
    while (ascii[0] && ascii[1] && ascii[2]) {
	*dest++=base64tab[(ascii[0]>>2) & 0x3f];
	*dest++=base64tab[(ascii[0] & 0x03)<<4 | ((ascii[1] >> 4) & 0x0f)];
	*dest++=base64tab[(ascii[1] & 0x0f)<<2 | ((ascii[2] >> 6) & 0x03)];
	*dest++=base64tab[(ascii[2] & 0x3f)];
	ascii+=3;
    }
    /* only 0, 1 or 2 bytes left */
    if (*ascii) { /* 1 or 2 bytes left */
	*dest++=base64tab[(ascii[0]>>2) & 0x3f];
	if (!ascii[1])  /* 1 byte left */
	    *dest++=base64tab[(ascii[0] & 0x03)<<4];
	else {  /* 2 bytes left */
	    *dest++=base64tab[(ascii[0] & 0x03)<<4 | ((ascii[1] >> 4) & 0x0f)];
	    *dest++=base64tab[(ascii[1] & 0x0f)<<2];
	}
	*dest++='=';
    }
    *dest++=0;
    return base64;
}

/* WARNING !!! no boundary checking is done. The caller *MUST* have enough
 * space to store the result : outsize = (insize+2)/3*4 + 1 (zero)
 */
char *asciintobase64(char *ascii, int len, char *base64) {
    char *dest=base64;

    while (len>2) {
	*dest++=base64tab[((unsigned)ascii[0]>>2) & 0x3f];
	*dest++=base64tab[((unsigned)ascii[0] & 0x03)<<4 | (((unsigned)ascii[1] >> 4) & 0x0f)];
	*dest++=base64tab[((unsigned)ascii[1] & 0x0f)<<2 | (((unsigned)ascii[2] >> 6) & 0x03)];
	*dest++=base64tab[((unsigned)ascii[2] & 0x3f)];
	ascii+=3;
	len-=3;
    }

    /* only 0, 1 or 2 bytes left */
    if (len>0) { /* 1 or 2 bytes left */
	*dest++=base64tab[((unsigned)ascii[0]>>2) & 0x3f];
	if (len==1)  /* 1 byte left */
	    *dest++=base64tab[((unsigned)ascii[0] & 0x03)<<4];
	else {  /* 2 bytes left */
	    *dest++=base64tab[((unsigned)ascii[0] & 0x03)<<4 | (((unsigned)ascii[1] >> 4) & 0x0f)];
	    *dest++=base64tab[((unsigned)ascii[1] & 0x0f)<<2];
	}
	*dest++='=';
    }
    *dest++=0;
    return base64;
}

/* WARNING !!! no boundary checking is done. The caller *MUST* have enough
 * space to store the result : outsize = insize*3/4
 * an empty string correctly returns an empty string.
 */
char *base64toascii(char *ascii, char *base64) {
    int i,j;
    char *p=ascii;
    
    do {
	for (i=4,j=0;i>0;i--) {
	    if (*base64=='=' || *base64==0) /* fin */ {
		j<<=(6*i);
		break;
	    }
	    j=(j<<6)|char64to6bits(*base64++);
	}
	if (i!=4) {  /* au moins un caractere lu ? */
	    *p++=(j>>16)&255;
	    if (i!=2) { /* le deuxieme octet est renseigne */
		*p++=(j>>8)&255;
		if (i!=1) { /* le troisieme octet est renseigne */
		    *p++=(j)&255;
		}
	    }
	}
    } while (!i); /* tant que i est nul, on a lu 4 caracteres donc 3 octets */
    *p++=0; /* fin de chaine forcee */
    return ascii;
}

/* reads a string of the form "var=val; var=val; ..." and extracts the desired
   "var" specified in <token>. The corresponding "val" is copied into <to> for
   at most <len> chars, including the ending 0. The result is <to>, or NULL if
   token not found, and <to> is set to an empty string.
*/
char *extracttoken(const char *from, const char *token, char *to, int len) {
    char *s;
    s=(char *)from;
    *to=0;
    while (1) {
	if ((s=strstr(s, token)) == NULL)
	    return NULL;
	s+=strlen(token);
	if (*s != '=') {
	    while(*s && !isspace(*s) && (*s!=';'))
		s++;
	    while(*s && (isspace(*s) || (*s!=';')))
		s++;
	}
	else {
	    char *t=to;
	    s++;
	    while(*s && !isspace(*s) && (*s!=';') && len-->1)
		*t++=*s++;
	    *t++=0;
	    return to;
	}
    }
}

/*
 * calculations on timeval structs. Integers are milliseconds.
 */

/* sets <tv> to the current time */
struct timeval *tv_now(struct timeval *tv) {
    if (tv)
	gettimeofday(tv, NULL);
    return tv;
}

/* returns a statically allocated struct timeval initialized with <ms> ms */
struct timeval *tv_ms(int ms) {
    static struct timeval tv;
    tv.tv_usec = (ms%1000)*1000;
    tv.tv_sec  = (ms/1000);
    return &tv;
}

/* adds <ms> ms to tv and returns a pointer to the newly filled struct */
struct timeval *tv_delay(struct timeval *tv, int ms) {
  if (!tv)
    return NULL;
  tv->tv_usec += (ms%1000)*1000;
  tv->tv_sec  += (ms/1000);
  if (tv->tv_usec >= 1000000) {
    tv->tv_usec -= 1000000;
    tv->tv_sec++;
  }
  return tv;
}

/* adds <ms> ms to <from>, set the result to <tv> and returns a pointer <tv> */
struct timeval *tv_delayfrom(struct timeval *tv, struct timeval *from, int ms) {
  if (!tv || !from)
    return NULL;
  tv->tv_usec = from->tv_usec + (ms%1000)*1000;
  tv->tv_sec  = from->tv_sec  + (ms/1000);
  while (tv->tv_usec >= 1000000) {
    tv->tv_usec -= 1000000;
    tv->tv_sec++;
  }
  return tv;
}

/* copies <from> to <tv> and returns a pointer <tv> */
struct timeval *tv_set(struct timeval *tv, struct timeval *from) {
  if (!tv || !from)
    return NULL;
  tv->tv_usec = from->tv_usec;
  tv->tv_sec  = from->tv_sec;
  return tv;
}

/* sets tv to now + <ms> ms, and returns a pointer to the newly filled struct */
struct timeval *tv_wait(struct timeval *tv, int ms) {
  if (!tv)
    return NULL;
  gettimeofday(tv, NULL);
  tv_delay(tv, ms);
  return tv;
}

/* compares <tv1> and <tv2> : returns 0 if equal, -1 if tv1 < tv2, 1 if tv1 > tv2 */
int tv_cmp(struct timeval *tv1, struct timeval *tv2) {
  if (tv1->tv_sec > tv2->tv_sec)
    return 1;
  else if (tv1->tv_sec < tv2->tv_sec)
    return -1;
  else if (tv1->tv_usec > tv2->tv_usec)
    return 1;
  else if (tv1->tv_usec < tv2->tv_usec)
    return -1;
  else return 0;
}

/* returns 1 if <tv> is in the past (means tv<=gettimeofday()) or 0 if not) */
int tv_past(struct timeval *tv) {
  struct timeval now;
  gettimeofday(&now, NULL);
  if ( tv_cmp(tv, &now) <= 0 )
    return 1;
  else
    return 0;
}

/* returns the absolute difference, in ms, between tv1 and tv2 */
unsigned long tv_delta(struct timeval *tv1, struct timeval *tv2) {
  int cmp;
  unsigned long ret;
  

  cmp=tv_cmp(tv1, tv2);
  if (!cmp)
    return 0; /* same dates, null diff */
  else if (cmp<0) {
    struct timeval *tmp=tv1;
    tv1=tv2;
    tv2=tmp;
  }
  ret=(tv1->tv_sec - tv2->tv_sec)*1000;
  if (tv1->tv_usec > tv2->tv_usec)
    ret+=(tv1->tv_usec - tv2->tv_usec)/1000;
  else
    ret-=(tv2->tv_usec - tv1->tv_usec)/1000;
  return (unsigned long) ret;
}
