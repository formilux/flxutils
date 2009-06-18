/*
  WARNING ! THIS CODE IS OPTIMIZED FOR SIZE WITH COMPILED-IN ARGUMENTS. IT IS
  SUBJECT TO BUFFER OVERFLOWS SO DON'T USE IT WITH RUNTIME ARGUMENTS !!!
*/

/* TODO : 
     - make the config buffer bigger
     - a few security checks (buffer overflows...)
     - cleanup the code a bit
*/

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

  **** needs to rework the doc a bit since it's not up-to-date. ****

  This code tries to build a few filesystem squeleton so that init has enough
  to work correctly :
    - mount -t proc /proc /proc
    - mount -t tmpfs /dev /dev
    - get information from a file : /.preinit which describes what to mount,
      what ramdisks, links and dirs to make :

ln      L source dest
      	make a symlink from <source> to <dest>
md      D path [ mode ]
      	create a directory named <path> with the mode <mode>. If mode is left
	undefined, 0755 is assumed.
mt      M blkdev[(major:minor)] path fstype [ { ro | rw } [ flags ] ]
      	if <major:minor> is specified, create a block device <blkdev>
	with <major> and <minor> and mode 0600.
	mount <blkdev> under <path> with type <fstype>, read-only, except if
	rw is specified, and args <flags>.
in      I path
      	set the next init program to <path>
ex      E cmd [ args ]*
      	execute <cmd> with args <args> and wait for its completion.
rx      R dir cmd [ args ]*
      	chroot to <dir>, execute <cmd> with args <args> and wait for its completion.
bl      B mode uid gid major minor naming_rule
        create a set of block devices
ch      C mode uid gid major minor naming_rule
        create a set of char devices
fi      F mode uid gid name
	create a fifo
ma      U mode
	change umask
pr      P <new_root> <put_old_relative>
        pivot root : old root is displaced into new_root/put_old_relative, and
	new_root is displaced under /.
mv      K <old_dir> <new_dir>
        keep directory <old_dir> after a pivot, then unmount it from old_dir.
	usefull for /dev, /proc, /var ...
um      O <old_dir>
        umount <old_dir> after a pivot for example.
lo      l </dev/loopX> <file>
	losetup /dev/loopX file.
      # anything
      	comment.

    The devices naming rules consist in strings mixed with numbering rules delimited with
    brackets. Each numbering rule has 4 comma-separated fields :
      - type of this string portion : 'c' for a single char, 'i' for an int, 'I' for an int
        for which 0 is not printed, 'h' for an hex digit
      - the range : 
          - chars: any concatenation of character ranges separated with a dash '-': 'a-fk-npq'
	  - ints : either an int or a range composed of 2 ints separated with a dash : '1-16'
	  - hex  : same as int, but with hex digits (case insensitive)
      - the scale : how much to add to the minor device for each step in the range.

    The commands may be prefixed with a '|' or '&', in which case, they will be
    executed only if the previous command failed (|) or succeeded (&).

    Each command argument may reference an environment variable and an optional default
    value. Syntax is similar to the Bourne shell with braces :  ${var} and ${var-default}.
    The default value will be used if the variable is not set, not if it's empty. An unset
    variable with no default value will return an empty string.

    The variables are not evaluated within quotes, but quotes can stop before the '$' sign
    and start again after the brace. 

    Example :
      ln hda3 /dev/disk  => symlinks /dev/disk to hda3
      md /var/tmp 1777	=> creates a directory /var/tmp with mode 1777
      mt /dev/hda1 /boot ext2 => attempts to mount /dev/hda1 read-only under /boot.
      mt /dev/${flash-boot} /boot ${fs-ext2} => attempts to mount /dev/$flash read-only under /boot.
      |mt /dev/hda1(3:1) /boot ext2 => only if the previous command failed, creates /dev/hda1 with major 3, minor 1 and mounts it under /boot
      in /sbin/init-std  => the following init will be this /sbin/init-std (31 chars max)
      ex /sbin/initramdisk /dev/ram6 1200 => executes /sbin/initramdisk with these args and waits for its completion
      bl 0600 0 0 3 1 hd[c,ab,64][i,1-16,1] => makes all hdaX and hdbX with X ranging from 1 to 16
      ch 0600 0 5 2 0 pty[c,p-za-f,16][h,0-f,1] => makes all 256 pty*
      #comment => ignore this line

  For executable reduction reasons, the .preinit file is limited to 4 kB.

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


  The root directory should contain the following dirs :
    - /var (directory) -> can be a ramfs or a real dir on another device
    - /var/tmp (directory) -> idem
    - /tmp (directory) -> idem
    - /etc (directory or symlink) -> several possibilities :
      - directory on the root fs (preferably)
      - directory on another fs
      - ramfs (or ramdisk) and directory extracted from other source
      - symlink to /boot/etc  (in this case, /boot/etc must be a populated directory)
      /etc will already have several symlinks

  /boot (mandatory directory) contains the following :
    - [kernel version] (directory) with vmlinuz, System.map, config and
      the tree behind /lib/modules/[version]
    - current (symlink to kernel version)
    - vmlinuz (symlink to current/vmlinuz)
    - System.map (symlink to current/System.map)
    - config (symlink to current/config)
    - eventually initrd (symlink to current/initrd)
    - modules.dep (symlink to current/modules.dep)
    - modules.pcimap (symlink to current/modules.pcimap)

  /boot *may* contain the following :
    - etc (populated directory)
    - bootfstab (if /etc and /boot/etc empty or non-existant)
             
  This helps in making /lib/modules : it will simply be a symlink to
  /boot/current which will contain all modules.


wrong for now:
  /tmp will always be linked to fs/var/tmp.
  /var will always be linked to fs/var.
  /fs must contain a description file for it (what to mount where, what to
  initialize) so that init has a working filesystem hierarchy and fstab works.
  /fs should be FAT-compatible, at least for its root structure.
*/

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/loop.h>

#ifndef MNT_DETACH
#define MNT_DETACH	2
#endif

#ifndef MS_MOVE
#define MS_MOVE		8192
#endif

#define STR_SECT ".rodata"

#ifdef DEBUG
static void print(char *c) {
    char *p = c;
    while (*p)
        p++;
    write(0, c, p-c);
}
#else
#define print(a,...)	do{}while(0)
#endif

//#define ATOL(x)		atol(x)
#define ATOL(x)		my_atoul(x)

typedef unsigned char uchar;


/* this ordering is awful but it's the most efficient regarding space wasted in
 * long strings alignment with gcc-2.95.3 (gcc 3.2.3 doesn't try to align long
 * strings).
 */
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) msg_ent_console[] = "Entering command line mode : enter one command per line, end with '.'\n";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) root_dir[]  = "/";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) cur_dir[]  = ".";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) dev_name[]  = "/dev";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) var_dir[]   = "/var";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) cfg_fname[] = "/.preinit";	   /* configuration file */
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) msg_err_console[] = "Command ignored, input already bound to console !\n";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) dev_console[] = "dev/console";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) str_rebuild[] = "rebuild";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) var_tmp[]   = "/var/tmp";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) var_run[]   = "/var/run";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) proc_self_fd[]   = "/proc/self/fd";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) proc_cmdline[] = "/proc/cmdline";
//static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) sbin_init[] = "sbin/init";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) sbin_init_sysv[] = "sbin/init-sysv";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) cfg_linuxrc[] = "/.linuxrc";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) dev_options[] = "size=0,nr_inodes=4096,mode=755";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) str__linuxrc[] = "/linuxrc";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) proc_dir[]  = "/proc";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) tmpfs_fs[]  = "tmpfs";
static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) dev_root[]  = "dev/root";


#define tmp_name	(var_tmp + 4)		// static const char tmp_name[]  = "/tmp";
#define proc_fs		(proc_dir+1)		// static const char proc_fs[]  = "proc";
#define fd_dir		(proc_self_fd + 11)	// static const char fd_dir[]  = "fd";
#define str_linuxrc	(str__linuxrc+1)	// "linuxrc"


#define CONST_STR(x)  ({ static const char __attribute__ ((__section__(STR_SECT),__aligned__(1))) ___str___[]=x; (char *)___str___; })

/* used by naming rules */
#define MAX_FIELDS	8
#define MAX_DEVNAME_LEN	64
#define MAX_CFG_SIZE	16384
#define	MAX_CFG_ARGS	64
#define MAX_CMDLINE_LEN 512
#define MAX_BRACE_LEVEL	10

struct dev_varstr {
    char type;
    union {
	struct {
	    char *set;
	    char *ptr;
	    char value; /* value to be printed */
	    uchar index; /* index in the set */
	} chr;
	struct {
	    uchar low;
	    uchar high;
	    uchar value;
	} num;
    } u;
    uchar scale;
};

enum {
    TOK_LN = 0,	/* ln : make a symlink */
    TOK_MD,	/* md : mkdir */
    TOK_MT,	/* mt : mount */
    TOK_RE,	/* re : remount */
    TOK_IN,	/* in : set init program */
    TOK_BR,	/* br : branch (=execute without forking) */
    TOK_EX,	/* ex : execute */
    TOK_RX,	/* rx : execute under chroot */
    TOK_BL,	/* bl : make block devices */
    TOK_CH,	/* ch : make char devices */
    TOK_FI,	/* fi : make a fifo */
    TOK_MA,	/* ma : set umask */
    TOK_CA,	/* ca : cat file */
    TOK_CD,	/* cd : chdir */
    TOK_CP,	/* cp : copy file */
    TOK_CR,	/* cr : chroot (without chdir) */
    TOK_SW,	/* sw : switch root = chdir + chroot . + reopen console */
    TOK_PR,	/* pr : pivot root */
    TOK_MV,	/* mv : move a filesystem */
    TOK_BI,	/* bi : bind a directory */
    TOK_UM,	/* um : umount a filesystem */
    TOK_LO,	/* lo : losetup */
    TOK_EC,	/* ec : echo */
    TOK_EQ,	/* eq : compare two strings */
    TOK_TE,	/* te : test an environment variable */
    TOK_RD,	/* rd : read a command from the console */
    TOK_RM,	/* rm : remove files */
    TOK_ST,	/* st : stat file existence */
    TOK_WK,	/* wk : wait key */
    TOK_OB,	/* {  : begin a command block */
    TOK_CB,	/* }  : end a command block */
    TOK_DOT,	/* .  : end of config */
    TOK_UNK,	/* unknown command */
    TOK_EOF,	/* end of file */
    TOK_COND_NEG = 0x20, /* negate the result before evaluation */
    TOK_COND_OR  = 0x40, /* conditionnal OR */
    TOK_COND_AND = 0x80, /* conditionnal AND */
    TOK_COND     = 0xE0, /* any condition */
};

/* counts from TOK_LN to TOK_DOT */
#define NB_TOKENS	32

/* possible states for variable parsing */
enum {
	VAR_NONE = 0,
	VAR_DOLLAR,
	VAR_OBRACE,
	VAR_DEFAULT,
	VAR_CBRACE,
};

/* this contains all two-chars command, 1-char commands, followed by a token
 * number.
 */
static const  __attribute__ ((__section__(STR_SECT),__aligned__(1))) struct {
    char lcmd[2]; /* long form */
    char scmd;	  /* short form */
    char minargs; /* min #args */
} tokens[NB_TOKENS] = {
    "ln", 'L', 2,	/* TOK_LN */
    "md", 'D', 1,	/* TOK_MD */
    "mt", 'M', 3,	/* TOK_MT */
    "re",   0, 3,	/* TOK_RE */
    "in", 'I', 1,	/* TOK_IN */
    "br",   0, 1,	/* TOK_BR */
    "ex", 'E', 1,	/* TOK_EX */
    "rx", 'R', 2,	/* TOK_RX */
    "bl", 'B', 6,	/* TOK_BL */
    "ch", 'C', 6,	/* TOK_CH */
    "fi", 'F', 4,	/* TOK_FI */
    "ma", 'U', 1,	/* TOK_MA */
    "ca",   0, 1,	/* TOK_CA */
    "cd",   0, 1,	/* TOK_CD */
    "cp",   0, 2,	/* TOK_CP */
    "cr",   0, 1,	/* TOK_CR */
    "sw",   0, 1,	/* TOK_SW */
    "pr", 'P', 2,	/* TOK_PR */
    "mv", 'K', 2,	/* TOK_MV */
    "bi", 'K', 2,	/* TOK_BI */
    "um", 'O', 1,	/* TOK_UM */
    "lo", 'l', 2,	/* TOK_LO */
    "ec",   0, 0,	/* TOK_EC */
    "eq",   0, 2,	/* TOK_EQ */
    "te",   0, 1,	/* TOK_TE */
    "rd",   0, 0,	/* TOK_RD */
    "rm",   0, 1,	/* TOK_RM */
    "st",   0, 1,	/* TOK_ST */
    "wk",   0, 2,	/* TOK_WK */
    "{",  '{', 0,	/* TOK_OB */
    "}",  '}', 0,	/* TOK_CB */
    ".",  '.', 0,	/* TOK_DOT : put every command before this one */
};

#define UID_ROOT	0
#define GID_ROOT	0
#define GID_TTY		5
#define GID_KMEM	9

static const __attribute__ ((__section__(STR_SECT),__aligned__(1))) struct {
    char   name[8];
    short  gid;
    char   major, minor;
    mode_t mode;	/* mode + S_IFCHR, S_IFBLK, S_IFIFO */
} dev_nodes[] =  {
    /* console must always be at the first location */
    { "console", GID_TTY,  5, 1, 0600 | S_IFCHR },
    { "mem",     GID_KMEM, 1, 1, 0640 | S_IFCHR },
    { "kmem",    GID_KMEM, 1, 2, 0640 | S_IFCHR },
    { "null",    GID_ROOT, 1, 3, 0666 | S_IFCHR },
    { "port",    GID_KMEM, 1, 4, 0640 | S_IFCHR },
    { "zero",    GID_ROOT, 1, 5, 0666 | S_IFCHR },
    { "full",    GID_ROOT, 1, 7, 0666 | S_IFCHR },
    { "random",  GID_ROOT, 1, 8, 0644 | S_IFCHR },
    { "urandom", GID_ROOT, 1, 9, 0644 | S_IFCHR },
    { "tty0",    GID_TTY,  4, 0, 0600 | S_IFCHR },
    { "tty",     GID_TTY,  5, 0, 0666 | S_IFCHR },
    { "ptmx",    GID_TTY,  5, 2, 0666 | S_IFCHR },
    { "initctl", GID_ROOT, 0, 0, 0600 | S_IFIFO },
};

static char cfg_data[MAX_CFG_SIZE];
static char *cfg_args[MAX_CFG_ARGS];
static char *cfg_line;
static char cmdline[MAX_CMDLINE_LEN];
static int cmdline_len;
static char *cst_str[MAX_FIELDS];
static char *var_str[MAX_FIELDS];
static struct dev_varstr var[MAX_FIELDS];
static int error; /* an error has emerged from last operation */
static int linuxrc; /* non-zero if we were called as 'linuxrc' */

/* the two input modes */
#define INPUT_FILE 0
#define INPUT_KBD  1

static unsigned long my_atoul(const char *s) {
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

static int streq(const char *str1, const char *str2) {
    char c1;
    while ((c1 = *str1) && c1 == *str2) { /* the second test ensures that *str2 != 0 */
	str1++;
	str2++;
    }
    return ((c1 | *str2) == 0);
}

/*
 * copies at most <size-1> chars from <src> to <dst>. Last char is always
 * set to 0, unless <size> is 0. The number of chars copied is returned
 * (excluding the terminating zero).
 * This code has been optimized for size and speed : on x86, it's 45 bytes
 * long, uses only registers, and consumes only 4 cycles per char.
 */
int my_strlcpy(char *dst, const char *src, int size) {
    char *orig = dst;
    if (size) {
	while (--size && (*dst = *src)) {
	    src++; dst++;
	}
	*dst = 0;
    }
    return dst - orig;
}

static void reopen_console() {
	int i, fd, oldfd;

	oldfd = dup2(0, 3);	// keep a valid console on fd 3

	for (i = 0; i < 3; i++)
		close(i);

	fd = open(dev_console, O_RDWR); // fd = 0 (stdin) or -1 (error)
	if (fd < 0)
		dup(oldfd); // restore 0 from old console

	close(oldfd);
	dup(0); // stdout
	dup(0); // stderr

	print("init/info : reopened /dev/console\n");
}

/* reads the kernel command line </proc/cmdline> into memory and searches
 * for the first assignment of the required variable. Return its value
 * (which may be empty) or NULL if not found.
 */
char *find_arg(char *arg) {
    char *a, *c;

    /* read cmdline the first time */
    if (cmdline_len <= 0) {
	int fd;

	if ((fd = open(proc_cmdline, O_RDONLY)) == -1)
	    return NULL;

	cmdline_len = read(fd, cmdline, sizeof(cmdline)-1);
	close(fd);
	if (cmdline_len <= 0)
	    return NULL;
	cmdline[cmdline_len++] = 0;
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

/*
 * looks for the last assignment of variable 'var=' in 'envp'.
 * the value found is returned (or NULL if not found). Note
 * that if the variable name does not end with an '=', then it
 * will be implicitly checked.
 * if 'remove' is not zero, the assignment is removed afterwards.
 * eg: init=my_getenv(envp, "init=", 1);
 */
char *my_getenv(char **envp, char *var, const int remove) {
    int namelen;
    char **last;

    last = NULL;
    namelen = strlen(var);
    if (!namelen || !envp)
	    return NULL;

    while (*envp != NULL) {
	//printf("comparing <%s> against <%s> for %d chars\n", *envp, var, namelen);
	if (strncmp(*envp, var, namelen) == 0 &&
	    ((var[namelen-1] == '=') || envp[0][namelen] == '='))
	    last = envp;
	envp++;
    }
    //printf("found <%s>\n", (last?*last:"null"));

    if (last == NULL) {
	return NULL;
    } else {
	char *ret = (*last) + namelen;
	if (var[namelen-1] != '=')
		ret++;
	if (remove)
	    while (*last != NULL) {
		//printf("moving <%s> over <%s>\n", *(last+1),*last);
		*last = *(last+1);
		last++;
	    }
	//printf("returning <%s>\n", ret);
	return ret;
    }
}

/* reads the configuration file <cfg_file> into memory.
 * returns 0 if OK, -1 if error.
 */
static inline int read_cfg(char *cfg_file) {
    int cfg_fd;
    int cfg_size;

    if (cfg_line == NULL) {
	if (((cfg_fd = open(cfg_file, O_RDONLY)) == -1) ||
	    ((cfg_size = read(cfg_fd, cfg_data, sizeof(cfg_data) - 1)) == -1)) {
	    return -1;
	}
	close(cfg_fd);
	cfg_line = cfg_data;
	cfg_data[cfg_size] = 0;
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
static int parse_cfg(char **cfg_data, char *bufend, char **envp) {
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
	while (c = *cfg_line) {
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
		char *dollar_ptr, *name_beg, *name_end;
		char *brace_end, *def_beg, *def_end, *repl;

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
		    memmove(p - 1, p, cfg_line - p);
		} else {
		    backslash = (*p == '\\');

		    if (*p == '"') {
			memmove(p, p + 1, cfg_line - p - 1);
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
					    memmove(dollar_ptr, brace_end, bufend - brace_end);
					    ofs = brace_end - dollar_ptr;
					    p -= ofs;
					    cfg_line -= ofs;
					    *cfg_data = cfg_line;
				    } else {
					    /* empty string with alternate mode or valid string in replace mode */
					    memmove(dollar_ptr, def_beg, def_end - def_beg);
					    memmove(dollar_ptr + (def_end - def_beg), brace_end, bufend - brace_end);
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
				    int l = strlen(repl);
				    int ofs = l - (brace_end - dollar_ptr); /* <0: reduce, >0: increase */

				    if (ofs + brace_end > bufend) { // too large, truncate the value.
					    l  -= (ofs + brace_end) - bufend;
					    ofs = bufend - brace_end;
				    }

				    memmove(dollar_ptr + l, brace_end, (bufend - brace_end) - ((ofs > 0)?ofs:0));
				    memcpy(dollar_ptr, repl, l);

				    p += ofs;
				    cfg_line += ofs;
				    *cfg_data = cfg_line;
				    //printf("  OK: -->%s<--, l=%d, ofs=%d\n", dollar_ptr, l, ofs);
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

	for (token = 0; token < NB_TOKENS; token++)
	    if ((!cfg_args[0][1] && tokens[token].scmd == cfg_args[0][0]) ||
		(cfg_args[0][1] &&
		 (tokens[token].lcmd[1] == cfg_args[0][1]) &&
		 (tokens[token].lcmd[0] == cfg_args[0][0])))
		    return token | cond;

	return TOK_UNK;
    }
    return TOK_EOF;
}

/* makes a dev entry. continues on error, but reports them. */
static inline int mknod_chown(mode_t mode, uid_t uid, gid_t gid, uchar major, uchar minor, char *name) {
    int error;

    if (mknod(name, mode, makedev(major, minor)) == -1) {
	error = 1;
	print("init/error : mknod("); print(name); print(") failed\n");
    }

    if (chown(name, uid, gid) == -1) {
	error = 1;
	print("init/error : chown("); print(name); print(") failed\n");
    }

    return error;
}

/* breaks a 3-fields, comma-separated string into 3 fields */
static inline int varstr_break(char *str, char *type, char **set, uchar *scale) {
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
    *scale = ATOL(res[2]);
    return 0;
}

/* reads a range from a string of the form "low-high" or "value".
 * Returns 0 if OK, or 1 if error.
 */
static int int_range(char *from, uchar *low, uchar *high) {
    char c;
    *low = 0;
    while ((c = *from) != '\0') {
	if (isdigit(c))
	    *low = *low * 10 + c - '0';
	else if (c == '-') {
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
static int hex_range(char *from, uchar *low, uchar *high) {
    uchar c;
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

	    //	      if (((uchar)(c -= '0') > 9)  /* not a digit */
	    //		  && (((uchar)(c -= 'A' - '0' - 10) < 0xa) || ((uchar)c > 0xf))
	    //		  && (((uchar)(c -= 'a' - 'A') < 0xa) || ((uchar)c > 0xf)))
	    //		  return 1;

	    *low = (*low << 4) + c;
	}
	from++;
    }
    if (low != high) /* high has not been written to */
	*high = *low;

    return 0;
}

static inline char *addcst(char *dest, char *cst) {
    while ((*dest++ = *cst++) != '\0');
    return dest - 1;
}

static inline char *addchr(char *dest, char chr) {
    *dest++ = chr;
    *dest = '\0';
    return dest;
}

static char *addint(char *dest, uchar num) {
    int div;
    char *out = dest;
    for (div = (1<<16) + (10<<8) + (100); div > 0;) {
	int q, r, d;
	d = (unsigned char)div;
	q = num / d; r = num % d;
	div >>= 8;
	if (!div || (out != dest) || q) {
	    *out++ = q + '0';
	}
	num = r;
    }
    *out = '\0';
    return out;
}

static char *addhex(char *dest, uchar num) {
    uchar c;
    if (num > 0x0F) {
	c = num >> 4;
	*dest++ = (c > 9) ? (c - 10 + 'a') : (c + '0');
    }
    c = num & 0x0F;
    *dest++ = (c > 9) ? (c - 10 + 'a') : (c + '0');
    *dest = '\0';
    return dest;
}

/* builds a device name from the current <var> and <cst_str> arrays,
 * and compute the corresponding minor number by adding all the relevant
 * fields' values
 */
static void name_and_minor(char *name, uchar *minor, int fields) {
    uchar min;
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
	    name = addint(name, var[f].u.num.value);
	    goto recalc_int;
	case 'I' :
	    if (var[f].u.num.value)
		name = addint(name, var[f].u.num.value);
	recalc_int:
	    min += (var[f].u.num.value - var[f].u.num.low) * var[f].scale;
	    break;
	case 0 :
	    break;
	default:
	    print("init/conf : field type must be c, h, i or I\n");
	}
    }
    if (minor)
	*minor = min;
}

/* increments the index for variable #f. When it overflows, it goes back to the
 * beginning and a carry is returned for the next variable to be incremented.
 * 0 is returned if no carry is returned, otherwise, 1 is returned.
 */
static int inc_var_index(int f) {
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
static void multidev(mode_t mode, uid_t uid, gid_t gid, uchar major, uchar minor, char *rule) {
    char *p1, *p2;
    int state;
    int i;

    enum {
	ST_STRING = 0,
	ST_CONST
    };

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
	print("var string incomplete\n");
    }

    /* now we must "compile" the variable fields */
    for (i = 0; i < field; i++) {
	memset(&var[i], 0, sizeof(var[i]));
	if (varstr_break(var_str[i], &var[i].type, &var[i].u.chr.set, &var[i].scale)) {
	    //print("empty variable field in string <"); print(var_str[i]); print(">\n");
	    continue;
	}

	switch (var[i].type) {
	case 'c':
	    var[i].u.chr.value = *(var[i].u.chr.ptr = var[i].u.chr.set);
	    var[i].u.chr.index = 0;
	    break;
	case 'h':
	    if (hex_range(var[i].u.chr.set, &var[i].u.num.low, &var[i].u.num.high)) {
		//printf("error in hex range in <%s>\n", var_str[i]);
		print("init/conf : error in hex range\n");
		continue;
	    }
	    var[i].u.num.value = var[i].u.num.low;
	    break;
	case 'i':
	case 'I':
	    if (int_range(var[i].u.chr.set, &var[i].u.num.low, &var[i].u.num.high)) {
		print("init/conf : error in int range\n");
		//printf("error in int range in <%s>\n", var_str[i]);
		continue;
	    }
	    var[i].u.num.value = var[i].u.num.low;
	    break;
	default:
	    // unknown type
	    break;
	}
    }

    while (1) {
	char name[MAX_DEVNAME_LEN];
	uchar minor_offset;
	int f;

	name_and_minor(name, &minor_offset, field);
	// printf("name = %s, minor = %d\n", name, minor + minor_offset);

	mknod_chown(mode, uid, gid, major, minor + minor_offset, name);
	f = 0;
	while (inc_var_index(f) && (f<field))
	    f++;
	
	if (f >= field)
	    break;
    }
}

/* converts an octal permission mode into the mode_t equivalent */
static mode_t a2mode(char *ascii) {
    mode_t m = 0;
    while ((unsigned)(*ascii - '0') < 8) {
	m = (m << 3) | (*ascii - '0');
	ascii++;
    }
    return m;
}

void flush_stdin() {
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
int keypressed(int delay) {
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

int main(int argc, char **argv, char **envp) {
    int old_umask;
    int pid1, err;
    int cfg_ok;
    int rebuild;
    int token;
    int cmd_input = INPUT_FILE;
    static char cmd_line[256]; /* one line of config from the prompt */
    struct stat statf;
    //    char *cmdline_arg;
    char *cfg_file;
    char *ret_msg = NULL;
    int brace_level, kbd_level, run_level;
    struct {
	int error;
	int cond;
    } context[MAX_BRACE_LEVEL];

    char *conf_init, *force_init;
    char missing_arg_msg[] = "0 args needed\n";

    /* first, we'll check if we have been called as 'linuxrc', used only in
       initrd. In this particular case, we ignore all arguments and use
       /.linuxrc as a configuration file.
    */

#ifdef DEBUG
    print("argv[0]: ");  print(argv[0]); print("\n");
    sleep(1);
#endif

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
    if (!strcmp(argv[0], str_linuxrc) || !strcmp(argv[0], str__linuxrc)) {
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
	cfg_file = linuxrc ? (char *)cfg_linuxrc : (char *)cfg_fname;

    if (!linuxrc) {
	/*FIXME*/
	/* restore the correct name. Warning: in the case where init is launched       
	 * from userspace, the config file is not read again so only the hardcoded
	 * name will be used for the executable name.
	 */
	//*argv = (char *)&sbin_init_sysv; /*"sbin/init-sysv"*/;
	conf_init = (char *)&sbin_init_sysv; /*"sbin/init-sysv"*/;
	force_init = my_getenv(envp, CONST_STR("INIT="), 1);
	//printf("force_init=<%s>, INIT_new=%p\n", my_getenv(envp, "INIT=", 0));
	/* if "rebuild" is passed as the only argument, then we'll try to rebuild a
	 * full /dev even if not pid==1, but only if it was not already populated
	 */
	rebuild = (argc == 2 && streq(argv[1], str_rebuild/*"rebuild"*/));
	pid1 = (getpid() == 1);
    }
    else {
	conf_init = NULL;
	force_init = my_getenv(envp, CONST_STR("init2="), 1);
	//*argv = (char *)&sbin_init; /* "sbin/init" */
	//printf("force_init=<%s>, init2_new=%s\n", my_getenv(envp, "init2=", 0));
    }

    if (pid1 || linuxrc) {
	setsid();
    }

    old_umask = umask(0);
    cfg_ok = (read_cfg(cfg_file) == 0);

    chdir(root_dir); /* be sure not to stay under /dev ! */
    /* do nothing if we're not called as the first process */
    if (pid1 || linuxrc || rebuild) {
	/* check if /dev is already populated : /dev/console should exist. We
	 * can safely ignore and overwrite /dev in case of linuxrc, reason why
	 * we don't test the presence of /dev/console.
	 */
#ifndef I_AM_REALLY_DEBUGGING
	if (linuxrc || stat(dev_console, &statf) == -1) {
	    print("init/info: /dev/console not found, rebuilding /dev.\n");
	    if (mount(dev_name, dev_name, tmpfs_fs, MS_MGC_VAL, dev_options) == -1)
		print("init/err: cannot mount /dev.\n");
	    else {
		int i;
		if (chdir(dev_name) == -1)
		    print("init/error : cannot chdir(/dev)\n");

		print("init/info: /dev has been mounted.\n");
		for (i = 0; i < sizeof(dev_nodes) / sizeof(dev_nodes[0]); i++) {
		    mknod_chown(dev_nodes[i].mode, (uid_t)UID_ROOT, (gid_t)dev_nodes[i].gid,
				dev_nodes[i].major, dev_nodes[i].minor, (char *)dev_nodes[i].name);
		} 
		symlink(proc_self_fd, fd_dir);
		print("init/info: /dev has been rebuilt.\n");

		chdir(root_dir);
		/* if /dev was empty, we may not have had /dev/console, so the
		 * kernel couldn't bind us to it. So let's attach to it now.
		 */
		reopen_console();
	    }
	} else if (!pid1) {
	    /* we don't want to rebuild anything else if pid is not 1 */
#ifdef DEBUG
	    print("init/info: /dev is OK.\n");
	    sleep(10);
#endif
	    return 0;
	}
#endif
	/* if /dev/root is non-existent, we'll try to make it now */

	if (stat(dev_root, &statf) == -1) {
	    print("init/info : /dev/root does not exist. Rebuilding...\n");
	    if (stat(root_dir, &statf) == 0) {
		if (mknod(dev_root, 0600 | S_IFBLK, statf.st_dev) == -1) {
		    //error = 1;
		    print("init/error : mknod(/dev/root) failed\n");
		}
	    }
	    else {
		print("init/error : cannot stat(/)\n");
	    }
	}
    }

    /* here, the cwd is still "/" */
    if (cfg_ok) {
	print("ready to parse file : "); print(cfg_file); print("\n");

	run_level = brace_level = 0;
	context[brace_level].error = 0;
	context[brace_level].cond = 0;
	
	/* main parsing loop */
	while (1) {
	    int cond = 0;

	    if (cmd_input == INPUT_KBD) {
		int len;
		char *cmd_ptr = cmd_line;
		static char prompt[MAX_BRACE_LEVEL + 4];
		char *p = prompt;
		int lev1, lev2;

		if (ret_msg) 
		    p += my_strlcpy(p, ret_msg, sizeof(cmd_line));

		ret_msg = NULL;
		p += my_strlcpy(p, error ? CONST_STR("ER\n>") : CONST_STR("OK\n>"), 5);

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
		write(1, prompt, p-prompt);

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
		    print("Conditions not permitted with a closing brace.\n");
		    /* we close the brace anyway, ignoring the condition. */
		} else if (brace_level == 0) {
		    print("Too many closing braces.\n");
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
		    print("Too many brace levels.\n");
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

	    //printf("parsing intruction <%s %s...> at level %d (%d)\n", cfg_args[0], cfg_args[1], brace_level, run_level);
	    /* skip conditionnal executions if they cannot change the error status,
	     * as well as blocks of code excluded from the evaluation
	     */
	    if ((cond & TOK_COND_OR) && !context[brace_level].error ||
		(cond & TOK_COND_AND) && context[brace_level].error ||
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
		print("<I>nit : used config name for init\n");

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
	    } else if (token == TOK_TE) {
		/* te <var=val> : compare an environment variable to a value.
		 * In fact, look for the exact assignment in the environment.
		 * The result is OK if found, NOK if not.
		 */
		char **env = envp;
		while (*env) {
		    //printf("testing <%s> against <%s>\n", cfg_args[1], *env);
		    if (!strcmp(*env, cfg_args[1]))
			break;
		    env++;
		}
		error = (*env == NULL);
		goto finish_cmd;
	    } else if (token == TOK_EQ) {
		/* eq str1 str2 : compare two strings (possibly containing variables).
		 * The result is OK if identical, otherwise NOK.
		 */
		error = (strcmp(cfg_args[1], cfg_args[2]) != 0);
		goto finish_cmd;
	    }

	    /* other options are reserved for pid 1/linuxrc/rebuild and prompt mode */
	    if (!pid1 && !linuxrc && !rebuild && cmd_input != INPUT_KBD) {
		print("Command ignored since pid not 1\n");
		error = context[brace_level].error;
		continue;

	    } else if (token == TOK_RD || token == TOK_EC || token == TOK_WK) {
		/* ec <string> : echo a string 
		   rd <string> : display message then read commands from the console instead of the file
		   wk <string> <delay> : display message and wait for a key for at most <delay> seconds.
		                         returns TRUE if a key is pressed, FALSE otherwise.
		*/
		char *msg;
		int len;

		if (cfg_args[1] != NULL) {
		    len = strlen(cfg_args[1]);
		    cfg_args[1][len] = '\n';
		    write(1, cfg_args[1], len + 1);
		}

		if (token == TOK_WK) {
		    error = !keypressed(ATOL(cfg_args[2]));
		    goto finish_cmd;
		}

		if (token != TOK_RD)
		    goto finish_cmd;

		if (cmd_input == INPUT_KBD) {
		    ret_msg = (char *)msg_err_console;
		} else {
		    ret_msg = (char *)msg_ent_console;
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
		if (mkdir(cfg_args[1], (cfg_args[2] == NULL) ? 0755 : a2mode(cfg_args[2])) == -1) {
		    error = 1;
		    print("<D>irectory : mkdir() failed\n");
		}
		goto finish_cmd;
	    case TOK_LN:
		/* ln from to : make a symlink */
		if (symlink(cfg_args[1], cfg_args[2]) == -1) {
		    error = 1;
		    print("<S>ymlink : symlink() failed\n");
		}
		goto finish_cmd;
	    case TOK_CA:
	    case TOK_CP: {
		    /* ca <src> : cat a file to stdout */
		    /* cp <src> <dst> : copy a file and its mode */
		    char buffer[4096];
		    int src, dst;
		    int len, mode, err;
		    struct stat stat_buf;

		    err = 1;
		    if (stat(cfg_args[1], &stat_buf) == -1)
			    goto stat_err;

		    src = open(cfg_args[1], O_RDONLY);
		    if (src < 0)
			    goto open_err_src;

		    if (token == TOK_CP) {
			    dst = open(cfg_args[2], O_CREAT|O_WRONLY|O_TRUNC|O_LARGEFILE, stat_buf.st_mode);
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
			    if (len < 0)
				    goto read_err;
			    if (write(dst, buffer, len) < 0)
				    goto write_err;
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
		    error = 1;
		}
		goto finish_cmd;
	    }
	    case TOK_RM: {
		/* rm file... : unlink file or links */
		int arg = 1;
		while (cfg_args[arg]) {
		    if (unlink(cfg_args[arg]) == -1) {
			error = 1;
			print("Unlink : unlink() failed\n");
		    }
		    arg++;
		}
		goto finish_cmd;
	    }
	    case TOK_BL:
		/* bl <mode> <uid> <gid> <major> <minor> <naming rule> : build a block device */
	    case TOK_CH:
		/* ch <mode> <uid> <gid> <major> <minor> <naming rule> : build a character device */
		if (chdir(dev_name) == -1) {
		    print("<B>lock_dev/<C>har_dev : cannot chdir(/dev)\n");
		    error = 1;
		    goto finish_cmd;
		}

		multidev(a2mode(cfg_args[1]) | ((token == TOK_BL) ? S_IFBLK : S_IFCHR),
			 (uid_t)ATOL(cfg_args[2]), (gid_t)ATOL(cfg_args[3]),
			 ATOL(cfg_args[4]), ATOL(cfg_args[5]), cfg_args[6]);
		chdir(root_dir);
		goto finish_cmd;
	    case TOK_FI:
		/* F <mode> <uid> <gid> <name> : build a fifo */
		if (chdir(dev_name) == -1) {
		    print("<F>ifo : cannot chdir(/dev)\n");
		    error = 1;
		    goto finish_cmd;
		}

		error = mknod_chown(a2mode(cfg_args[1]) | S_IFIFO,
				    (uid_t)ATOL(cfg_args[2]), (gid_t)ATOL(cfg_args[3]),
				    0, 0, cfg_args[4]);
		chdir(root_dir);
		goto finish_cmd;
	    } /* end of switch() */

	    /* other options are reserved for pid 1/linuxrc/prompt mode only */
	    if (!pid1 && !linuxrc && cmd_input != INPUT_KBD) {
		print("Command ignored since pid not 1\n");
		error = context[brace_level].error;
		continue;
	    }

	    switch (token) {
	    case TOK_MT:
	    case TOK_RE: {
		/* M dev[(major:minor)] mnt type [ {rw|ro} [ flags ] ]: (re)mount dev on mnt (read-only) */
		char *maj, *min, *end;
		char *mntdev;
		int mntarg;
		
		/* if the device name doesn't begin with a slash, look for it
		 * in /proc/cmdline
		 */
		mntdev = cfg_args[1];
		// the following code handles "var=/dev/sda2" or "home=/dev/hda1(3:1)" on
		// the kernel command line.
		//if ((*mntdev != '/') && ((cmdline_arg = find_arg(mntdev)) != NULL)) {
		//    mntdev = cmdline_arg;
		//    print("<M>ount : using command line device\n");
		//}
		
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
			imaj = ATOL(maj);
		    if (*min)
			imin = ATOL(min);
		    dev = makedev(imaj, imin);
		    if (mknod(mntdev, S_IFBLK|0600, dev) == -1) { /* makes the node as required */
			error = 1;
			print("<M>ount : mknod() failed\n");
		    }
		}
		
		mntarg = MS_RDONLY;
		if (cfg_args[4] != NULL && streq(cfg_args[4], CONST_STR("rw"))) {
		    print("<M>ount : 'rw' flag found, mounting read/write\n");
		    mntarg &= ~MS_RDONLY;
		}
		else {
		    print("<M>ount : 'rw' flag not found, mounting read only\n");
		}

		if (token == TOK_RE)
		    mntarg |= MS_REMOUNT;

		if (mount(mntdev, cfg_args[2], cfg_args[3], MS_MGC_VAL | mntarg, cfg_args[5]) == -1) {
		    error = 1;
		    print("<M>ount : error during mount()\n");
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
			    ioctl(0, TIOCSCTTY, 1);

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
			    print("<E>xec(child) : execve() failed\n");
			    //printf("after execve(%s)!\n", exec_args[0]);
			    if (token != TOK_BR)
				    exit(1);
		    }
		    else if (res > 0) {
			    int ret, rem;
			    int status;

			    strcpy(argv[0], "init: terminal cleaner");

			    ret = waitpid(res, &status, 0);

			    /* tcdrain() does not appear to work, let's wait
			     * for the terminal to finish writing output data.
			     */
			    //tcdrain(0);
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
		    print("<E>xec(parent) : waiting for termination\n");
		    while (((ret = wait(&error)) != -1) && (ret != res))
			print("<E>xec(parent) : signal received\n");
		    
		    error = (ret == -1) || ((WIFEXITED(error) > 0) ? WEXITSTATUS(error) : 1);
		    print("<E>xec(parent) : child exited\n");
		}
		else {
		    print("<E>xec : fork() failed\n");
		    error = 1;
		}
		break;
	    }
	    case TOK_MA:
		/* U <umask> : change umask */
		umask(a2mode(cfg_args[1]));
		break;
	    case TOK_CD:
		/* cd <new_dir> : change current directory */
		if (chdir(cfg_args[1]) == -1) {
		    error = 1;
		    print("cd : error during chdir()\n");
		}
		break;
	    case TOK_CR:
		/* cr <new_root> : change root without chdir */
		if (chroot(cfg_args[1]) == -1) {
		    error = 1;
		    print("cr (chroot) : error during chroot()\n");
		}
		break;
	    case TOK_SW:
		/* sw <new_root> : switch root + reopen console from new root if it exists */
		if (chroot(cfg_args[1]) == -1) {
		    error = 1;
		    print("cr (chroot) : error during chroot()\n");
		}
		if (chdir(root_dir) == -1) {
		    error = 1;
		    print("cd : error during chdir()\n");
		}
		/* replace stdin/stdout/stderr with newer ones */
		reopen_console();
		break;
	    case TOK_PR:
		/* P <new_root> <put_old_relative> : pivot root */
		if (chdir(cfg_args[1]) == -1) {
		    error = 1;
		    print("<P>ivot : error during chdir(new root)\n");
		}

		if (pivot_root(/*CONST_STR(".")*/cur_dir, cfg_args[2]) == -1) {
		    error = 1;
		    print("<P>ivot : error during pivot_root()\n");
		}

		chroot(cur_dir/*CONST_STR(".")*/);
		if (chdir(root_dir) == -1) {
		    error = 1;
		    print("<P>ivot : error during chdir(/)\n");
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
		    error = 1;
		    print("<bi> : error during mount|bind\n");
		}
		if (token == TOK_BI)
		    break;
		/* fall through umount if we were trying a move */
	    case TOK_UM:
		/* um <old_dir> : umount <old_dir> after a pivot. */
		if (umount2(cfg_args[1], MNT_DETACH) == -1) {
		    error = 1;
		    print("<um> : error during umount\n");
		}
		break;
	    case TOK_LO: {
		/* lo </dev/loopX> <file> : losetup /dev/loopX file */
		struct loop_info loopinfo;
		int lfd, ffd;

		if ((lfd = open (cfg_args[1], O_RDONLY)) < 0) {
		    error = 1;
		    print("(l)osetup : error opening loop device\n");
		    break;
		}
		if ((ffd = open (cfg_args[2], O_RDONLY)) < 0) {
		    error = 1;
		    print("(l)osetup : error opening image\n");
		    goto losetup_close_all;
		}
		memset(&loopinfo, 0, sizeof (loopinfo));
		my_strlcpy(loopinfo.lo_name, cfg_args[2], LO_NAME_SIZE);
		if (ioctl(lfd, LOOP_SET_FD, ffd) < 0) {
		    error = 1;
		    print("(l)osetup : error during LOOP_SET_FD\n");
		    goto losetup_close_all;
		}
		if (ioctl(lfd, LOOP_SET_STATUS, &loopinfo) < 0) {
		    error = 1;
		    ioctl(lfd, LOOP_CLR_FD, 0);
		    print("(l)osetup : error during LOOP_SET_STATUS\n");
		    goto losetup_close_all;
		}
	    losetup_close_all:
		close (lfd); close (ffd);
		break;
	    }
	    default:
		print("unknown cmd in /.preinit\n");
		break;
	    }
	finish_cmd:
	    if (cond & TOK_COND_NEG)
		error = !error;
	} /* while (1) */
    } else if (pid1 && !linuxrc) {
	print("init/info : error while opening configuration file : ");
	print(cfg_file); print("\n");

	/* /.preinit was not found. In this case, we take default actions :
	 *   - mount /proc
	 *   - mount /var as tmpfs if it's empty and /tmp is a symlink
	 */
	if (mount(proc_dir, proc_dir, proc_fs, MS_MGC_VAL, NULL) == -1)
	    print("init/err: cannot mount /proc.\n");
	else
	    print("init/info: /proc mounted RW.\n");

	/* we'll see if we want to build /var */
	if ((stat(var_tmp, &statf) == -1) && /* no /var/tmp */
	    (stat(tmp_name, &statf) == 0) && S_ISLNK(statf.st_mode)) { /* and /tmp is a symlink */
	    print("init/info: building /var.\n");
	    if (mount(var_dir, var_dir, tmpfs_fs, MS_MGC_VAL, NULL) == -1)
		print("init/err: cannot mount /var.\n");
	    else {
		print("init/info: /var has been mounted.\n");
		mkdir(var_dir, 0755);
		mkdir(var_tmp, 01777);
		mkdir(var_run,  0755);
		print("init/info: /var has been built.\n");
	    }
	}
    }
    else {
	print("init/info : error while opening configuration file : ");
	print(cfg_file); print("\n");
    }

    if (rebuild) {
	print("end of rebuild\n");
#ifdef SLOW_DEBUG
	sleep(10);
#endif
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
	umount2(dev_name, MNT_DETACH);
    }

#if 0

    if (cmd_input != INPUT_KBD) {
	if (linuxrc) {
	    close(2); close(1); close(0);
	    umount2(dev_name, MNT_DETACH);
	    print("exit from linuxrc\n");
#ifdef SLOW_DEBUG
	    sleep(10);
#endif
	    /* handle the lilo command line "init2=prog" */
	    //cmdline_arg = find_arg(CONST_STR("init2"));
	    //cmdline_arg = my_getenv(envp, CONST_STR("init2="), 1);
	} else {
	    /* handle the lilo command line "INIT=prog" */
	    //cmdline_arg = find_arg(CONST_STR("INIT"));
	    cmdline_arg = my_getenv(envp, CONST_STR("INIT="), 1);
	}
	
	if (cmdline_arg != NULL) {
	    argv[0] = cmdline_arg;
	    argv[1] = NULL;
	}
    }

    print("init/debug: *argv = "); print (*argv); print("\n");
#endif

    umask(old_umask);
#ifdef SLOW_DEBUG
    sleep(10);
#endif
    /* the old linuxrc behaviour doesn't exec on exit. */
    if (*argv != NULL) {
	err = execve(*argv, argv, envp);
	print("init/error : last execve() failed\n");

	/* we'll get a panic there, so let some time for the user to read messages */
	if (pid1)
	    sleep(60);
	return err;
    }
    return 0;
}

