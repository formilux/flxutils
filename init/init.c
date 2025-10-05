/*
  preinit - new try on 2002/04/20 - Willy Tarreau <willy AT meta-x.org>

  usage : /sbin/preinit [ \< config_file ] [ { init args | "rebuild" } ]

  Note : the "\< config_file" is to be used within configuration files :
  #!/sbin/preinit <
  ....
  Thus, when you pass "init=/.preinit", the kernel executes :
  /sbin/preinit < /.preinit

  The '<' character has been chosen for its rareness.

  the "rebuild" argument make the tool only rebuild a complete /dev
  tree from the informations contained in the .preinit file, and then
  exit. It does this even if the pid is not 1, but doesn't execute nor 
  mount anything. Only mkdir, links, blocks, chars and fifo devices are
  created. Very useful before a lilo :

  # chroot /mnt/disk /.preinit rebuild
  # lilo -r /mnt/disk
  # umount /mnt/disk/dev

  If /dev/console is found under /dev, it will not be rebuilt.

  Please read the README file for detailed information.

  The choice of the init program is quite complex, because we want to avoid
  stupid loops and buggy behaviours while conservating quite a parametrable
  init. Basically, we'll use environment variables that we will destroy once
  read, to avoid getting them again if we loop on ourselves (imagine an
  'exec /sbin/init" from a shell cmd line, which will blindly re-exec a shell).
  So there are two distinct cases (init and linuxrc) :

  1) linuxrc
  We want to be able to either return or transfer execution to the real init.

  - if we find "init2=xxxx" in the env, we memorize it and move it away from the env.
  - if "in xxxx" has been specified on a keyboard input from an "rd"
  statement, we unconditionnaly do an execve("xxx", "xxx", ##no args##, ##envp##).
  - if the env contained "init2=xxxx", we do an execve("xxxx", "xxxx", ##no args##, ##envp##).
  we don't pass the args because it is a rescue init, such as a shell, and
  we don't want it to fail on "bash: auto: command not found" or similar.
  - if the conf contained "in xxxx", we do an execve("xxxx", "xxxx", argv[], ##envp##)
  because we want it to know about the original args. Eg: init=/linuxrc single
  - if the conf didn't contain "in xxxx", we unmount all what we can and
  return. The kernel will be able to switch over to the next init stage.

  2) init, or anything else (telinit, ...)
  We want to transfer execution to the real init.

  - if we find "INIT=xxxx" in the env, we memorize it and move it away from the env.
  - if "in xxxx" has been specified on a keyboard input from an "rd"
  statement, we unconditionnaly do an execve("xxx", "xxx", ##no args##, ##envp##).
  - if the env contained "INIT=xxxx", we do an execve("xxxx", "xxxx", ##no args##, ##envp##).
  we don't pass the args because it is a rescue init, such as a shell, and
  we don't want it to fail on "bash: auto: command not found" or similar.
  - if the conf contained "in xxxx", we do an execve("xxxx", "xxxx", argv[], ##envp##)
  because we want it to know about the original args. Eg: init=/.preinit single
  - if the conf didn't contain "in xxxx", we transfer execution to "/sbin/init-sysv".

  Note: basically, each time an environment variable is read, it must be killed afterwards.
  Eg: init2=, INIT=, ...
*/

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

#ifndef NOLIBC
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysmacros.h>
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/loop.h>
#include <sched.h>
#include <errno.h>
#include <dirent.h>
#include <syscall.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#endif

#include <linux/random.h>

#ifndef NOLIBC
# define my_syscall0 syscall
# define my_syscall1 syscall
# define my_syscall2 syscall
# define my_syscall3 syscall
# define my_syscall4 syscall
# define my_syscall5 syscall
#endif

#if defined(__NR_kexec_file_load)
#include <linux/kexec.h>
# ifndef LINUX_REBOOT_CMD_KEXEC
#  define LINUX_REBOOT_CMD_KEXEC 0x45584543
# endif
#endif

#if defined(__NR_finit_module) || defined(__NR_delete_module)
// Note: file only present starting with kernel 3.8
//# include <linux/module.h>
# if !defined(MODULE_INIT_IGNORE_MODVERSIONS)
#  define MODULE_INIT_IGNORE_MODVERSIONS 1
# endif
# if !defined(MODULE_INIT_COMPRESSED_FILE)
#  define MODULE_INIT_COMPRESSED_FILE 4
# endif
#endif

/*
 * compatibility defines in case they are missing
 */

#ifndef MNT_DETACH
#define MNT_DETACH      2
#endif

#ifndef MS_MOVE
#define MS_MOVE         8192
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE     0
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 256
#endif

#ifndef NOLIBC
struct linux_dirent64 {
	uint64_t       d_ino;
	int64_t        d_off;
	unsigned short d_reclen;
	unsigned char  d_type;
	char           d_name[];
};

#if (!defined(__GLIBC__) || __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 30))
static long getdents64(int fd, struct linux_dirent64 *dirp, unsigned long count)
{
	return my_syscall3(SYS_getdents64, fd, dirp, count);
}
#endif

int pivot_root(const char *new_root, const char *put_old);
#endif

#undef alloca
#define alloca(size)   __builtin_alloca(size)

/*
 * configuration settings and convenience defines
 */

/* used by naming rules */
#define MAX_FIELDS	8
#define MAX_DEVNAME_LEN	64
#define MAX_CFG_SIZE	16384
#define	MAX_CFG_ARGS	64
#define MAX_CMDLINE_LEN 512
#define MAX_BRACE_LEVEL	10
#define MAX_MNT_SIZE	1024
#define MAX_PART_SIZE	16384

/* well-known UID/GID */
#define UID_ROOT        0
#define GID_ROOT        0
#define GID_TTY         5
#define GID_KMEM        9

/* the two input modes */
#define INPUT_FILE 0
#define INPUT_KBD  1

/*
 * various types used later
 */

/* descriptor for a range of device minors */
struct dev_varstr {
	union {
		struct {
			char *set;
			char *ptr;
			char value; /* value to be printed */
			uint8_t index; /* index in the set */
		} chr;
		struct {
			uint8_t low;
			uint8_t high;
			uint8_t value;
		} num;
	} u;
	char type; /* 'c', 'h', 'i', 'I', or 0 */
	uint8_t scale;
};

/* configuration language tokens */
struct token {
	char lcmd[2];   /* long form */
	char scmd;      /* short form */
	char minargs;   /* min #args */
} __attribute__((__aligned__(1)));

/* device node descriptor */
struct dev_node {
	char   name[8];
	mode_t mode;    /* mode + S_IFCHR, S_IFBLK, S_IFIFO */
	short  gid;
	char   major, minor;
} __attribute__((__aligned__(1)));

/* RNDADDENTROPY ioctl */
struct rnd {
	int entropy_count;
	int buf_size;
	char buf[1024];
};

/* flags for blkdev->flags below */
#define BLKD_F_IGNORE         0x00000001
#define BLKD_F_DETECTED       0x00000002 // partition was already detected
#define BLKD_F_FLASH          0x00000004 // partition could be a /flash
#define BLKD_F_IMAGE1         0x00000008 // partition matches the first of 2 images
#define BLKD_F_IMAGE2         0x00000010 // partition matches the second of 2 images

struct blkdev {
	char *name;
	unsigned int major;
	unsigned int minor;
	unsigned long long size; // in kB
	struct blkdev *next;
	struct blkdev *part;
	unsigned int flags; // BLKD_F_xxx
	unsigned int pref;  // 0+: position in preference list
	char type[16]; // FS type
};

/* possible states for variable parsing */
enum {
	VAR_NONE = 0,
	VAR_DOLLAR,
	VAR_OBRACE,
	VAR_DEFAULT,
	VAR_CBRACE,
};

/* all supported configuration language tokens */
enum {
	TOK_BI,                /* bi : bind a directory */
	TOK_BL,                /* bl : make block devices */
	TOK_BR,                /* br : branch (=execute without forking) */
	TOK_CA,                /* ca : cat file */
	TOK_CD,                /* cd : chdir */
	TOK_CH,                /* ch : make char devices */
	TOK_CP,                /* cp : copy file */
	TOK_CR,                /* cr : chroot (without chdir) */
	TOK_EC,                /* ec : echo */
	TOK_EN,                /* en : env */
	TOK_EQ,                /* eq : compare two strings */
	TOK_EX,                /* ex : execute */
	TOK_FI,                /* fi : make a fifo */
	TOK_FP,                /* fp : find partitions */
	TOK_HA,                /* ha : halt */
	TOK_HE,                /* he : help */
	TOK_IN,                /* in : set init program */
	TOK_LN,                /* ln : make a symlink */
	TOK_LO,                /* lo : losetup */
	TOK_LP,                /* lp : list partitions */
	TOK_LS,                /* ls : list files in DIR $1 */
#if defined(__NR_kexec_file_load)
	TOK_KX,                /* kx : kexec */
#endif
	TOK_MA,                /* ma : set umask */
	TOK_MD,                /* md : mkdir */
	TOK_ML,                /* ml : modlist */
#ifdef __NR_finit_module
	TOK_MP,                /* mp : modprobe */
#endif
#ifdef __NR_delete_module
	TOK_MR,                /* mr : rmmod */
#endif
	TOK_MT,                /* mt : mount */
	TOK_MV,                /* mv : move a filesystem */
	TOK_PO,                /* po : power off */
	TOK_PR,                /* pr : pivot root */
	TOK_RB,                /* rb : reboot */
	TOK_RD,                /* rd : read a command from the console */
	TOK_RE,                /* re : remount */
	TOK_RF,                /* rf : random feed */
	TOK_RM,                /* rm : remove files */
	TOK_RX,                /* rx : execute under chroot */
	TOK_SE,                /* se : setenv */
	TOK_SL,                /* sl : sleep */
	TOK_SP,                /* sp : suspend */
	TOK_ST,                /* st : stat file existence */
	TOK_SW,                /* sw : switch root = chdir + chroot . + reopen console */
	TOK_TA,                /* ta : tar "t"/"x"/"xv" archive $2 to dir #3 */
	TOK_TD,                /* td : test /dev for devtmpfs support */
	TOK_TE,                /* te : test an environment variable */
	TOK_TN,                /* tn : test that argument is non-empty */
	TOK_UM,                /* um : umount a filesystem */
	TOK_WK,                /* wk : wait key */
	/* better add new commands above */
	TOK_OB,	               /* {  : begin a command block */
	TOK_CB,	               /* }  : end a command block */
	TOK_DOT,               /* .  : end of config */
	TOK_UNK,               /* unknown command */
	TOK_EOF,               /* end of file */
	/* below are conditions which can be ORed with a token */
	TOK_COND_NEG = 0x100,  /* negate the result before evaluation */
	TOK_COND_OR  = 0x200,  /* conditionnal OR */
	TOK_COND_AND = 0x400,  /* conditionnal AND */
	TOK_COND     = 0x700,  /* any condition */
};

/*
 * constant map arrays used later.
 */

/* this contains all two-chars command, 1-char commands, followed by a token
 * number.
 */
static const struct token tokens[] = {
	/* TOK_BI */ { "bi", 'K', 2, },
	/* TOK_BL */ { "bl", 'B', 6, },
	/* TOK_BR */ { "br",   0, 1, },
	/* TOK_CA */ { "ca",   0, 1, },
	/* TOK_CD */ { "cd",   0, 1, },
	/* TOK_CH */ { "ch", 'C', 6, },
	/* TOK_CP */ { "cp",   0, 2, },
	/* TOK_CR */ { "cr",   0, 1, },
	/* TOK_EC */ { "ec",   0, 0, },
	/* TOK_EN */ { "en",   0, 0, },
	/* TOK_EQ */ { "eq",   0, 2, },
	/* TOK_EX */ { "ex", 'E', 1, },
	/* TOK_FI */ { "fi", 'F', 4, },
	/* TOK_FP */ { "fp",   0, 1, },
	/* TOK_HA */ { "ha",   0, 0, },
	/* TOK_HE */ { "he", '?', 0, },
	/* TOK_IN */ { "in", 'I', 1, },
	/* TOK_LN */ { "ln", 'L', 2, },
	/* TOK_LO */ { "lo", 'l', 2, },
	/* TOK_LP */ { "lp",   0, 0, },
	/* TOK_LS */ { "ls",   0, 0, },
#if defined(__NR_kexec_file_load)
	/* TOK_KX */ { "kx",   0, 2, },
#endif
	/* TOK_MA */ { "ma", 'U', 1, },
	/* TOK_MD */ { "md", 'D', 1, },
	/* TOK_ML */ { "ml",   0, 0, },
#ifdef __NR_finit_module
	/* TOK_MP */ { "mp",   0, 1, },
#endif
#ifdef __NR_delete_module
	/* TOK_MR */ { "mr",   0, 1, },
#endif
	/* TOK_MT */ { "mt", 'M', 3, },
	/* TOK_MV */ { "mv", 'K', 2, },
	/* TOK_PO */ { "po",   0, 0, },
	/* TOK_PR */ { "pr", 'P', 2, },
	/* TOK_RB */ { "rb",   0, 0, },
	/* TOK_RD */ { "rd",   0, 0, },
	/* TOK_RE */ { "re",   0, 3, },
	/* TOK_RF */ { "rf",   0, 0, },
	/* TOK_RM */ { "rm",   0, 1, },
	/* TOK_RX */ { "rx", 'R', 2, },
	/* TOK_SE */ { "se", 'S', 1, },
	/* TOK_SL */ { "sl",   0, 1, },
	/* TOK_SP */ { "sp",   0, 0, },
	/* TOK_ST */ { "st",   0, 1, },
	/* TOK_SW */ { "sw",   0, 1, },
	/* TOK_TA */ { "ta",   0, 3, },
	/* TOK_TD */ { "td",   0, 0, },
	/* TOK_TE */ { "te",   0, 1, },
	/* TOK_TN */ { "tn",   0, 1, },
	/* TOK_UM */ { "um", 'O', 1, },
	/* TOK_WK */ { "wk",   0, 2, },
	/**** end of commands dumped by the help command ****/

	/* TOK_OB */ { "{",  '{', 0, },
	/* TOK_CB */ { "}",  '}', 0, },

	/**** all supported commands must be before this one ****/
	/* TOK_DOT*/ { ".",  '.', 0, }
};

/* One string per token, each terminated by a zero */
static const char tokens_help[] =
	/* TOK_BI */ "BInd old_dir new_dir : mount --bind\0"
	/* TOK_BL */ "BLockdev mode uid gid major minor rule\0"
	/* TOK_BR */ "BRanch cmd [args] : execve\0"
	/* TOK_CA */ "CAt file\0"
	/* TOK_CD */ "ChDir dir\0"
	/* TOK_CH */ "CHardev mode uid gid major minor rule\0"
	/* TOK_CP */ "CP src dst\0"
	/* TOK_CR */ "ChRoot dir\0"
	/* TOK_EC */ "ECho string\0"
	/* TOK_EN */ "ENv : show environment\0"
	/* TOK_EQ */ "EQ str1 str2 : compare strings\0"
	/* TOK_EX */ "EXec cmd [args] : fork+execve\0"
	/* TOK_FI */ "FIfo mode uid gid name\0"
	/* TOK_FP */ "FindPart ${img} [fstype...]: (sets {chosen,second}_{part,type})\0"
	/* TOK_HA */ "HAlt\0"
	/* TOK_HE */ "HElp [cmd]\0"
	/* TOK_IN */ "INit path\0"
	/* TOK_LN */ "LN target link : symlink\0"
	/* TOK_LO */ "LOsetup /dev/loopX file\0"
	/* TOK_LP */ "ListPartitions [-r] : -r for rescan\0"
	/* TOK_LS */ "LS [-e|-l] dir\0"
#if defined(__NR_kexec_file_load)
	/* TOK_KX */ "KeXec kernel initrd|- [cmdline*|-]\0"
#endif
	/* TOK_MA */ "uMAsk umask\0"
	/* TOK_MD */ "MkDir path [mode]\0"
	/* TOK_ML */ "ModList: list loaded kernel modules\0"
#ifdef __NR_finit_module
	/* TOK_MP */ "ModProbe [-f] path [\"args...\"]\0"
#endif
#ifdef __NR_delete_module
	/* TOK_MR */ "ModRm [-f] name\0"
#endif
	/* TOK_MT */ "MounT dev[(major:minor)] mnt type [{rw|ro} [flags]]\0"
	/* TOK_MV */ "MoVe old_dir new_dir : mount --move\0"
	/* TOK_PO */ "POwer off\0"
	/* TOK_PR */ "PivotRoot new_root old_relative\0"
	/* TOK_RB */ "ReBoot\0"
	/* TOK_RD */ "ReaD prompt\0"
	/* TOK_RE */ "REmount dev[(major:minor)] mnt type [{rw|ro} [flags]]\0"
	/* TOK_RF */ "Random Feed [file|dir]*\0"
	/* TOK_RM */ "RM file\0"
	/* TOK_RX */ "Remote-eXec dir cmd [args] : fork+chroot+execve\0"
	/* TOK_SE */ "SetEnv name [value]\0"
	/* TOK_SL */ "SLeep sec[.frac]: sleep this delay or forever if <0 or 'inf'\0"
	/* TOK_SP */ "SusPend\0"
	/* TOK_ST */ "STat file\0"
	/* TOK_SW */ "SWitchroot root\0"
	/* TOK_TA */ "TAr [x|xv|t] file [dir]\0"
	/* TOK_TD */ "TestDev : test if /dev is devtmpfs\0"
	/* TOK_TE */ "TEst var=val\0"
	/* TOK_TN */ "TestNon-empty ${var}: returns true if set and not empty\0"
	/* TOK_UM */ "UMount dir\0"
	/* TOK_WK */ "WaitKey prompt delay\0"
	;

/* errno names from 1, each ending with \0. The list ends on \xFF. */
static const char errstr[] = ""
        /* EPERM */     "PERM\0"
        /* ENOENT */    "NOENT\0"
        /* ESRCH */     "SRCH\0"
        /* EINTR */     "INTR\0"
        /* EIO */       "IO\0"
        /* ENXIO */     "NXIO\0"
        /* E2BIG */     "2BIG\0"
        /* ENOEXEC */   "NOEXEC\0"
        /* EBADF */     "BADF\0"
        /* ECHILD */    "CHILD\0"
        /* EAGAIN */    "AGAIN\0"
        /* ENOMEM */    "NOMEM\0"
        /* EACCES */    "ACCES\0"
        /* EFAULT */    "FAULT\0"
        /* ENOTBLK */   "NOTBLK\0"
        /* EBUSY */     "BUSY\0"
        /* EEXIST */    "EXIST\0"
        /* EXDEV */     "XDEV\0"
        /* ENODEV */    "NODEV\0"
        /* ENOTDIR */   "NOTDIR\0"
        /* EISDIR */    "ISDIR\0"
        /* EINVAL */    "INVAL\0"
        /* ENFILE */    "NFILE\0"
        /* EMFILE */    "MFILE\0"
        /* ENOTTY */    "NOTTY\0"
        /* ETXTBSY */   "TXTBSY\0"
        /* EFBIG */     "FBIG\0"
        /* ENOSPC */    "NOSPC\0"
        /* ESPIPE */    "SPIPE\0"
        /* EROFS */     "ROFS\0"
        /* EMLINK */    "MLINK\0"
        /* EPIPE */     "PIPE\0"
        /* EDOM */      "DOM\0"
        /* ERANGE */    "RANGE\0"
	"\xff\0"; /* end marker */

/* mandatory device nodes : name, mode, gid, major, minor */
static const struct dev_node dev_nodes[] =  {
	/* console must always be at the first location */
	{ "console", 0600 | S_IFCHR, GID_TTY,  5, 1 },
	{ "mem",     0640 | S_IFCHR, GID_KMEM, 1, 1 },
	{ "kmem",    0640 | S_IFCHR, GID_KMEM, 1, 2 },
	{ "null",    0666 | S_IFCHR, GID_ROOT, 1, 3 },
	{ "port",    0640 | S_IFCHR, GID_KMEM, 1, 4 },
	{ "zero",    0666 | S_IFCHR, GID_ROOT, 1, 5 },
	{ "full",    0666 | S_IFCHR, GID_ROOT, 1, 7 },
	{ "random",  0644 | S_IFCHR, GID_ROOT, 1, 8 },
	{ "urandom", 0644 | S_IFCHR, GID_ROOT, 1, 9 },
	{ "tty0",    0600 | S_IFCHR, GID_TTY,  4, 0 },
	{ "tty",     0666 | S_IFCHR, GID_TTY,  5, 0 },
	{ "ptmx",    0666 | S_IFCHR, GID_TTY,  5, 2 },
	{ "initctl", 0600 | S_IFIFO, GID_ROOT, 0, 0 },
};


/*
 * Global variables now.
 */

static char cfg_data[MAX_CFG_SIZE];
static char *cfg_args[MAX_CFG_ARGS];
static char *cfg_line;
static char cmdline[MAX_CMDLINE_LEN];
static int cmdline_len;
static char *cst_str[MAX_FIELDS];
static char *var_str[MAX_FIELDS];
static char mounts[MAX_MNT_SIZE];
static char parts[MAX_PART_SIZE]; // /proc/partitions
static struct dev_varstr var[MAX_FIELDS];
static int error;       /* an error has emerged from last operation */
static int error_num;   /* a copy of errno when error != 0 */
static int linuxrc;     /* non-zero if we were called as 'linuxrc' */
static char tmp_path[MAXPATHLEN];
static struct blkdev *blkdevs;


/*
 * Utility functions
 */


/* prints character <c> */
static void pchar(char c)
{
	write(1, &c, 1);
}

/* prints string <s> up to <n> chars */
static void printn(const char *c, int n)
{
	const char *p = c;

	while (*p && n--)
		p++;

	write(1, c, p-c);
}

/* same as above but also adds a trailing '\n' */
static void printnln(const char *c, int n)
{
	printn(c, n);
	pchar('\n');
}

/* Used to emit informative messages */
static void print(const char *c)
{
	printn(c, -1);
}

/* same as above but also adds a trailing '\n' */
static void println(const char *c)
{
	printnln(c, -1);
}

/* Used only to emit debugging messages when compiled with -DDEBUG */
static void debug(char *c)
{
#ifdef DEBUG
	char *p = c;

	while (*p)
		p++;

	write(0, c, p-c);
#endif
}

static unsigned long my_atoul(const char *s)
{
	unsigned long res = 0;
	unsigned long digit;

	while (*s) {
		digit = *s - '0';
		if (digit > 9)
			return 0;
		res = res * 10 + digit;
		s++;
	}
	return res;
}

/* reads the digits in <s>, stores them as a number in <ret> and
 * returns the pointer to the first non-digit character.
 */
static char *my_atoull_next(char *s, unsigned long long *ret)
{
	unsigned long long res = 0;
	unsigned long long digit;

	while (*s) {
		digit = *s - '0';
		if (digit > 9)
			break;
		res = res * 10ULL + digit;
		s++;
	}
	*ret = res;
	return s;
}

static int streq(const char *str1, const char *str2)
{
	char c1;

	while ((c1 = *str1) && c1 == *str2) { /* the second test ensures that *str2 != 0 */
		str1++;
		str2++;
	}
	return ((c1 | *str2) == 0);
}

/* compares strings str1 and str2 up to <limit> characters, returns non-zero if
 * equal otherwise non-zero. <limit> must be > 0 (compared on equality).
 */
static int streqlen(const char *str1, const char *str2, int limit)
{
	char c1;

	while ((c1 = *str1) && c1 == *str2) { /* the second test ensures that *str2 != 0 */
		str1++;
		str2++;
		if (--limit <= 0)
			return 1;
	}
	return ((c1 | *str2) == 0);
}

/* size-optimized memmove() alternative. It's 40 bytes on x86_64. */
static void my_memmove(char *dst, const char *src, int len)
{
	int pos = (dst <= src) ? -1 : len;

	while (len--) {
		pos += (dst <= src) ? 1 : -1;
		dst[pos] = src[pos];
	}
}

static void my_strcpy(char *dst, const char *src)
{
	while ((*dst++ = *src++));
}

static int my_strlen(const char *str)
{
	int len;

	for (len = 0; str[len]; len++);
	return len;
}

/*
 * copies at most <size-1> chars from <src> to <dst>. Last char is always
 * set to 0, unless <size> is 0. The number of chars copied is returned
 * (excluding the terminating zero).
 * This code has been optimized for size and speed : on x86, it's 45 bytes
 * long, uses only registers, and consumes only 4 cycles per char.
 */
static int my_strlcpy(char *dst, const char *src, int size)
{
	char *orig = dst;

	if (size) {
		while (--size && (*dst = *src)) {
			src++; dst++;
		}
		*dst = 0;
	}
	return dst - orig;
}

/*
 * looks for the last assignment of variable 'var=' in 'envp'.
 * the value found is returned (or NULL if not found). Note
 * that if the variable name does not end with an '=', then it
 * will be implicitly checked.
 * if 'remove' is not zero, the assignment is removed afterwards.
 * eg: init=my_getenv(envp, "init=", 1);
 */
static char *my_getenv(char **envp, char *var, const int remove)
{
	int namelen;
	char **last;

	last = NULL;
	namelen = my_strlen(var);
	if (!namelen || !envp)
		return NULL;

	while (*envp != NULL) {
		if (streqlen(*envp, var, namelen) &&
		    ((var[namelen-1] == '=') || envp[0][namelen] == '='))
			last = envp;
		envp++;
	}

	if (last == NULL) {
		return NULL;
	} else {
		char *ret = (*last) + namelen;
		if (var[namelen-1] != '=')
			ret++;

		if (remove) {
			while (*last != NULL) {
				*last = *(last+1);
				last++;
			}
		}
		return ret;
	}
}

/* appends a constant string <cst> to the end <dest> of a string and returns
 * the pointer to the new end. No size checks are performed.
 */
static char *addcst(char *dest, const char *cst)
{
	while ((*dest++ = *cst++) != '\0');
	return dest - 1;
}

/* appends a character <chr> to the end <dest> of a string and returns the
 * pointer to the new end. No size checks are performed.
 */
static char *addchr(char *dest, char chr)
{
	*dest++ = chr;
	*dest = '\0';
	return dest;
}

/* appends a 32-bit unsigned integer <num> to the end <dest> of a string and
 * returns the pointer to the new end. No size checks are performed.
 */
static char *adduint(char *dest, uint32_t num)
{
	uint32_t div;

	for (div = 1; num / div >= 10; div *= 10)
		;

	while (div) {
		*dest++ = '0' + num / div;
		num %= div;
		div /= 10;
	}

	*dest = '\0';
	return dest;
}

/* appends an 8-bit unsigned hex integer <num> to the end <dest> of a string
 * and returns the pointer to the new end. No size checks are performed.
 */
static char *addhex(char *dest, uint8_t num)
{
	uint8_t c;

	if (num > 0x0F) {
		c = num >> 4;
		*dest++ = (c > 9) ? (c - 10 + 'a') : (c + '0');
	}
	c = num & 0x0F;
	*dest++ = (c > 9) ? (c - 10 + 'a') : (c + '0');
	*dest = '\0';
	return dest;
}

/* looks up errno value <num> into errstr and retuns a pointer to its name if
 * found otherwise NULL.
 */
static const char *find_errstr(int num)
{
	const char *ret = errstr;

	while (*ret != '\xff') {
		/* ok valid names present */
		if (!--num)
			return ret;
		while (*(ret++))
			;
	}
	return NULL;
}

/* breaks a 3-fields, comma-separated string into 3 fields */
static int varstr_break(char *str, char *type, char **set, uint8_t *scale)
{
	int state;
	char *res[3];

	for (state = 0; state < 3; state++) {
		res[state] = str;

		while (*str && *str != ',')
			str++;

		if (*str)
			*str++ = 0;
		else if (state < 2)
			return 1;
	}

	*type = *res[0];
	*set = res[1];
	*scale = my_atoul(res[2]);
	return 0;
}

/* reads a range from a string of the form "low-high" or "value".
 * Returns 0 if OK, or 1 if error.
 */
static int int_range(char *from, uint8_t *low, uint8_t *high)
{
	*low = 0;
	while (*from) {
		if ((unsigned char)(*from - '0') <= 9)
			*low = *low * 10 + (*from - '0');
		else if (*from == '-') {
			low = high;
			*low = 0;
		}
		else
			return 1;
		from++;
	}
	if (low != high)  /* high has not been written to */
		*high = *low;

	return 0;
}

/* reads a range from a hex string of the form "low-high" or "value".
 * Returns 0 if OK, or 1 if error.
 */
static int hex_range(char *from, uint8_t *low, uint8_t *high)
{
	uint8_t c;
	*low = 0;
	while ((c = *from) != '\0') {
		if (c == '-') {
			low = high; /* all writes will now be done on <high> */
			*low = 0;
		}
		else {
			if (c >= '0' && c <= '9')
				c -= '0';
			else if (c >= 'a' && c <= 'f')
				c -= 'a' - 10;
			else if (c >= 'A' && c <= 'F')
				c -= 'A' - 10;
			else
				return 1;

			*low = (*low << 4) + c;
		}
		from++;
	}
	if (low != high) /* high has not been written to */
		*high = *low;

	return 0;
}

/* converts an octal string into an unsigned long equivalent, reads no more
 * than <limit> characters.
 */
static unsigned long base8_to_ul_lim(const char *ascii, unsigned int limit)
{
	unsigned long m = 0;

	while (limit && (unsigned char)(*ascii - '0') < 8) {
		m = (m << 3) | (*ascii - '0');
		ascii++;
		limit--;
	}
	return m;
}

/* converts an octal string into an unsigned long equivalent (reads no
 * more than 4GB.
 */
static unsigned long base8_to_ul(const char *ascii)
{
	return base8_to_ul_lim(ascii, ~0);
}

/* Measures the environment size in total bytes and number of variables.
 * Trailing zeroes are accounted for in the number of bytes so that the result
 * can be used to allocate a copy of this environment. Similarly the trailing
 * NULL entry is accounted for in the number of entries. It also returns the
 * pointer to the final NULL (the environment's tail).
 */
static char **env_size(char **envp, int *bytes, int *entries)
{
	*bytes = 0;
	*entries = 1;
	while (*envp != NULL) {
		*bytes += my_strlen(*envp) + 1;
		(*entries)++;
		envp++;
	}
	return envp;
}

/* Finds the pointers to the tail of the environment, i.e. the first byte after
 * the final 0 is set into env_store, and the pointer to the final NULL is
 * returned. Note that if the environment is totally empty, env_store will be
 * assigned a NULL.
 */
static char **env_get_tail(char **envp, char **env_store)
{
	*env_store = NULL;
	while (*envp != NULL) {
		*env_store = *envp;
		envp++;
	}
	return envp;
}

/* duplicate <old_env> into <new_env> using <env_store> for the contents, and
 * terminate it with a NULL. The caller must ensure that all contents fit into
 * env_store and new_env, including the trailing NULL for new_env. This may be
 * used when expanding the allocation area for adding new entries. <env_store>
 * is updated, and the pointer to the final NULL is returned in order to help
 * with appending new variables.
 */
static char **env_dup(char **new_env, char **old_env, char **env_store)
{
	char *store = *env_store;

	while (*old_env)
		store = addcst(*(new_env++) = store, *(old_env++)) + 1;
	*env_store = store;

	/* and the final NULL */
	*new_env = NULL;
	return new_env;
}

/* append line <line> to the environment in <env_tail> stored at <env_store>.
 * The caller is responsible for making sure that there is at least one entry
 * left in <env_tail> so that the NULL it contain can be replaced and appended
 * again, and enough bytes left in <env_store> for the line and its trailing 0.
 * <env_store> is updated to point past the trailing zero and <env_tail> is
 * updated to point to the new NULL, so that multiple calls can be chained for
 * multiple variables. The function is expected to only be called after
 * env_dup() or itself, so that <env_store> already points to the first byte
 * past the end of the environment, and <env_tail> points to the final NULL.
 */
static char **env_append_line(char **env_tail, char **env_store, const char *line)
{
	char *store = *env_store;

	*env_tail = store;
	/* and the final NULL */
	*(++env_tail) = NULL;
	store = addcst(store, line) + 1;
	*env_store = store;
	return env_tail;
}

/* append line "name=value" to the environment in <env_tail> stored at
 * <env_store>. The caller is responsible for making sure that there is at
 * least one entry left in <env_tail> so that the NULL it contain can be
 * replaced and appended again, and enough bytes left in <env_store> for the
 * line and its trailing 0. <env_store> is updated to point past the trailing
 * zero and the pointer to the final NULL is returned so that multiple calls
 * can be chained for multiple variables. The function is expected to only be
 * called after env_dup() or itself, so that <env_store> already points to the
 * first byte past the end of the environment, and <env_tail> points to the
 * final NULL.
 */
static char **env_append_var(char **env_tail, char **env_store, const char *name, const char *value)
{
	char *store = *env_store;

	/* add our new variable */
	*env_tail = store;

	/* and the final NULL */
	*(++env_tail) = NULL;

	store = addcst(store, name);
	store = addchr(store, '=');
	store = addcst(store, value);
	store++; // \0
	*env_store = store;
	return env_tail;
}

/* opens file <file> and reads up to <size> bytes from it into <buffer>, then
 * closes the file and returns the number of bytes read, or <0 on error, in
 * which case error_num will be set to errno. If <size> is at least 1, a
 * trailing zero is appended, even if it means truncating that file.
 */
static int read_from_file(const char *file, char *buffer, int size)
{
	int fd, len;

	fd = open(file, O_RDONLY, 0);
	if (fd == -1) {
		error_num = errno;
		return -1;
	}

	len = read(fd, buffer, size - 1);
	error_num = errno;
	close(fd);
	if (len >= 0)
		buffer[len] = 0;
	return len;
}

/* flushes stdin and waits for it to be done */
static void flush_stdin()
{
	int ret;
	fd_set in;
	struct timeval tv;

	while (1) {
		FD_ZERO(&in);
		FD_SET(0, &in);
		tv.tv_sec = tv.tv_usec = 0;
		ret = select(1, &in, NULL, NULL, &tv);

		if (ret <= 0)
			break;

		read(0, &ret, sizeof(ret));
	}
}

/* waits <delay> seconds for a character to be entered from stdin. It returns
 * zero if the timeout expires, otherwise it flushes stdin and returns 1
 */
static int keypressed(int delay)
{
	int ret;
	fd_set in;
	struct timeval tv;

	FD_ZERO(&in);
	FD_SET(0, &in);
	tv.tv_sec = delay; tv.tv_usec=0;
	ret = select(1, &in, NULL, NULL, &tv);

	if (ret <= 0)
		return 0;

	flush_stdin();
	return 1;
}

/* makes a dev entry. continues on error, but reports them. */
static int mknod_chown(mode_t mode, uid_t uid, gid_t gid, uint8_t major, uint8_t minor, char *name)
{
	int error;

	if (mknod(name, mode, makedev(major, minor)) == -1 && chmod(name, mode) == -1) {
		error_num = errno;
		error = 1;
		debug("init/error : mknod("); debug(name); debug(") failed\n");
	}

	if (chown(name, uid, gid) == -1) {
		error_num = errno;
		error = 1;
		debug("init/error : chown("); debug(name); debug(") failed\n");
	}

	return error;
}

/* tries to create the specified path and its parents. Returns the zero on
 * success, or the last mkdir() return value or zero. It stores a copy of
 * <path> into <tmp_path> if it's not already the same string, so that inner
 * calls can modify the path. Outer directories are created with mode 755.
 */
static int recursive_mkdir(const char *path, mode_t mode)
{
	int pos;
	int ret;

	if (path != tmp_path)
		my_strlcpy(tmp_path, path, sizeof(tmp_path));

	/* trim trailing slashes */
	pos = my_strlen(tmp_path) - 1;
	while (pos > 0 && tmp_path[pos] == '/')
		tmp_path[pos--] = 0;

	ret = mkdir(tmp_path, mode);
	if (ret == -1 && errno == ENOENT) {
		/* look for the leftmost '/' in the last '/.../' block */
		for (; pos > 0; pos--) {
			if (tmp_path[pos] == '/' && tmp_path[pos - 1] != '/') {
				tmp_path[pos] = 0;
				ret = recursive_mkdir(tmp_path, 0755);
				tmp_path[pos] = '/';
				if (ret == 0) {
					ret = mkdir(tmp_path, mode);
					if (ret < 0)
						error_num = errno;
				}
				break;
			}
		}
	}
	return ret;
}

/* closes 0,1,2 and opens /dev/console on them instead */
static void reopen_console()
{
	int i, fd, oldfd;

	oldfd = dup2(0, 3);	// keep a valid console on fd 3

	for (i = 0; i < 3; i++)
		close(i);

	fd = open("/dev/console", O_RDWR, 0); // fd = 0 (stdin) or -1 (error)
	if (fd < 0)
		dup2(oldfd, 0); // restore 0 from old console

	close(oldfd);
	dup2(0, 1); // stdout
	dup2(0, 2); // stderr

	debug("init/info : reopened /dev/console\n");
}

/* return 0 if at least one entry is missing */
static int is_dev_populated()
{
	struct stat statf;

	if (stat("/dev/console", &statf) == -1)
		return 0;

	if (stat("/dev/null", &statf) == -1)
		return 0;

	return 1;
}

/* tries to display or extract a tar archive from file <file> to directory
 * <dir>. The <action> may be "x"/"xv" for extract or "t" to display (test).
 * Returns 0 on success or -1 with errno possibly set. This function only
 * supports a subset of the posix and the GNU/old GNU tar formats. Sparse
 * files are not supported, uname/gname/mtime are ignored, large files are
 * not supported, write errors are ignored. The tar format is quite simple
 * for extraction and is described here :
 *
 *    https://www.gnu.org/software/tar/manual/html_node/Standard.html
 */
static int tar_extract(const char *action, const char *file, const char *dir)
{
	union {
		char buffer[512];             /* raw data */
		struct {                      /* byte offset */
			char name[100];       /*   0 */
			char mode[8];         /* 100 */
			char uid[8];          /* 108 */
			char gid[8];          /* 116 */
			char size[12];        /* 124 */
			char mtime[12];       /* 136 */
			char chksum[8];       /* 148 */
			char typeflag;        /* 156 */
			char linkname[100];   /* 157 */
			char magic_ver[8];    /* 257 : "ustar  \0" or "ustar\0""00" */
			char uname[32];       /* 265 */
			char gname[32];       /* 297 */
			char devmajor[8];     /* 329 */
			char devminor[8];     /* 337 */
			char prefix[155];     /* 345 */
			/* 500 */
		} hdr;
	} blk;

	char name[256];
	char dest[256];
	int ret, len, pos;
	int mode, major, minor;
	int uid, gid;
	int fd, outfd;
	unsigned long size;

	if (!dir)
		dir = ".";

	if (!action)
		action = "";

	ret = fd = open(file, O_RDONLY, 0);
	error_num = errno;
	if (ret < 0)
		goto out_ret;

	ret = 0;
	outfd = -1;
	while (1) {
		/* note: we always come here with ret == 0, except if we
		 * just met empty blocks in which case ret == the number of
		 * consecutive empty blocks seen.
		 */
		len = read(fd, blk.buffer, sizeof(blk.buffer));
		error_num = errno;
		if (len < sizeof(blk.buffer))
			goto out_fail;

		/* accept any form of ustar */
		if (!streqlen(blk.hdr.magic_ver, "ustar", 5)) {
			/* the end of the archive is marked by two consecurive
			 * empty records, otherwise it's a real error.
			 */
			while (len) {
				if (blk.buffer[--len])
					goto out_fail;
			}
			if (++ret < 2)
				continue;

			/* we've reached the end */
			break;
		}

		/* concatenate hdr.prefix and hdr.name into <name> */
		len = 0;
		len += my_strlcpy(dest + len, blk.hdr.prefix, 156);     // max 155 copied.
		len += my_strlcpy(dest + len, blk.hdr.name, 101);       // max 100 copied.

		if (action[0] == 't' || (action[0] == 'x' && action[1] == 'v'))
			printnln(dest, len);

		/* now concatenate <dir>, "/" and <dest> into <name> */
		pos = 0;
		pos += my_strlcpy(name + pos, dir, sizeof(name));
		pos += my_strlcpy(name + pos, "/", sizeof(name) - pos);
		pos += my_strlcpy(name + pos, dest, sizeof(name) - pos);

		/* link target for hardlinks/symlinks */
		len  = my_strlcpy(dest, blk.hdr.prefix, 156);     // max 155 copied.
		len += my_strlcpy(dest + len, blk.hdr.linkname, 101); // max 100 copied.

		mode = base8_to_ul_lim(blk.hdr.mode, sizeof(blk.hdr.mode));
		uid  = base8_to_ul_lim(blk.hdr.uid,  sizeof(blk.hdr.uid));
		gid  = base8_to_ul_lim(blk.hdr.gid,  sizeof(blk.hdr.gid));
		size = base8_to_ul_lim(blk.hdr.size, sizeof(blk.hdr.size));
		major = base8_to_ul_lim(blk.hdr.devmajor, sizeof(blk.hdr.devmajor));
		minor = base8_to_ul_lim(blk.hdr.devminor, sizeof(blk.hdr.devminor));

		if (*action == 'x') {
			/* extract */
			/* For all common types, we need to create the parent directory
			 * first. These are types 0 and '0'..'6'.
			 */
			if (!blk.hdr.typeflag ||
			    (unsigned char)(blk.hdr.typeflag - '0') <= 6) {
				while (pos > 0 && name[pos] != '/')
					pos--;
				if (pos && name[pos] == '/') {
					name[pos] = 0;
					recursive_mkdir(name, 0755);
					name[pos] = '/';
				}
			}

			switch (blk.hdr.typeflag) {
			case  0  :
			case '0' : /* normal file */
				ret = outfd = open(name, O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE, mode);
				if (ret >= 0)
					chown(name, uid, gid);
				error_num = errno;
				/* the error is handled below by not copying the data */
				break;
			case '1' : /* hard link */
				ret = link(dest, name);
				error_num = errno;
				break;
			case '2' : /* symlink */
				ret = symlink(dest, name);
				if (ret >= 0)
					chown(name, uid, gid);
				error_num = errno;
				break;
			case '3' : /* char dev */
				ret = -mknod_chown(mode | S_IFCHR, uid, gid, major, minor, name);
				break;
			case '4' : /* block */
				ret = -mknod_chown(mode | S_IFBLK, uid, gid, major, minor, name);
				break;
			case '5' : /* directory */
				ret = recursive_mkdir(name, mode);
				break;
			case '6' : /* fifo */
				ret = -mknod_chown(mode | S_IFIFO, uid, gid, major, minor, name);
				break;
			default: /* unhandled, silently skip size */
				break;
			}
		}

		/* now copy the file data or simply skip the record.
		 * outfd < 0 if we only want to silently skip.
		 */
		while (size > 0) {
			char buffer[4096];

			len = size;
			if (len > sizeof(buffer))
				len = sizeof(buffer);
			len = (len + 511) & -512;
			len = read(fd, buffer, len);
			error_num = errno;
			if (len <= 0)
				goto out_fail;

			if ((unsigned long)len > size)
				len = size;
			size -= len;

			if (outfd >= 0) {
				if (write(outfd, buffer, len) < 0) {
					error_num = errno;
					close(outfd);
					outfd = -1;
				}
			}
		}

		if (outfd >= 0) {
			close(outfd);
			outfd = -1;
		}
		ret = 0;
	}

	ret = 0;
out_close:
	if (outfd >= 0)
		close(outfd);
	close(fd);
out_ret:
	return ret;
out_fail:
	ret = -1;
	goto out_close;
}

/* Lists files and dirs found in <dir>. Optionally takes "[-]e" to only test
 * for file existence (non-empty dir), "[-]l" for the long output format, where
 * each entry is prefixed with one of [bcdlps] depending on its type.
 * Returns 0 on success or -1 with errno possibly set. For the -e test, returns
 * -1 if the directory is empty.
 */
static int list_dir(const char *fmt, const char *dir)
{
	/* man getdents64 for the details, or include/linux/dirent.h */
	struct linux_dirent64 *d;
	const char *str;
	char buffer[4096];
	int ret, len, pos;
	int fd, count;

	if (!fmt)
		fmt = "";

	if (!dir || !*dir) {
		dir = fmt;
		fmt = "";
	}

	if (!*dir)
		dir = ".";

	if (*fmt == '-')
		fmt++;

	ret = fd = open(dir, O_RDONLY | O_DIRECTORY, 0);
	error_num = errno;
	if (ret < 0)
		goto out_ret;

	ret = count = 0;
	while ((ret = len = getdents64(fd, (void *)buffer, sizeof(buffer))) > 0) {
		for (pos = 0; pos < len; pos += d->d_reclen) {
			d = (struct linux_dirent64 *)&buffer[pos];

			count++;
			if (*fmt == 'e')
				continue;
			else if (*fmt == 'l') {
#if DT_UNKNOWN == 0 && DT_SOCK == 12
				/* this should always be true on linux, this avoids the switch/case
				 * and saves around 100 bytes on x86_64. We need a single contigous
				 * string to emit 2 chars at once...
				 */
				unsigned int type = d->d_type;
				static const char code[] =
					"? " // 0 == DT_UNKNOWN
					"p " // 1 == DT_FIFO
					"c " // 2 == DT_CHR
					"? " // 3
					"d " // 4 == DT_DIR
					"? " // 5
					"b " // 6 == DT_BLK
					"? " // 7
					"- " // 8 == DT_REG
					"? " // 9
					"l " // 10 == DT_LNK
					"? " // 11
					"s " // 12 == DT_SOCK
					"";

				if (type > DT_SOCK)
					type = DT_UNKNOWN;
				str = code + type * 2;
#else
				switch (d->d_type) {
				case DT_FIFO : str = "p "; break;
				case DT_CHR  : str = "c "; break;
				case DT_DIR  : str = "d "; break;
				case DT_BLK  : str = "b "; break;
				case DT_REG  : str = "- "; break;
				case DT_LNK  : str = "l "; break;
				case DT_SOCK : str = "s "; break;
				default      : str = "? "; break;
				}
#endif
				printn(str, 2);
			}
			println(d->d_name);
		}
	}
	/* here we have ret = 0 on success or -1 on error. We may want to change
	 * this to only check whether something exists in the directory (-e).
	 */

	if (*fmt == 'e')
		ret = count ? 0 : -1;
	close(fd);
out_ret:
	return ret;
}

/* Uses rnd->buf to feed rnd->buf_size bytes to random device opened in
 * <rand_fd>. One bit per 16 input bytes is accounted for. The number of
 * entropy bits added is returned.
 */
static int _feed_random_to_dev(int rand_fd, struct rnd *rnd)
{
	if (rnd->buf_size > sizeof(rnd->buf))
		rnd->buf_size = sizeof(rnd->buf);
	rnd->entropy_count = (rnd->buf_size + 15) / 16;
	if (ioctl(rand_fd, RNDADDENTROPY, rnd) < 0)
		return 0;
	return rnd->entropy_count;
}

/* feeds <area> for <size> bytes to random opened in <rand_fd> counting one bit
 * per 16 input bytes. The number of entropy bits added is returned.
 */
static int feed_random_area_to_dev(int rand_fd, const void *area, int size)
{
	struct rnd rnd;
	int tot_ent = 0;
	int ret;

	while (size > 0) {
		rnd.buf_size = sizeof(rnd.buf);
		if (rnd.buf_size > size)
			rnd.buf_size = size;
		memcpy(rnd.buf, area, rnd.buf_size);
		if (!_feed_random_to_dev(rand_fd, &rnd))
			break;
		tot_ent += rnd.entropy_count;
		size -= rnd.buf_size;
	}
	return tot_ent;
}

/* read file/link <path> and pass its contents as entropy to rand_fd.
 * Returns the number of bits added, possibly 0 (e.g. nothing useful)
 * or -1 on failure.
 */
static int feed_random_from_file(int rand_fd, const char *path)
{
	struct stat statf;
	struct rnd rnd;
	int tot_ent = 0;
	int size, ret;
	int fd;

	if (stat(path, &statf) == -1)
		return -1;

	tot_ent = feed_random_area_to_dev(rand_fd, &statf, sizeof(statf));
	if (S_ISREG(statf.st_mode)) {
		/* regular file. We use O_NONBLOCK so that /proc/kmsg works
		 * as well. We limit ourselves to 128kB.
		 */
		fd = open(path, O_NONBLOCK | O_RDONLY);
		if (fd < 0)
			return tot_ent;

		for (size = 0; size < 131072; size += ret) {
			rnd.buf_size = read(fd, rnd.buf, sizeof(rnd.buf));
			if (rnd.buf_size <= 0)
				break;
			if (!_feed_random_to_dev(rand_fd, &rnd))
				break;
			tot_ent += rnd.entropy_count;
		}
		close(fd);
	}
	return tot_ent;
}

/* Try to feed /dev/random with the contents of the specified dirs/files.
 * Dirs are opened, all their entries names and stats are used for the hash
 * (e.g useful with device trees or PCI entries), links and files are read
 * and fed as-is. Each stat counts for one bit. Each read link or file counts
 * for one bit every 16 bytes.
 */
static int feed_random_from_path(char *const *args)
{
	struct linux_dirent64 *d;
	struct timeval tv;
	char buffer[4096];
	char path[256];
	char *fpath;
	int tot_ent = 0;
	int rand_fd;
	int pos, fd;
	int len;

	rand_fd = open("/dev/random", O_WRONLY | O_NONBLOCK);
	if (rand_fd < 0)
		return tot_ent;

	gettimeofday(&tv, NULL);
	tot_ent += feed_random_area_to_dev(rand_fd, &tv, sizeof(tv));

	for (; *args; args++) {
		len = my_strlcpy(path, *args, sizeof(path));
		if (len + 2 >= sizeof(path))
			continue;
		path[len++] = '/';
		path[len] = 0;
		fpath = path + len;

		fd = open(*args, O_RDONLY | O_DIRECTORY, 0);
		if (fd < 0) {
			/* either not found or not a directory */
			tot_ent += feed_random_from_file(rand_fd, *args);
			args++;
			continue;
		}

		while ((len = getdents64(fd, (void *)buffer, sizeof(buffer))) > 0) {
			for (pos = 0; pos < len; pos += d->d_reclen) {
				d = (struct linux_dirent64 *)&buffer[pos];
				if (streq(d->d_name, "kcore") || streq(d->d_name, "kallsyms") ||
				    streq(d->d_name, "kmem")  || streq(d->d_name, "mem"))
					continue;
				my_strlcpy(fpath, d->d_name, path + sizeof(path) - fpath);
				tot_ent += feed_random_from_file(rand_fd, path);
			}
		}
		close(fd);
	}

	close(rand_fd);
	return tot_ent;
}

/*
 * Functions below are totally specific to the program and its configuration
 * language.
 */


/* returns the FD type for /dev by scanning /proc/mounts whose format is :
 *   [dev] [mnt] [type] [opts*] 0 0
 */
static char *get_dev_type()
{
	/* scan /proc/mounts for the best match for /dev */
	int len;
	int best;
	char *ptr, *end, *mnt, *match;

	if ((len = read_from_file("/proc/mounts", mounts, sizeof(mounts))) <= 0)
		return NULL;

	end = mounts + len;

	best = 0;
	for (ptr = mounts; ptr < end; ptr++) {
		/* skip dev name */
		while (*ptr && *ptr != ' ')
			ptr++;
		/* skip spaces */
		while (*ptr == ' ')
			ptr++;
		mnt = ptr;
		/* skip mnt name */
		while (*ptr && *ptr != ' ')
			ptr++;
		if (*ptr)
			*(ptr++) = 0;

		/* look for the longest exact match of "/" or "/dev" */
		if ((ptr - mnt) > best && (streq(mnt, "/dev") || streq(mnt, "/"))) {
			best = ptr - mnt; // counts the trailing zero
			/* skip FS name and terminate with zero */
			match = ptr;
			while (*ptr && *ptr != ' ')
				ptr++;
			if (*ptr)
				*(ptr++) = 0;
		}
		/* skip to next line */
		while (*ptr && *ptr != '\n')
			ptr++;
	}
	return best ? match : NULL;
}

/* reads the kernel command line </proc/cmdline> into memory and searches
 * for the first assignment of the required variable. Return its value
 * (which may be empty) or NULL if not found.
 */
char *find_arg(char *arg)
{
	char *a, *c;

	/* read cmdline the first time */
	if (cmdline_len <= 0) {
		if ((cmdline_len = read_from_file("/proc/cmdline", cmdline, sizeof(cmdline))) <= 0)
			return NULL;
		/* trim trailing LFs */
		while (cmdline_len > 0 && cmdline[cmdline_len-1] == '\n')
			cmdline[--cmdline_len] = 0;
		/* always include trailing zero */
		cmdline_len++;
	}

	/* search for the required arg in cmdline */
	c = cmdline;
	a = arg;

	/* we can check up to the last byte because we know it's zero */
	while (c <= cmdline + cmdline_len) {
		if (*a == 0) {
			/* complete match. is it a full word ? */
			if (*c == '=') {
				a = ++c;
				while ((*a != ' ') && (*a != '\0') && (*a != '\n'))
					a++;
				*a = 0;
				return c; /* return value */
			}
			else if (*c == ' ' || *c == '\n' || *c == '\0') {
				*c = 0;
				return c; /* return pointer to empty string */
			}
			else { /* not full word. bad match. */
				c -= (a - arg) - 1;
				a = arg;
			}
		}
		else if (*c == *a) {
			a++;
			c++;
		}
		else {
			c -= (a - arg) - 1;
			a = arg;
		}
	}
	return NULL; /* not found */
}

/* reads the configuration file <cfg_file> into memory.
 * returns 0 if OK, -1 if error.
 */
static int read_cfg(char *cfg_file)
{
	int cfg_size;

	if (cfg_line == NULL) {
		cfg_size = read_from_file(cfg_file, cfg_data, sizeof(cfg_data));
		if (cfg_size < 0)
			return -1;
		cfg_line = cfg_data;
	}
	return 0;
}

/* reads one line from <cfg_data>. comments are ignored. The command token is
 * returned as the result of this function, or TOK_UNK if none matches, or
 * TOK_EOF if nothing left. All args are copied into <cfg_args> as an array
 * of pointers.  Maximum line length is 256 chars and maximum args number is 15.
 * <bufend> must point to the first non-addressable byte of the buffer so that
 * the function can insert data if required due to variables resolving. If
 * <envp> is not NULL, variables may be looked up there.
 */
static int parse_cfg(char **cfg_data, char *bufend, char **envp)
{
	int nbargs;
	int token;
	int cond;
	char *cfg_line = *cfg_data;

	memset(cfg_args, 0, sizeof(cfg_args));
	while (*cfg_line) {
		char c, *p = cfg_line;

		cond = 0;
		/* search beginning of line */
		do {
			if (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
				if (*p == '|') {
					cond |= TOK_COND_OR;
				} else if (*p == '&') {
					cond |= TOK_COND_AND;
				} else if (*p == '!') {
					cond ^= TOK_COND_NEG;
				} else {
					/* catches any printable char and the final zero */
					break;
				}
			}
			p++;
		} while (1);

		/* now search end of line */
		cfg_line = p;
		while (*cfg_line && *cfg_line != '#' && *cfg_line != '\n' && *cfg_line != '\r') {
			cfg_line++;
		}

		/* terminate the line cleanly to avoid further tests */
		while ((c = *cfg_line)) {
			*cfg_line++ = '\0';
			if (c == '\n')
				break;
		}

		/* update the caller's pointer once for all. */
		*cfg_data = cfg_line;

		/* skip empty lines */
		if (!*p)
			continue;

		/* fills the cfg_args[] array with the command itself, followed by all
		 * args. cfg_args[last+1]=NULL.
		 *
		 * We have a particular case here :
		 * if the line begins with '/', then we return TOK_EX so that it executes
		 * the command just as it would have done with 'ex' prefixed to the line.
		 * For this, we insert a fake cfg_args[0], also pointing to the '/', which
		 * will be matched later.
		 */
		nbargs = 0;
		if (*p == '/')
			cfg_args[nbargs++] = p;

		for (; *p && (nbargs < MAX_CFG_ARGS - 1); nbargs++) {
			int backslash = 0, quote = 0, var_state = VAR_NONE;
			char *dollar_ptr = NULL, *name_beg = NULL, *name_end = NULL;
			char *brace_end = NULL, *def_beg = NULL, *def_end = NULL, *repl = NULL;

			cfg_args[nbargs] = p;

			do {
				if (backslash) {
					backslash = 0;
					if (*p == 'n')
						*p = '\n';
					else if (*p == 'r')
						*p = '\r';
					else if (*p == 't')
						*p = '\t';
					my_memmove(p - 1, p, cfg_line - p);
				} else {
					backslash = (*p == '\\');

					if (*p == '"') {
						my_memmove(p, p + 1, cfg_line - p - 1);
						quote = !quote;
						continue;
					}

					if (quote) {
						p++;
						continue;
					}

					/* variables are not evaluated within quotes for now */
					switch (var_state) {
					case VAR_NONE:
						if (*p != '$')
							break;
						dollar_ptr = p;
						var_state = VAR_DOLLAR;
						name_beg = name_end = def_beg = def_end = brace_end = NULL;
						break;
					case VAR_DOLLAR:
						if (*p != '{') {
							var_state = VAR_NONE;
							dollar_ptr = NULL;
						} else {
							var_state = VAR_OBRACE;
							name_beg = p + 1;
						}
						break;
					case VAR_OBRACE:
						if (*p == '-' || *p == '+') {
							var_state = VAR_DEFAULT;
							name_end  = p;
							def_beg = p + 1;
						}
						else if (*p == '}') {
							var_state = VAR_CBRACE;
							name_end  = p;
							brace_end = p + 1;
						}
						break;
					case VAR_DEFAULT:
						if (*p == '}') {
							var_state = VAR_CBRACE;
							def_end  = p;
							brace_end = p + 1;
						}
						break;
					}

					if (var_state == VAR_CBRACE) {
						int replace;

						/* here we have to do everything.
						 * Name_end points to the first character after the name, which can be
						 * '-', '+' or '}'.
						 */
						var_state = VAR_NONE;
						replace = (*name_end == '+');
						*name_end = 0;

						repl = my_getenv(envp, name_beg, 0); // supports envp==NULL and name==NULL

						if (!repl)
							repl = find_arg(name_beg); // check in /proc/cmdline (useful for root= and init=)

						if (replace || !repl) {
							/* easy part: variable not found, or replacement with known a
							 * string, we use the default value. It will always be shorter
							 * than the expression because it's contained in it.
							 */
							int ofs;

							if ((replace && !repl) || (def_end == def_beg)) {
								/* empty string is conserved in replace mode */
								my_memmove(dollar_ptr, brace_end, bufend - brace_end);
								ofs = brace_end - dollar_ptr;
								p -= ofs;
								cfg_line -= ofs;
								*cfg_data = cfg_line;
							} else {
								/* empty string with alternate mode or valid string in replace mode */
								my_memmove(dollar_ptr, def_beg, def_end - def_beg);
								my_memmove(dollar_ptr + (def_end - def_beg), brace_end, bufend - brace_end);
								ofs = (def_beg - dollar_ptr) + (brace_end - def_end);
								p -= ofs;
								cfg_line -= ofs;
								*cfg_data = cfg_line;
							}
						} else {
							/* complicated part, we have to move the tail
							 * left or right, the right size for what we
							 * have to replace.
							 */
							int l = my_strlen(repl);
							int ofs = l - (brace_end - dollar_ptr); /* <0: reduce, >0: increase */

							if (ofs + brace_end > bufend) { // too large, truncate the value.
								l  -= (ofs + brace_end) - bufend;
								ofs = bufend - brace_end;
							}

							my_memmove(dollar_ptr + l, brace_end, (bufend - brace_end) - ((ofs > 0)?ofs:0));
							memcpy(dollar_ptr, repl, l);

							p += ofs;
							cfg_line += ofs;
							*cfg_data = cfg_line;
						}
					}

					p++;
				}
			} while (*p && (backslash || quote || var_state || (*p != ' ' && *p != '\t')));

			if (*p) {
				*p = 0;
				do p++; while (*p == ' ' || *p == '\t');
			}
		}

		/* search a matching token for the command : it can either be a single
		 * char (old language) or a double char (new language).
		 *
		 * We have a particular case :
		 * if the line begins with '/', then we return TOK_EX so that it executes
		 * the command just as it would have done with 'ex' prefixed to the line.
		 */
		if (**cfg_args == '/')
			return TOK_EX | cond;

		for (token = 0; token < sizeof(tokens) / sizeof(tokens[0]); token++) {
			if ((!cfg_args[0][1] && tokens[token].scmd == cfg_args[0][0]) ||
			    (cfg_args[0][1] &&
			     (tokens[token].lcmd[1] == cfg_args[0][1]) &&
			     (tokens[token].lcmd[0] == cfg_args[0][0])))
				return token | cond;
		}

		return TOK_UNK;
	}
	return TOK_EOF;
}

/* builds a device name from the current <var> and <cst_str> arrays,
 * and compute the corresponding minor number by adding all the relevant
 * fields' values
 */
static void name_and_minor(char *name, uint8_t *minor, int fields)
{
	uint8_t min;
	int f;

	min = 0;
	for (f = 0; f < fields; f++) {
		if (*cst_str[f])
			name = addcst(name, cst_str[f]);

		switch (var[f].type) {
		case 'c' :
			name = addchr(name, var[f].u.chr.value);
			min += var[f].u.chr.index * var[f].scale;
			break;
		case 'h' :
			name = addhex(name, var[f].u.num.value);
			goto recalc_int;
		case 'i' :
			name = adduint(name, var[f].u.num.value);
			goto recalc_int;
		case 'I' :
			if (var[f].u.num.value)
				name = adduint(name, var[f].u.num.value);
		recalc_int:
			min += (var[f].u.num.value - var[f].u.num.low) * var[f].scale;
			break;
		case 0 :
			break;
		default:
			debug("init/conf : field type must be c, h, i or I\n");
		}
	}
	if (minor)
		*minor = min;
}

/* increments the index for variable #f. When it overflows, it goes back to the
 * beginning and a carry is returned for the next variable to be incremented.
 * 0 is returned if no carry is returned, otherwise, 1 is returned.
 */
static int inc_var_index(int f)
{
	switch (var[f].type) {
	case 'c' :
		if ((var[f].u.chr.ptr[1] == '-') && (var[f].u.chr.value < var[f].u.chr.ptr[2])) {
			var[f].u.chr.value++;
			var[f].u.chr.index++;
			return 0;
		} else { /* we cannot increment within the current range */
			if (*++var[f].u.chr.ptr == '-') {
				if (*++var[f].u.chr.ptr)
					var[f].u.chr.ptr++;
			}
			if (!*var[f].u.chr.ptr) { /* no other range found */
				var[f].u.chr.value = *(var[f].u.chr.ptr = var[f].u.chr.set);
				var[f].u.chr.index = 0;
				return 1;
			}
			else { /* found a new range */
				var[f].u.chr.value = *var[f].u.chr.ptr;
				var[f].u.chr.index++;
				return 0;
			}
		}
	case 'h':
	case 'i':
	case 'I':
		if (var[f].u.num.value == var[f].u.num.high) {
			var[f].u.num.value = var[f].u.num.low;
			return 1;
		} else {
			var[f].u.num.value++;
			return 0;
		}
	default:
		return 1; /* an empty variable propagates carry */
	}
}


/* makes one or several device inodes depending on the rule <rule> */
static void multidev(mode_t mode, uid_t uid, gid_t gid, uint8_t major, uint8_t minor, char *rule)
{
	enum {
		ST_STRING = 0,
		ST_CONST,
	};

	char *p1, *p2;
	int state;
	int i;
	int field;	/* each field is a couple of a cst string and a var string */

	p1 = p2 = rule;
	state = ST_STRING;
	field = 0;

	cst_str[field] = var_str[field] = p2; /* be sure to point to something valid */

	while (*p2) {
		if (state == ST_STRING) {
			if (*p2 == '[') {
				state = ST_CONST;
				cst_str[field] = p1;
				*p2++ = 0;
				p1 = p2;
			}
			else
				p2++;
		}
		else if (state == ST_CONST) {
			if (*p2 == ']') {
				state = ST_STRING;
				var_str[field++] = p1;
				*p2++ = 0;
				p1 = p2;
				cst_str[field] = p1;
			}
			else
				p2++;
		}
	}

	if (state == ST_STRING) {
		if (p2 > p1) {
			state = ST_CONST;
			cst_str[field] = p1;
			var_str[field++] = p2;
			*p2++ = 0;
		}
	}
	else {
		debug("var string incomplete\n");
	}

	/* now we must "compile" the variable fields */
	for (i = 0; i < field; i++) {
		memset(&var[i], 0, sizeof(var[i]));

		if (varstr_break(var_str[i], &var[i].type, &var[i].u.chr.set, &var[i].scale)) {
			continue;
		}

		switch (var[i].type) {
		case 'c':
			var[i].u.chr.value = *(var[i].u.chr.ptr = var[i].u.chr.set);
			var[i].u.chr.index = 0;
			break;
		case 'h':
			if (hex_range(var[i].u.chr.set, &var[i].u.num.low, &var[i].u.num.high)) {
				debug("init/conf : error in hex range\n");
				continue;
			}
			var[i].u.num.value = var[i].u.num.low;
			break;
		case 'i':
		case 'I':
			if (int_range(var[i].u.chr.set, &var[i].u.num.low, &var[i].u.num.high)) {
				debug("init/conf : error in int range\n");
				continue;
			}
			var[i].u.num.value = var[i].u.num.low;
			break;
		default: // unknown type
			break;
		}
	}

	while (1) {
		char name[MAX_DEVNAME_LEN];
		uint8_t minor_offset;
		int f;

		name_and_minor(name, &minor_offset, field);
		mknod_chown(mode, uid, gid, major, minor + minor_offset, name);
		f = 0;

		while (inc_var_index(f) && (f<field))
			f++;
	
		if (f >= field)
			break;
	}
}

/* Checks in a blkdev list if <name> appears and returns a pointer to its
 * predecessor if found, or to the last pointer pointing to NULL if not found,
 * in order to know where to add the new device.
 */
static struct blkdev **blkdev_find_part(struct blkdev **root, const char *name)
{
	while (*root && !streq((*root)->name, name))
		root = &(*root)->next;
	return root;
}

/* finds from the root where a device name should be attached:
 *  - if the name does not end with a digit, it's a device
 *  - if the candidate's name ends in a digit and the new one has the same name
 *    followed by 'p' and digits, it's a partition of that one;
 *  - if the candidate's name does not end in a digit and the new one has the
 *    same name followed only by digits, it's a partition of that one;
 *  - if a candidate has exactly the same name, the device was already added,
 *    so NULL is returned.
 *  - otherwise it's a new device.
 * Note that parents must be added before partitions, which is already the case
 * in /proc/partitions.
 */
static struct blkdev **blkdev_find_parent(struct blkdev **root, const char *name)
{
	int namelen = my_strlen(name);
	int commlen;

	while (*root) {
		for (commlen = 0; name[commlen] && name[commlen] == (*root)->name[commlen]; commlen++)
			;
		if (commlen && commlen == my_strlen((*root)->name)) {
			if (commlen == namelen)
				return NULL; // exact match, already there

			if ((unsigned char)((*root)->name[commlen-1] - '0') <= 9) {
				/* root ends with digit */
				if (name[commlen] == 'p' && (unsigned char)(name[commlen + 1] - '0') <= 9)
					return &(*root)->part; // matches with 'pXX' after digit
			} else {
				/* root ends without digit */
				if ((unsigned char)(name[commlen] - '0') <= 9)
					return &(*root)->part; // matches with extra digits
			}
			/* does not match (e.g. sda1 vs sda11) */
		}
		root = &(*root)->next;
	}
	/* new device, append to the end */
	return root;
}

/* detect and return the FS type of device (or file) in <path>. A const string
 * is returned, or NULL if unknown.
 */
static const char *find_fs_type(const char *path)
{
	uint8_t buf[1536 + 1]; /* normally suffices */
	uint32_t sig32;
	int len;

	len = read_from_file(path, (char*)buf, sizeof(buf));
	if (len < sizeof(buf) - 1)
		return NULL; // short/failed read

	/* first, check the 4 first bytes which say a lot about most
	 * FS. We process them in big endian because that's easier to
	 * visually match against hex dumps during testing.
	 */
	sig32 = (buf[0] * 16777216U) + (buf[1] * 65536) + (buf[2] * 256) + buf[3];
	switch (sig32) {
	case 0xffffffff: return "empty";
	case 0x1f8b0800: return "gzip";
	case 0xfd377a58: return "xz";
	case 0x68737173: // "hsqs"
		return buf[0x1c] >= 4 ? "squashfs" : // v4
		                        "squashfs3"; // v2 (0x2) or v3 (0x3)
	case 0x42534658: return "xfs";   // "XFSB"
	case 0x55424923: return "ubi";   // only on mtd
	case 0x31181006: return "ubifs"; // only on mtd
	case 0x27051956: // uimage/ukernel/uinitrd
		return (buf[0x1e] == 2) ? "ukernel" :
		       (buf[0x1e] == 3) ? "uinitrd" :
		       "uimage";
	case 0x1985e001: // "jffs2:dirent"
	case 0x851901e0:
	case 0x1985e002: // "jffs2:inode"
	case 0x851902e0:
	case 0x19852003: // "jffs2:clean"
	case 0x85190320:
	case 0x19852004: // "jffs2:padding"
	case 0x85190420:
	case 0x19852006: // "jffs2:summary"
	case 0x85190620:
	case 0x1985e008: // "jffs2:xattr"
	case 0x851908e0:
	case 0x1985e009: // "jffs2:xref"
	case 0x851909e0:
		return "jffs2";
	}

	/* let's now check for the ext2/3/4 family (0xef53) */
	if (buf[0x438] == 0x53 && buf[0x439] == 0xef)
		return (buf[0x45c] & 0x04) ? // has_journal
		       (buf[0x460] & 0x40) ? // extent
		       "ext4" : "ext3" : "ext2";

	/* fat / mbr ? we'd have 0x55 AA at 0x1FE. */
	if (buf[0x1fe] == 0x55 && buf[0x1ff] == 0xaa) {
		/* FAT starts with a boot sector made of an in-sector
		 * jump opcode (0xEB + dist or 0xE9 + dist lower than
		 * 0x200). It also (almost?) always has 512 bytes per
		 * sector (@0xB), has 2 FATs (@0x10) and a type (@0x15)
		 * in F0..FF. Otherwise it's just an MBR.
		 */
		if ((buf[0] == 0xeb || (buf[0] == 0xE9 && buf[2] <= 0x1)) && // short/near jump
		    (buf[0xb] == 0 && buf[0xc] == 2) && // 512 bytes per sector
		    (buf[0x10] == 2) && // 2 FATs
		    (unsigned char)(buf[0x15] - 0xf0) <= 0x0F)
			return "vfat";
		else
			return "mbr";
	}

	/* not found */
	return NULL;
}

/* try to detect and fill the FS type of this partition. We won't try to create
 * the devices, they are expected to already be there.
 */
static void blkdev_find_part_type(struct blkdev *part)
{
	const char *type;
	char *path;

	if (part->flags & BLKD_F_DETECTED)
		goto leave; // detection already done

	if (part->size < 1024)
		goto leave; // less than 1MB is not for us

	path = addcst(tmp_path, "/dev/");
	path = addcst(path, part->name);
	type = find_fs_type(tmp_path);
	if (type) {
		my_strlcpy(part->type, type, sizeof(part->type));
		part->flags |= BLKD_F_DETECTED;
	}
 leave:
	return;
}

int main(int argc, char **argv, char **envp)
{
	int old_umask;
	int pid1, err;
	int cfg_ok;
	int rebuild;
	int single_cmd = 0;
	int token;
	int cmd_input = INPUT_FILE;
	char cmd_line[256]; /* one line of config from the prompt */
	struct stat statf;
	//    char *cmdline_arg;
	char *cfg_file;
	char *ret_msg = NULL;
	int brace_level, kbd_level, run_level;
	int env_bytes, env_entries;
	char *conf_init, *force_init;
	char missing_arg_msg[] = "0 args needed\n";
	struct {
		int error;
		int cond;
	} context[MAX_BRACE_LEVEL];

	/* first, we'll check if we have been called as 'linuxrc', used only in
	   initrd. In this particular case, we ignore all arguments and use
	   /.linuxrc as a configuration file.
	*/

	/* if we are called as "linuxrc" or "/linuxrc", then we work a bit
	 * differently : /dev is unmounted at the end, and we proceed even if the
	 * pid is not 1.
	 *
	 * Warning: for an unknown reason (isolinux, kernel?) specifying
	 * 'init=/linuxrc' or 'init=linuxrc' on the command line doesn't work
	 * because 'init' is set in argv[0] ! But setting it by hand works,
	 * so does 'init=/.linuxrc'. Rather strange. So the best solution is
	 * to link /sbin/init to /.linuxrc so that the interpreter's name is
	 * used even if nothing is specified.
	 */
	if (streq(argv[0], "linuxrc") || streq(argv[0], "/linuxrc")) {
		linuxrc = 1;
		rebuild = 0;
		pid1 = 0;
	}
	/* check if a config file name has been given to preinit : init [ \< <cfg_file> ] [ init args ] */
	if (argc > 2 && *argv[1] == '<') {
		cfg_file = argv[2];
		argv[2] = argv[0];
		argv += 2;
		argc -= 2;
	}
	else
		cfg_file = linuxrc ? "/.linuxrc" : "/.preinit";

	if (!linuxrc) {
		/* restore the correct name. Warning: in the case where init is launched
		 * from userspace, the config file is not read again so only the hardcoded
		 * name will be used for the executable name.
		 */
		conf_init = "/sbin/init-sysv";
		force_init = my_getenv(envp, "INIT=", 1);

		/* if "rebuild" is passed as the only argument, then we'll try to rebuild a
		 * full /dev even if not pid==1, but only if it was not already populated
		 */
		rebuild = (argc == 2 && streq(argv[1], "rebuild"));
		pid1 = (getpid() == 1);
	}
	else {
		conf_init = NULL;
		force_init = my_getenv(envp, "init2=", 1);
	}

	if (pid1 || linuxrc) {
		setsid();
	}

	old_umask = umask(0);
	cfg_ok = 0;

	/* do nothing if we're not called as the first process */
	if (pid1 || linuxrc || rebuild) {
		cfg_ok = (read_cfg(cfg_file) == 0);
		chdir("/"); /* be sure not to stay under /dev ! */

		/* check if /dev is already populated : /dev/console should exist. We
		 * can safely ignore and overwrite /dev in case of linuxrc, reason why
		 * we don't test the presence of /dev/console.
		 */
		if (linuxrc || !is_dev_populated()) {
			debug("init/info: /dev/console not found, rebuilding /dev.\n");
			if (mount("/dev", "/dev", "devtmpfs", MS_MGC_VAL, "size=4k,nr_inodes=4096,mode=755") == -1 &&
			    mount("/dev", "/dev", "tmpfs",    MS_MGC_VAL, "size=4k,nr_inodes=4096,mode=755") == -1) {
				debug("init/err: cannot mount /dev.\n");
			}
			else {
				int i;
				if (chdir("/dev") == -1)
					debug("init/error : cannot chdir(/dev)\n");

				debug("init/info: /dev has been mounted.\n");
				for (i = 0; i < sizeof(dev_nodes) / sizeof(dev_nodes[0]); i++) {
					mknod_chown(dev_nodes[i].mode, UID_ROOT, dev_nodes[i].gid,
						    dev_nodes[i].major, dev_nodes[i].minor, (char *)dev_nodes[i].name);
				}
				symlink("/proc/self/fd", "fd");
				debug("init/info: /dev has been rebuilt.\n");

				chdir("/");
				/* if /dev was empty, we may not have had /dev/console, so the
				 * kernel couldn't bind us to it. So let's attach to it now.
				 */
				reopen_console();
			}
		} else if (!pid1) {
			/* we don't want to rebuild anything else if pid is not 1 */
			return 0;
		}

		/* if /dev/root is non-existent, we'll try to make it now */

		if (stat("/dev/root", &statf) == -1) {
			debug("init/info : /dev/root does not exist. Rebuilding...\n");
			if (stat("/", &statf) == 0) {
				if (mknod("/dev/root", 0600 | S_IFBLK, statf.st_dev) == -1) {
					debug("init/error : mknod(/dev/root) failed\n");
				}
			}
			else {
				debug("init/error : cannot stat(/)\n");
			}
		}
	}
	else {
		/* We weren't called as linuxrc, pid1 nor rebuild. If passed
		 * "-z" followed by at least one word, it will be processed as
		 * a single command to be executed as if read from a file.
		 */
		if (argc > 2 && streq(argv[1], "-z")) {
			int i;

			cfg_line = cfg_data;
			for (i = 2; i < argc; i++) {
				cfg_line += my_strlcpy(cfg_line, argv[i], cfg_data + sizeof(cfg_data) - cfg_line);
				if (cfg_line + 1 < cfg_data + sizeof(cfg_data))
					*(cfg_line++) = i + 1 < argc ? ' ' : '\n';
				*cfg_line = 0;
			}
			/* now the config is made of this unique line */
			cfg_line = cfg_data;
			cfg_ok = 1;
			single_cmd = 1;
		}
	}

	/* here, the cwd is still "/" */
	if (cfg_ok) {
		debug("ready to parse file : "); debug(cfg_file); debug("\n");

		run_level = brace_level = 0;
		context[brace_level].error = 0;
		context[brace_level].cond = 0;

		/* main parsing loop */
		while (1) {
			int cond = 0;

			if (cmd_input == INPUT_KBD) {
				int len;
				char *cmd_ptr = cmd_line;
				char prompt[sizeof(cmd_line) + MAX_BRACE_LEVEL + 16];
				char *p = prompt;
				int lev1, lev2;

				if (ret_msg)
					p += my_strlcpy(p, ret_msg, sizeof(cmd_line));

				ret_msg = NULL;
				p = addcst(p, error ? "ER (" : "OK\n>");
				if (error) {
					const char *errname;

					p = adduint(p, error_num);
					errname = find_errstr(error_num);
					if (errname) {
						p = addcst(p, " ");
						p = addcst(p, errname);
					}
					p = addcst(p, ")\n>");
				}

				lev1 = run_level;
				lev2 = brace_level;
				while (lev1) {
					*p++ = '>';
					lev1--;
					lev2--;
				}
				if (lev2) {
					*p++ = '{';
					while (lev2--)
						*p++ = '>';
					*p++ = '}';
				}
				*p++ = ' ';
				printn(prompt, p-prompt);

				len = read(0,  cmd_line, sizeof(cmd_line)-1);
				if (len > 0) {
					cmd_line[len] = 0;
					token = parse_cfg(&cmd_ptr, cmd_line + sizeof(cmd_line), envp);
					if (token == TOK_EOF) /* empty line */
						continue;
				} else {
					token = TOK_DOT;
				}

				if (token == TOK_DOT) {
					cmd_input = INPUT_FILE;
					brace_level = kbd_level;
					/* if it was not right, 'rd' would not have been called : */
					run_level = brace_level;
					/* don't report prompt errors to the rest of the config ! */
					error = 0;
				}
			} /* end of kbd */

			if (cmd_input == INPUT_FILE) {
				token = parse_cfg(&cfg_line,  cfg_data + sizeof(cfg_data), envp);
				if (token == TOK_EOF || token == TOK_DOT)
					break;
			}

			if (token == TOK_UNK) {
				ret_msg = "?\n";
				continue;
			}

			cond = token & TOK_COND;
			token &= ~TOK_COND;

			if (cfg_args[(int)tokens[token].minargs] == NULL) {
				missing_arg_msg[0] = '0' + tokens[token].minargs;
				ret_msg = missing_arg_msg;
				continue;
			}

			/* now we can reset the error */
			context[brace_level].error = error;
			error = 0;

			if (token == TOK_CB) { /* closing brace */
				if (cond & TOK_COND) {
					debug("Conditions not permitted with a closing brace.\n");
					/* we close the brace anyway, ignoring the condition. */
				} else if (brace_level == 0) {
					debug("Too many closing braces.\n");
					error = context[brace_level].error;
					continue;
				}
				/* transmit the current error to the upper level, and restore the upper
				 * condition to know if we had to negate the result or not.
				 */
				error = context[brace_level--].error;
				cond  = context[brace_level].cond;
				if (run_level > brace_level)
					run_level = brace_level;
				goto finish_cmd;
			}
			else if (token == TOK_OB) { /* opening brace */
				if (brace_level == MAX_BRACE_LEVEL - 1) {
					debug("Too many brace levels.\n");
					error = context[brace_level].error;
					break;
				}

				/* if this block should not be executed because of a particular
				 * condition, we'll mark the new level to be skipped, so that
				 * other braces are correctly counted but not evaluated.
				 */
				if ((run_level == brace_level) &&
				    (!(cond & TOK_COND_OR) || context[brace_level].error) &&
				    (!(cond & TOK_COND_AND) || !context[brace_level].error)) {
					/* save the current condition to properly handle a negation.
					 * The error code has already been set to 0.
					 */
					context[brace_level++].cond = cond;
					run_level = brace_level;
				}
				else {
					/* the braces will not be evaluated because a & or | prevents
					 * it to do so. Since we want to be able to chain these
					 * expressions to do if/then/else with &{}|{}, we'll propagate
					 * the current error code and void the condition.
					 */
					error = context[brace_level].error;
					context[brace_level++].cond = 0;
				}
				continue;
			}

			/* skip conditionnal executions if they cannot change the error status,
			 * as well as blocks of code excluded from the evaluation
			 */
			if (((cond & TOK_COND_OR) && !context[brace_level].error) ||
			    ((cond & TOK_COND_AND) && context[brace_level].error) ||
			    (run_level < brace_level)) {
				error = context[brace_level].error;
				continue;
			}


			/*
			 * Now we begin to parse the 'real' commands. The first set is always available
			 */

			if (token == TOK_IN) {
				/* I | in <path> : specify the path to init.
				 * Problem: what to do with init args ?
				 * We could pass ours, but if init is replaced with a
				 * shell, this one might be called with 'auto'...
				 * So we flush them all.
				 */
				debug("<I>nit : used config name for init\n");

				/* in keyboard mode, specifying init stops any further parsing,
				 * so that the rest of the config file can be skipped.
				 */
				if (cmd_input == INPUT_KBD) {
					force_init = cfg_args[1];
					break;
				} else {
					/* non sense to switch error status when assigning init */
					error = context[brace_level].error;
					conf_init = cfg_args[1];
					continue;
				}
			} else if (token == TOK_TE || token == TOK_EN) {
				/* te <var=val> : compare an environment variable to a value.
				 * In fact, look for the exact assignment in the environment.
				 * The result is OK if found, NOK if not.
				 */
				char **env = envp;

				while (*env) {
					if (token == TOK_EN)
						println(*env);
					else if (streq(*env, cfg_args[1]))
						break;
					env++;
				}
				error = token != TOK_EN && (*env == NULL);
				goto finish_cmd;
			} else if (token == TOK_TN) {
				/* tn ${string} : test if expands to non-empty string.
				 * The result is OK if non-empty, NOK if not.
				 */
				error = !(cfg_args[1] && *cfg_args[1]);
				goto finish_cmd;
			} else if (token == TOK_SE) {
				/* se name [value]: setenv name [value].
				 * first we need to remove an hypothetical occurrence of the variable,
				 * then duplicate the environment in the stack and append the new
				 * variable.
				 */
				my_getenv(envp, cfg_args[1], 1);
				if (cfg_args[1] && cfg_args[2]) {
					int name_len  = my_strlen(cfg_args[1]);
					int value_len = my_strlen(cfg_args[2]);
					char *env_store, **new_envp;

					env_size(envp, &env_bytes, &env_entries);
					new_envp   = alloca((env_entries + 1) * sizeof(*new_envp));
					env_store  = alloca(env_bytes + name_len + value_len + 2); // name=val\0
					env_append_var(env_dup(new_envp, envp, &env_store),
						       &env_store, cfg_args[1], cfg_args[2]);

					envp = new_envp;
				}
			} else if (token == TOK_EQ) {
				/* eq str1 str2 : compare two strings (possibly containing variables).
				 * The result is OK if identical, otherwise NOK.
				 */
				error = !streq(cfg_args[1], cfg_args[2]);
				goto finish_cmd;
			} else if (token == TOK_TD) {
				/* td (no arg) : tests if /dev is a devtmpfs and returns OK otherwise NOK. */
				char *res = get_dev_type();

				error = !res || !streq(res, "devtmpfs");
				goto finish_cmd;
			} else if (token == TOK_SL) {
				/* sl delay[.frac] : sleep for <delay> sec or forever if <0 or 'inf' */
				unsigned long long sec;
				char *delay = cfg_args[1];
				unsigned int digit;
				struct timeval tv, *tv_ptr;
				char *dot;

				tv_ptr = NULL;
				if (!streq(cfg_args[1], "inf") && *(cfg_args[1]) != '-') {
					dot = my_atoull_next(delay, &sec);
					tv.tv_sec = sec;
					tv.tv_usec = 0;
					if (*(dot++) == '.') {
						unsigned int mul = 1000000;
						while ((mul /= 10) && (digit = (unsigned char)*(dot++) - '0') <= 9)
							tv.tv_usec += mul * digit;
					}
					tv_ptr = &tv;
				}
				select(0, NULL, NULL, NULL, tv_ptr);
			} else if (token == TOK_HE) {
				int i;
				const char *help = tokens_help;

				/* ? / help : show supported commands */
				if (!cfg_args[1])
					println("Supported commands:");
				println("# long short args help");
				for (i = 0; i < TOK_OB; i++) {
					if (!cfg_args[1] ||
					    streqlen(cfg_args[1], tokens[i].lcmd, 2) ||
					    (!cfg_args[1][1] && cfg_args[1][0] == tokens[i].scmd)) {
						print("   ");
						printn(tokens[i].lcmd, 2);
						print("    ");
						pchar(tokens[i].scmd ? tokens[i].scmd : '-');
						print("    ");
						pchar(tokens[i].minargs + '0');
						print("   ");
						if (*help)
							print(help);
						pchar('\n');
					}
					help = help + my_strlen(help) + 1;
				}
				goto finish_cmd;
			}

			/* other options are reserved for pid 1/linuxrc/rebuild and prompt mode */
			if (!pid1 && !linuxrc && !rebuild && !single_cmd && cmd_input != INPUT_KBD) {
				debug("Command ignored since pid not 1\n");
				error = context[brace_level].error;
				continue;

			} else if (token == TOK_RD || token == TOK_EC || token == TOK_WK) {
				/* ec <string> : echo a string
				   rd <string> : display message then read commands from the console instead of the file
				   wk <string> <delay> : display message and wait for a key for at most <delay> seconds.
				   returns TRUE if a key is pressed, FALSE otherwise.
				*/

				if (cfg_args[1] != NULL)
					println(cfg_args[1]);

				if (token == TOK_WK) {
					error = !keypressed(my_atoul(cfg_args[2]));
					goto finish_cmd;
				}

				if (token != TOK_RD)
					goto finish_cmd;

				if (cmd_input == INPUT_KBD) {
					ret_msg = "Command ignored, input already bound to console!\n";
				} else {
					ret_msg = "Entering command line mode : enter one command per line, end with '.'\n";
					kbd_level = brace_level;
				}
				error = context[brace_level].error;
				cmd_input = INPUT_KBD;
				continue;
			}

			/* the second command set is available if pid==1 or if "rebuild" is set */
			switch (token) {
			case TOK_MD:
				/* md path [ mode ] :  make a directory */
				if (recursive_mkdir(cfg_args[1], (cfg_args[2] == NULL) ? 0755 : base8_to_ul(cfg_args[2])) == -1) {
					error = 1;
					debug("<D>irectory : mkdir() failed\n");
				}
				goto finish_cmd;
			case TOK_LN:
				/* ln from to : make a symlink */
				if (symlink(cfg_args[1], cfg_args[2]) == -1) {
					error_num = errno;
					error = 1;
					debug("<S>ymlink : symlink() failed\n");
				}
				goto finish_cmd;
			case TOK_FP:    /* fp: find partitions. */
			case TOK_LP: {	/* lp: list partitions. */
				struct blkdev **parent, *spare = NULL;
				struct blkdev *mbr, *part, *part1, *part2;
				unsigned long long major, minor, size;
				char linebuf[256];
				int arg_pos, best_pos = MAX_CFG_ARGS;
				char *dev;
				char *ptr;

				/* "lp -r" rescans the whole table */
				if (token == TOK_LP && cfg_args[1] && cfg_args[1][0] == '-' && cfg_args[1][1] == 'r')
					blkdevs = NULL;

				if (read_from_file("/proc/partitions", parts, sizeof(parts)) == -1) {
					error = 1;
					debug("ListPart: cannot read /proc/partitions\n");
					goto finish_cmd;
				}

				/* input format:
				 * major minor  #blocks  name
				 *
				 *    1        0      16384 ram0
				 *    1        1      16384 ram1
				 */
				for (ptr = parts; *ptr; ) {
					if (!spare)
						spare = alloca(sizeof(*spare));

					/* skip spaces */
					while (*ptr == ' ')
						ptr++;
					if (!*ptr)
						break;
					/* skip empty lines */
					if (*ptr == '\n') {
						ptr++;
						continue;
					}
					/* skip title */
					if ((unsigned char)(*ptr - '0') > 9) {
						ptr++;
						continue;
					}

					/* OK the line starts with a digit, it interests us */
					ptr = my_atoull_next(ptr, &major);
					while (*ptr == ' ')
						ptr++;

					ptr = my_atoull_next(ptr, &minor);
					while (*ptr == ' ')
						ptr++;

					ptr = my_atoull_next(ptr, &size);
					while (*ptr == ' ')
						ptr++;

					dev = ptr;
					while (*ptr && *ptr != '\n')
						ptr++;
					if (*ptr)
						*(ptr++) = 0;
					/* OK, ptr now points to the next line and we
					 * have all our elements in major/minor/size/dev.
					 */
					//printf("major=%d minor=%d size=%u dev=%s\n", (int)major, (int)minor, (int)size, dev);
					parent = blkdev_find_parent(&blkdevs, dev);
					if (!parent)
						continue; // already there
					/* the parent is the start of a sub-tree where to attach
					 * that new device, let's fill the struct and attach it.
					 */
					parent = blkdev_find_part(parent, dev);

					/* if the parent doesn't point to NULL, it points to that
					 * device, hence it's already there.
					 */
					if (*parent)
						continue;

					/* it was not there, let's add it */
					spare->next = spare->part = NULL;
					spare->major = major;
					spare->minor = minor;
					spare->size = size;
					spare->name = alloca(ptr - dev + 1);
					addcst(spare->name, dev);
					spare->flags = 0;
					spare->type[0] = 0;
					*parent = spare;
					spare = NULL;
				}

				if (token == TOK_LP) {
					/* only list devices that are or contain a partition */
					for (mbr = blkdevs; mbr; mbr = mbr->next) {
						if (!mbr->part)
							continue;
						//printf("%s [%llu MB] %u:%u\n", mbr->name, mbr->size / 1024, mbr->major, mbr->minor);
						ptr = linebuf;
						ptr = addcst(ptr, mbr->name);
						ptr = addchr(ptr, ' ');
						ptr = addchr(ptr, '[');
						ptr = adduint(ptr, mbr->size / 1024);
						ptr = addcst(ptr, " MB]\n");
						print(linebuf);

						for (part = mbr->part; part; part = part->next) {
							blkdev_find_part_type(part);
							//printf("%c- %s [%llu MB] %u:%u\n",
							//       (part->next) ? '|' : '`',
							//       part->name, part->size / 1024, part->major, part->minor);
							ptr = linebuf;
							ptr = addchr(ptr, part->next ? '|' : '`');
							ptr = addchr(ptr, '-');
							ptr = addchr(ptr, ' ');
							ptr = addcst(ptr, part->name);
							ptr = addchr(ptr, ' ');
							ptr = addchr(ptr, '[');
							ptr = adduint(ptr, part->size / 1024);
							ptr = addcst(ptr, " MB]");
							ptr = addchr(ptr, ' ');
							ptr = addcst(ptr, part->type);
							println(linebuf);
						}
					}
					goto finish_cmd;
				}

				/* This is the "fp" command. It takes an image name
				 * (possibly empty, defaulting to active), and an
				 * optional list of FS types to limit to. We want to
				 * find a device that has two consecutive parts of
				 * the exact same size at at least 16 MB, located
				 * somewhere after at least one ext2 partition of
				 * at least 8MB (/flash). One of the partitions must
				 * be of one of the specified types (defaulting to
				 * squashfs). We'll first eliminate devices whose
				 * partition layout doesn't match, then check the
				 * FS types for remaining candidates, then pick the
				 * first device in the table (since they are in
				 * detection order). "best_pos" equals 0 when the
				 * first (or default) choice is found.
				 */
				part = part1 = part2 = NULL;
				for (mbr = blkdevs; mbr && best_pos; mbr = mbr->next) {
					if (!mbr->part || (mbr->flags & BLKD_F_IGNORE))
						continue;

					for (part = mbr->part; part && best_pos; part = part->next) {
						if (part->size < 8192)
							continue;
						/* we have one 8MB part, check what follows */
						for (part1 = part->next; part1 && best_pos; part1 = part1->next) {
							if (part1->size < 16384)
								continue;
							part2 = part1->next;
							if (!part2 || part2->size < 16384)
								continue;
							if (part1->size != part2->size)
								continue;
							/* here we have one 8+MB part followed by two
							 * equally sized parts or at least 16MB, that's
							 * a good candidate.
							 */
							blkdev_find_part_type(part);
							blkdev_find_part_type(part1);
							blkdev_find_part_type(part2);
							if (!streqlen(part->type, "ext", 3))
								continue;
							part->flags |= BLKD_F_FLASH; // part may be /flash
							arg_pos = 0; // anticipate no args
							if (cfg_args[2] && *cfg_args[2]) {
								/* one or multiple FS type(s) were listed, only compare against these ones */
								for (arg_pos = 0; cfg_args[arg_pos + 2]; arg_pos++) {
									if (!streq(part1->type, cfg_args[arg_pos + 2]) &&
									    !streq(part2->type, cfg_args[arg_pos + 2]))
										continue;

									part1->flags |= BLKD_F_IMAGE1;
									part1->pref = arg_pos;
									part2->flags |= BLKD_F_IMAGE2;
									part2->pref = arg_pos;
									if (arg_pos < best_pos)
										best_pos = arg_pos;
									break;
								}
							}
							else if (streq(part1->type, "squashfs") || streq(part2->type, "squashfs")) {
								/* default type is squashfs */
								part1->flags |= BLKD_F_IMAGE1;
								part1->pref = arg_pos;
								part2->flags |= BLKD_F_IMAGE2;
								part2->pref = arg_pos;
								if (arg_pos < best_pos)
									best_pos = arg_pos;
							}
						}
					}
				}

				/* Now we've scanned all candidate partitions. If we've
				 * found any, as reflected by best_pos being lower than
				 * the max number of choices, we'll check which ones we've
				 * marked as image1/image2 with that scrore, and based on
				 * cfg_args[1], we'll set chosen_part or second_part to
				 * point to one or the other. For now part1/part2 are
				 * properly set.
				 */
				if (best_pos < MAX_CFG_ARGS) {
					char *env_store, **new_envp, **env_tail;
					int prev_env_bytes, prev_entries;
					int alloc_sz;

					part = part1 = part2 = NULL;
					for (mbr = blkdevs; mbr && (!part1 || !part2); mbr = mbr->next) {
						if (!mbr->part || (mbr->flags & BLKD_F_IGNORE))
							continue;

						for (part = mbr->part; part && (!part1 || !part2); part = part->next) {
							if (part->pref != best_pos)
								continue;
							if (!part1 && (part->flags & BLKD_F_IMAGE1))
								part1 = part;
							else if (!part2 && (part->flags & BLKD_F_IMAGE2))
								part2 = part;
						}
					}
					/* Now part1 and part2 are set.
					 * By convention we'll use part1 as chosen and part2 as second,
					 * se we need to swap them if img==backup/image2.
					 */
					if (streq(cfg_args[1], "backup") || streq(cfg_args[1], "image2")) {
						part = part1;
						part1 = part2;
						part2 = part;
					}

					//printf("setting chosen=%s(%s) second=%s(%s)\n", part1->name, part1->type, part2->name, part2->type);
					/* Here by construction we cannot have part1 or part2 NULL.
					 * We'll have to set chosen_part and second_part based on these
					 * names, in the environment. We know that we'll set both
					 * variables anyway so we can compute by how much we have to
					 * extend the environment (2*"/dev/" + each part name + vars
					 * names + "=" + "\0").
					 */
					alloc_sz  =
						12 /* "chosen_part=" */ + 12 /* "second_part=" */ +
						12 /* "chosen_type=" */ + 12 /* "second_type=" */ +
						2 * (5 /* "/dev/" */ + 5 /* "/dev/" */ + 1 /* \0 */) +
						2 /* \0 for types */ +
						my_strlen(part1->name) + my_strlen(part2->name) +
						my_strlen(part1->type) + my_strlen(part2->type);

					/* check how much was allocated before dropping vars */
					env_size(envp, &prev_env_bytes, &prev_entries);
					my_getenv(envp, "chosen_part", 1);
					my_getenv(envp, "chosen_type", 1);
					my_getenv(envp, "second_part", 1);
					my_getenv(envp, "second_type", 1);
					env_size(envp, &env_bytes, &env_entries);
					env_tail = env_get_tail(envp, &env_store);

					if (env_bytes + alloc_sz > prev_env_bytes ||
					    env_entries + 4 > prev_entries ||
					    !env_store) {
						/* need to expand the environment */
						new_envp  = alloca((env_entries + 4) * sizeof(*new_envp));
						env_store = alloca(env_bytes + alloc_sz);
						env_tail  = env_dup(new_envp, envp, &env_store);
						envp = new_envp;
					}

					addcst(addcst(tmp_path, "/dev/"), part1->name);
					env_tail = env_append_var(env_tail, &env_store, "chosen_part", tmp_path);
					env_tail = env_append_var(env_tail, &env_store, "chosen_type", part1->type);

					addcst(addcst(tmp_path, "/dev/"), part2->name);
					env_tail = env_append_var(env_tail, &env_store, "second_part", tmp_path);
					env_tail = env_append_var(env_tail, &env_store, "second_type", part2->type);
				}
				goto finish_cmd;
			}
			case TOK_ML:
				println("Module Size Used by State Address");
				token = TOK_CA;
				cfg_args[1] = "/proc/modules";
				cfg_args[2] = NULL;
				/* fall through TOK_CA to show modules */
			case TOK_CA:
			case TOK_CP: {
				/* ca <src> : cat a file to stdout */
				/* cp <src> <dst> : copy a file and its mode */
				char buffer[4096];
				int src, dst;
				int len, err;
				struct stat stat_buf;

				err = 1;
				if (stat(cfg_args[1], &stat_buf) == -1) {
					error_num = errno;
					goto stat_err;
				}

				src = open(cfg_args[1], O_RDONLY, 0);
				error_num = errno;
				if (src < 0)
					goto open_err_src;

				if (token == TOK_CP) {
					dst = open(cfg_args[2], O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE, stat_buf.st_mode);
					error_num = errno;
					if (dst < 0)
						goto open_err_dst;
				} else {
					// TOK_CA: output to stdout
					dst = 1;
				}

				while (1) {
					len = read(src, buffer, sizeof(buffer));
					if (len == 0)
						break;
					error_num = errno;
					if (len < 0)
						goto read_err;
					if (write(dst, buffer, len) < 0) {
						error_num = errno;
						goto write_err;
					}
				}
				err = 0;
				read_err:
				write_err:
				if (dst != 1)
					close(dst);
				open_err_dst:
				close(src);
				open_err_src:
				stat_err:
				error = err;
				goto finish_cmd;
			}
			case TOK_ST: {
				/* st file : return error if file does not exist */
				struct stat stat_buf;

				if (stat(cfg_args[1], &stat_buf) == -1) {
					error_num = errno;
					error = 1;
				}
				goto finish_cmd;
			}
			case TOK_RF: {
				/* rf [file|dir]... : random feed from files/dirs */
				feed_random_from_path(cfg_args + 1);
				goto finish_cmd;
			}
			case TOK_RM: {
				/* rm file... : unlink file or links */
				int arg = 1;

				while (cfg_args[arg]) {
					if (unlink(cfg_args[arg]) == -1) {
						error_num = errno;
						error = 1;
						debug("Unlink : unlink() failed\n");
					}
					arg++;
				}
				goto finish_cmd;
			}
			case TOK_BL:
				/* bl <mode> <uid> <gid> <major> <minor> <naming rule> : build a block device */
			case TOK_CH:
				/* ch <mode> <uid> <gid> <major> <minor> <naming rule> : build a character device */
				if (chdir("/dev") == -1) {
					error_num = errno;
					debug("<B>lock_dev/<C>har_dev : cannot chdir(/dev)\n");
					error = 1;
					goto finish_cmd;
				}

				multidev(base8_to_ul(cfg_args[1]) | ((token == TOK_BL) ? S_IFBLK : S_IFCHR),
					 my_atoul(cfg_args[2]), my_atoul(cfg_args[3]),
					 my_atoul(cfg_args[4]), my_atoul(cfg_args[5]), cfg_args[6]);
				chdir("/");
				goto finish_cmd;
			case TOK_FI:
				/* F <mode> <uid> <gid> <name> : build a fifo */
				if (chdir("/dev") == -1) {
					error_num = errno;
					debug("<F>ifo : cannot chdir(/dev)\n");
					error = 1;
					goto finish_cmd;
				}

				error = mknod_chown(base8_to_ul(cfg_args[1]) | S_IFIFO,
						    my_atoul(cfg_args[2]), my_atoul(cfg_args[3]),
						    0, 0, cfg_args[4]);
				chdir("/");
				goto finish_cmd;
			} /* end of switch() */

			/* other options are reserved for pid 1/linuxrc/prompt mode only */
			if (!pid1 && !linuxrc && !single_cmd && cmd_input != INPUT_KBD) {
				debug("Command ignored since pid not 1\n");
				error = context[brace_level].error;
				continue;
			}

			switch (token) {
			case TOK_MT:
			case TOK_RE: {
				/* M dev[(major:minor)] mnt type [ {rw|ro} [ flags ] ]: (re)mount dev on mnt (read-only) */
				char *maj, *min, *end;
				const char *type;
				char *mntdev;
				int mntarg;
		
				/* if the device name doesn't begin with a slash, look for it
				 * in /proc/cmdline
				 */
				mntdev = cfg_args[1];
				maj = mntdev;   /* handles /dev/xxx(maj:min) */

				while (*maj && *maj != '(')
					maj++;

				if (*maj) {
					int imaj = 0, imin = 0;
					dev_t dev;
		    
					*(maj++) = 0;
					min = end = maj;
					while (*min && *min != ':')
						min++;

					if (*min) {
						*(min++) = 0;
						end = min;
					}

					while (*end && *end != ')')
						end++;

					if (*end)
						*end = 0;
					if (*maj)
						imaj = my_atoul(maj);
					if (*min)
						imin = my_atoul(min);

					dev = makedev(imaj, imin);
					if (mknod(mntdev, S_IFBLK|0600, dev) == -1) { /* makes the node as required */
						error_num = errno;
						error = 1;
						debug("<M>ount : mknod() failed\n");
					}
				}
		
				mntarg = MS_RDONLY;
				if (cfg_args[4] != NULL && streq(cfg_args[4], "rw")) {
					debug("<M>ount : 'rw' flag found, mounting read/write\n");
					mntarg &= ~MS_RDONLY;
				}
				else {
					debug("<M>ount : 'rw' flag not found, mounting read only\n");
				}

				if (token == TOK_RE)
					mntarg |= MS_REMOUNT;

				type = cfg_args[3];
				if (streq(type, "auto"))
					type = find_fs_type(mntdev);

				if (!type || mount(mntdev, cfg_args[2], type, MS_MGC_VAL | mntarg, cfg_args[5]) == -1) {
					if (type)
						error_num = errno;
					error = 1;
					if (type != cfg_args[3]) {
						/* FS was set to "auto", let's check /proc/filesystems */
						char fstypes[1024];
						char *ptr, *next;
						int len;

						len = read_from_file("/proc/filesystems", fstypes, sizeof(fstypes));
						if (len < 0)
							fstypes[0] = 0;

						for (ptr = fstypes; *ptr; ptr = next) {
							/* put a zero at the end of the line */
							for (next = ptr; *next; next++) {
								if (*next == '\n') {
									*(next++) = 0;
									break;
								}
							}

							if (*ptr == 'n') // "nodev"
								continue;

							while (*ptr == ' ' || *ptr == '\t')
								ptr++;

							/* now we have an FS type in <ptr> */
							if (mount(mntdev, cfg_args[2], ptr, MS_MGC_VAL | mntarg, cfg_args[5]) != -1) {
								error = 0;
								break; // succeeded
							}
							if (!type) // only update errno if it was not set
								error_num = errno;
						}
					}
					if (error)
						debug("<M>ount : error during mount()\n");
				}
				break;
			}
			case TOK_BR:
				/* br cmd [cfg_args] : execute cmd with cfg_args but do not return */
				/* fall through TOK_EX/TOK_RX and never fork nor exit */
			case TOK_EX:
				/* E cmd [cfg_args] : execute cmd with cfg_args */
				/* fall through TOK_RX */
			case TOK_RX: {
				/* R dir cmd [cfg_args] : execute cmd with cfg_args, chrooted to dir */
				int res;

				res = (token == TOK_BR) ? 0 : fork();
				if (res == 0) {
					char **exec_args;
					/* OK, here's the problem :
					 * PID 1 has its own session ID 0, and we cannot change it.
					 * It cannot either set controlling TTYs and such. So we have
					 * to create a new session. But data written from another session
					 * does get flushed when the session is closed. To solve this
					 * problem, we have to create 2 processes for each fork(), one
					 * to do the real work, and the other one to maintain the session
					 * open, and to wait for the flush (nearly immediate).
					 */

					/* create a session, become the session leader and steal the tty */
					if (setsid() != -1)
						ioctl(0, TIOCSCTTY, (void *)1);

					/* set this terminal to our process group */
					tcsetpgrp(0, getpgrp());
					exec_args = cfg_args + 1;

					if (token == TOK_RX) {
						chroot(*exec_args);
						/* FIXME: no chdir("/") ??? */
						exec_args++;
					}

					/* now we want one pid for the cleaner and another one for the job */
					res = (token == TOK_BR) ? 0 : fork();
					if (res == 0) {
						/* This is the real process. It must be attached to the terminal
						 * and in its own group. This must be done in this exact order.
						 */
						tcsetpgrp(0, getpid());
						setpgid(0, getpid());
						execve(exec_args[0], exec_args, envp);
						debug("<E>xec(child) : execve() failed\n");
						if (token != TOK_BR)
							exit(1);
					}
					else if (res > 0) {
						int ret, rem;
						int status;

						my_strcpy(argv[0], "init: terminal cleaner");

						ret = waitpid(res, &status, 0);

						/* tcdrain() does not appear to work, let's wait
						 * for the terminal to finish writing output data.
						 */
						while (1) {
							if (ioctl(0, TIOCOUTQ, &rem) < 0)
								break;
							if (rem == 0)
								break;
							sched_yield();
						}

						/* forward the error code we got */
						exit(((ret == -1) || !WIFEXITED(status)) ?
						     1 : WEXITSTATUS(status));
					}
					if (token != TOK_BR)
						exit(1);
				}
				else if (res > 0) {
					int ret;

					debug("<E>xec(parent) : waiting for termination\n");
					while (((ret = wait(&error)) != -1) && (ret != res))
						debug("<E>xec(parent) : signal received\n");

					error_num = ((WIFEXITED(error) > 0) ? WEXITSTATUS(error) : 1);
					error = (ret == -1) || error_num;
					debug("<E>xec(parent) : child exited\n");
				}
				else {
					error_num = errno;
					debug("<E>xec : fork() failed\n");
					error = 1;
				}
				break;
			}
#ifdef __NR_finit_module
			case TOK_MP: {
				/* mp [-f] path [args...] : modprobe */
				int flags = 0;
				int skip = 0;
				char buf[4];
				int ret;
				int fd;

				if (cfg_args[1][0] == '-') {
					if (cfg_args[1][1] == 'f')
						flags |= MODULE_INIT_IGNORE_MODVERSIONS;
					skip++;
				}

				fd = open(cfg_args[1 + skip], O_RDONLY, 0);
				if (fd < 0) {
					error_num = errno;
					error = 1;
					debug("ModProbe : open() failed\n");
					break;
				}

				/* a module that doesn't present an ELF header is
				 * likely compressed.
				 */
				if (read(fd, buf, sizeof(buf)) >= sizeof(buf) &&
				    !streqlen(buf, "\x7f""ELF", 4))
					flags |= MODULE_INIT_COMPRESSED_FILE;
				lseek(fd, 0, SEEK_SET);

				ret = my_syscall3(__NR_finit_module, fd, cfg_args[2 + skip] ? cfg_args[2 + skip] : "", flags);
				if (ret != 0) {
					error_num = errno = -ret;
					error = 1;
					debug("ModProbe : finit_module() failed\n");
					break;
				}
				break;
			}
#endif
#ifdef __NR_delete_module
			case TOK_MR: {
				/* mr [-f] name : modrm */
				int flags = 0;
				int skip = 0;
				int ret;

				if (cfg_args[1][0] == '-') {
					if (cfg_args[1][1] == 'f')
						flags |= O_NONBLOCK | O_TRUNC;
					skip++;
				}

				ret = my_syscall2(__NR_delete_module, cfg_args[1 + skip], flags);
				if (ret != 0) {
					error_num = errno = -ret;
					error = 1;
					debug("ModRm : delete_module() failed\n");
					break;
				}
				break;
			}
#endif
			case TOK_MA:
				/* U <umask> : change umask */
				umask(base8_to_ul(cfg_args[1]));
				break;
			case TOK_CD:
				/* cd <new_dir> : change current directory */
				if (chdir(cfg_args[1]) == -1) {
					error_num = errno;
					error = 1;
					debug("cd : error during chdir()\n");
				}
				break;
			case TOK_CR:
				/* cr <new_root> : change root without chdir */
				if (chroot(cfg_args[1]) == -1) {
					error_num = errno;
					error = 1;
					debug("cr (chroot) : error during chroot()\n");
				}
				break;
			case TOK_SW:
				/* sw <new_root> : switch root + reopen console from new root if it exists */
				if (chroot(cfg_args[1]) == -1) {
					error_num = errno;
					error = 1;
					debug("cr (chroot) : error during chroot()\n");
				}
				if (chdir("/") == -1) {
					error_num = errno;
					error = 1;
					debug("cd : error during chdir()\n");
				}
				/* replace stdin/stdout/stderr with newer ones */
				reopen_console();
				break;
			case TOK_PR:
				/* P <new_root> <put_old_relative> : pivot root */
				if (chdir(cfg_args[1]) == -1) {
					error_num = errno;
					error = 1;
					debug("<P>ivot : error during chdir(new root)\n");
				}

				if (pivot_root(".", cfg_args[2]) == -1) {
					error_num = errno;
					error = 1;
					debug("<P>ivot : error during pivot_root()\n");
				}

				chroot(".");
				if (chdir("/") == -1) {
					error_num = errno;
					error = 1;
					debug("<P>ivot : error during chdir(/)\n");
				}

				/* replace stdin/stdout/stderr with newer ones */
				reopen_console();
				break;
			case TOK_MV:
				/* K | mv <fs_dir> <new_dir> :
				 * initially used to keep directory <old_dir> after a pivot, now used to
				 * move a filesystem to another directory.
				 */
				if (mount(cfg_args[1], cfg_args[2], cfg_args[1], MS_MGC_VAL | MS_MOVE, NULL) == 0)
					break;
				/* if it doesn't work, we'll try to cheat with BIND/UMOUNT */
			case TOK_BI:
				/* bi <old_dir> <new_dir> : bind old_dir to new_dir, which means that directory
				 * <new_dir> will show the same contents as <old_dir>.
				 */
				if (mount(cfg_args[1], cfg_args[2], cfg_args[1], MS_MGC_VAL | MS_BIND, NULL) == -1) {
					error_num = errno;
					error = 1;
					debug("<bi> : error during mount|bind\n");
				}
				if (token == TOK_BI)
					break;
				/* fall through umount if we were trying a move */
			case TOK_UM:
				/* um <old_dir> : umount <old_dir> after a pivot. */
				if (umount2(cfg_args[1], MNT_DETACH) == -1) {
					error_num = errno;
					error = 1;
					debug("<um> : error during umount\n");
				}
				break;
			case TOK_LO: {
				/* lo </dev/loopX> <file> : losetup /dev/loopX file */
				struct loop_info loopinfo;
				int lfd, ffd;

				if ((lfd = open(cfg_args[1], O_RDONLY, 0)) < 0) {
					error_num = errno;
					error = 1;
					debug("(l)osetup : error opening loop device\n");
					break;
				}
				if ((ffd = open(cfg_args[2], O_RDONLY, 0)) < 0) {
					error_num = errno;
					error = 1;
					debug("(l)osetup : error opening image\n");
					goto losetup_close_all;
				}
				memset(&loopinfo, 0, sizeof (loopinfo));
				my_strlcpy(loopinfo.lo_name, cfg_args[2], LO_NAME_SIZE);
				if (ioctl(lfd, LOOP_SET_FD, (void *)(long)ffd) < 0) {
					error_num = errno;
					error = 1;
					debug("(l)osetup : error during LOOP_SET_FD\n");
					goto losetup_close_all;
				}
				if (ioctl(lfd, LOOP_SET_STATUS, &loopinfo) < 0) {
					error_num = errno;
					error = 1;
					ioctl(lfd, LOOP_CLR_FD, (void *)0);
					debug("(l)osetup : error during LOOP_SET_STATUS\n");
					goto losetup_close_all;
				}
				losetup_close_all:
				close (lfd); close (ffd);
				break;
			case TOK_TA:
				error = -tar_extract(cfg_args[1], cfg_args[2], cfg_args[3]);
				break;
			case TOK_LS:
				error = -list_dir(cfg_args[1], cfg_args[2]);
				break;
#if defined(__NR_kexec_file_load)
			case TOK_KX: { /* kx: kexec kernel initrd [cmdline*] */
				int retries = 2;
				char *p;
				int kfd, ifd;
				int arg;

				if ((kfd = open(cfg_args[1], O_RDONLY, 0)) < 0) {
					error_num = errno;
					error = 1;
					debug("(kexec : cannot open kernel\n");
					break;
				}

				if (streq(cfg_args[2], "-"))
					ifd = -1; // no initrd provided
				else if ((ifd = open(cfg_args[2], O_RDONLY, 0)) < 0) {
					error_num = errno;
					error = 1;
					debug("(kexec : cannot open initrd\n");
					close(kfd);
					break;
				}

				/* cmdline could be:
				 *   - empty: reuse /proc/cmdline
				 *   - "-" : don't pass anything
				 *    - something: pass this.
				 */

				/* cfg_args[] is made of pointers to cfg_data,
				 * so all of them collectively fit into cfg_data
				 * and may even perfectly match. Let's rebuild
				 * the line from cfg_args[3+] into cfg_args[0].
				 */

				p = cfg_args[0];
				if (!cfg_args[3]) {
					/* Use cmdline, which is already filled for up to
					 * cmdline_len chars, however it may have holes in
					 * the middle that we must turn back to spaces. We
					 * make cfg_args[0] point to it as well, and p point
					 * to the final zero.
					 */
					find_arg(""); // make sure to load cmdline if not already.
					for (p = cmdline; p < cmdline + cmdline_len - 1; p++) {
						if (!*p)
							*p = ' ';
					}
					cfg_args[0] = cmdline;
				} else if (streq(cfg_args[3], "-")) {
					/* do not pass args */
					*p = 0;
				} else {
					/* recompose the line */
					for (arg = 3; cfg_args[arg]; arg++) {
						if (p != cfg_args[0])
							*(p++) = ' ';
						p += my_strlcpy(p, cfg_args[arg], cfg_data + sizeof(cfg_data) - p);
					}
				}

				/* Here we prepare to try again because often the load
				 * fails with EADDRNOTAVAIL on first try and succeeds on
				 * second one.
				 */
				while (retries > 0) {
					if (my_syscall5(__NR_kexec_file_load, kfd, ifd,
					                p - cfg_args[0] + 1, cfg_args[0],
					                KEXEC_ARCH_DEFAULT | (ifd < 0 ? KEXEC_FILE_NO_INITRAMFS : 0)) == 0) {
						error = 0;
						break;
					}
					error_num = errno;
					error = 1;
					debug("(kexec : cannot load files\n");
					if (error_num != EADDRNOTAVAIL)
						break;
					retries--;
				}

				if (ifd >= 0)
					close(ifd);
				close(kfd);

				if (error)
					break;
				/* fall through */
			}
#endif
			case TOK_HA:  /* ha : halt */
			case TOK_PO:  /* po : power off */
			case TOK_RB:  /* rb : reboot */
			case TOK_SP:  /* sp : suspend */
				error = reboot(token == TOK_HA ? LINUX_REBOOT_CMD_HALT :
				               token == TOK_PO ? LINUX_REBOOT_CMD_POWER_OFF :
				               token == TOK_RB ? LINUX_REBOOT_CMD_RESTART :
#if defined(__NR_kexec_file_load)
				               token == TOK_KX ? LINUX_REBOOT_CMD_KEXEC :
#endif
				               /* TOK_SP */      LINUX_REBOOT_CMD_SW_SUSPEND);
				error_num = errno;
				break;
			}
			default:
				debug("unknown cmd in /.preinit\n");
				break;
			}
		finish_cmd:
			if (cond & TOK_COND_NEG)
				error = !error;
		} /* while (1) */
	} else if (pid1 && !linuxrc) {
		debug("init/info : error while opening configuration file : ");
		debug(cfg_file); debug("\n");

		/* /.preinit was not found. In this case, we take default actions :
		 *   - mount /proc
		 *   - mount /var as tmpfs if it's empty and /tmp is a symlink
		 */
		if (mount("/proc", "/proc", "proc", MS_MGC_VAL, NULL) == -1)
			debug("init/err: cannot mount /proc.\n");
		else
			debug("init/info: /proc mounted RW.\n");

		/* we'll see if we want to build /var */
		if ((stat("/var/tmp", &statf) == -1) && /* no /var/tmp */
		    (stat("/tmp", &statf) == 0) && S_ISLNK(statf.st_mode)) { /* and /tmp is a symlink */
			debug("init/info: building /var.\n");
			if (mount("/var", "/var", "tmpfs", MS_MGC_VAL, NULL) == -1)
				debug("init/err: cannot mount /var.\n");
			else {
				debug("init/info: /var has been mounted.\n");
				recursive_mkdir("/var/tmp", 01777);
				recursive_mkdir("/var/run",  0755);
				debug("init/info: /var has been built.\n");
			}
		}
	}
	else {
		debug("init/info : error while opening configuration file : ");
		debug(cfg_file); debug("\n");
	}

	if (single_cmd) {
		debug("end of single_cmd\n");
		return error;
	}

	if (rebuild) {
		debug("end of rebuild\n");
		/* nothing more to do */
		return 0;
	}


	/* We undo all that we can to restore a clean context.
	 * In case the init has been specified by the user at the console prompt,
	 * we don't close 0/1/2 nor dismount /dev, else we wouldn't be able to do
	 * anything.
	 */

	if (force_init != NULL) {
		/* we keep the cmdline args if we take it from the kernel cmdline,
		 * but we flush any args if it comes from the keyboard
		 */
		argv[0] = force_init;
		if (cmd_input == INPUT_KBD)
			argv[1] = NULL;
	} else {
		/* standard conf or new linuxrc : !NULL = chained init */
		/* old linuxrc : NULL = no chained init */
		argv[0] = conf_init;
	}

	if (linuxrc && force_init == NULL) {
		/* normal linuxrc : we close and unmount all what we have done, but not
		 * in case of forced init, because if we force, it should mean that we
		 * want a prompt or something special.
		 */
		close(2); close(1); close(0);
		umount2("/dev", MNT_DETACH);
	}

	umask(old_umask);

	/* the old linuxrc behaviour doesn't exec on exit. */
	if (*argv != NULL) {
		err = execve(*argv, argv, envp);
		debug("init/error : last execve() failed\n");

		/* we'll get a panic there, so let some time for the user to read messages */
		if (pid1)
			sleep(60);
		return err;
	}
	return 0;
}
