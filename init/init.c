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

    Example :
      ln hda3 /dev/disk  => symlinks /dev/disk to hda3
      md /var/tmp 1777	=> creates a directory /var/tmp with mode 1777
      mt /dev/hda1 /boot ext2 => attempts to mount /dev/hda1 read-only under /boot.
      |mt /dev/hda1(3:1) /boot ext2 => only if the previous command failed, creates /dev/hda1 with major 3, minor 1 and mounts it under /boot
      in /sbin/init-std  => the following init will be this /sbin/init-std (31 chars max)
      ex /sbin/initramdisk /dev/ram6 1200 => executes /sbin/initramdisk with these args and waits for its completion
      bl 0600 0 0 3 1 hd[c,ab,64][i,1-16,1] => makes all hdaX and hdbX with X ranging from 1 to 16
      ch 0600 0 5 2 0 pty[c,p-za-f,16][h,0-f,1] => makes all 256 pty*
      #comment => ignore this line

  For executable reduction reasons, the .preinit file is limited to 4 kB.

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

extern char **environ;

static const char cfg_fname[] = "/.preinit";	   /* configuration file */
static const char tmpfs_fs[]  = "tmpfs";
static const char dev_console[] = "dev/console";
static const char dev_root[]  = "dev/root";
static const char dev_name[]  = "/dev";
static const char root_dir[]  = "/";
static const char var_dir[]   = "/var";
static const char var_run[]   = "/var/run";
static const char var_tmp[]   = "/var/tmp";
static const char proc_dir[]  = "/proc";
static const char proc_self_fd[]   = "/proc/self/fd";
static const char proc_cmdline[] = "/proc/cmdline";
static const char sbin_init_sysv[] = "sbin/init-sysv";
static const char str_rebuild[] = "rebuild";
static const char dev_options[] = "size=0,nr_inodes=4096,mode=755";

#define tmp_name  (var_tmp + 4) //static const char tmp_name[]  = "/tmp";
#define proc_fs   (proc_dir+1) //static const char proc_fs[]  = "proc";
#define fd_dir    (proc_self_fd + 11) //static const char fd_dir[]  = "fd";

#define UID_ROOT	0
#define GID_ROOT	0
#define GID_TTY		5
#define GID_KMEM	9

/* used by naming rules */
#define MAX_FIELDS	8
#define MAX_DEVNAME_LEN	64
#define MAX_CFG_SIZE	4096
#define	MAX_CFG_ARGS	16
#define MAX_CMDLINE_LEN 512

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

static const struct {
    char   name[8];
    short  gid;
    char   major, minor;
    mode_t mode;	/* mode + S_IFCHR, S_IFBLK, S_IFIFO */
} dev_nodes[] = {
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

#define NB_TOKENS	15

enum {
    TOK_LN = 0,	/* ln : make a symlink */
    TOK_MD,	/* md : mkdir */
    TOK_MT,	/* mt : mount */
    TOK_IN,	/* in : set init program */
    TOK_EX,	/* ex : execute */
    TOK_RX,	/* rx : execute under chroot */
    TOK_BL,	/* bl : make block devices */
    TOK_CH,	/* ch : make char devices */
    TOK_FI,	/* fi : make a fifo */
    TOK_MA,	/* ma : set umask */
    TOK_PR,	/* pr : pivot root */
    TOK_MV,	/* mv : move */
    TOK_UM,	/* um : umount */
    TOK_LO,	/* lo : losetup */
    TOK_EC,	/* ec : echo */
    TOK_UNK,	/* unknown command */
    TOK_EOF,	/* end of file */
    TOK_COND_OR  = 0x40, /* conditionnal OR */
    TOK_COND_AND = 0x80, /* conditionnal AND */
    TOK_COND     = 0xc0, /* any condition */
};

/* this contains all two-chars command, 1-char commands, followed by a token
 * number.
 */
static const struct {
    char lcmd[2]; /* long form */
    char scmd;	  /* short form */
    char minargs; /* min #args */
} tokens[NB_TOKENS] = {
    "ln", 'L', 2,	/* TOK_LN */
    "md", 'D', 1,	/* TOK_MD */
    "mt", 'M', 3,	/* TOK_MT */
    "in", 'I', 1,	/* TOK_IN */
    "ex", 'E', 1,	/* TOK_EX */
    "rx", 'R', 2,	/* TOK_RX */
    "bl", 'B', 6,	/* TOK_BL */
    "ch", 'C', 6,	/* TOK_CH */
    "fi", 'F', 4,	/* TOK_FI */
    "ma", 'U', 1,	/* TOK_MA */
    "pr", 'P', 2,	/* TOK_PR */
    "mv", 'K', 2,	/* TOK_MV */
    "um", 'O', 1,	/* TOK_UM */
    "lo", 'l', 2,	/* TOK_LO */
    "ec",   0, 0,	/* TOK_EC */
};

static char cfg_data[MAX_CFG_SIZE];
static char *cfg_args[MAX_CFG_ARGS];
static char *cfg_line;
static char cmdline[MAX_CMDLINE_LEN];
static char *cst_str[MAX_FIELDS];
static char *var_str[MAX_FIELDS];
static struct dev_varstr var[MAX_FIELDS];
static int error; /* an error has emerged from last operation */

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
static inline int my_strlcpy(char *dst, const char *src, int size) {
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
    if (!*cmdline) {
	int fd, len;

	if ((fd = open(proc_cmdline, O_RDONLY)) == -1)
	    return NULL;
	if ((len = read(fd, cmdline, sizeof(cmdline)-1)) == -1) {
	    close(fd);
	    return NULL;
	}
	cmdline[len] = 0;
	close(fd);
    }
    
    /* search for the required arg in cmdline */
    c = cmdline;
    a = arg;

    while (*c) {
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
	if (*c == *a) {
	    a++;
	    c++;
	}
	else {
	    c -= (a - arg) - 1;
	    a = arg;
	}
    }
    if (*a == 0) /* complete match at end of string */
	return c; /* pointer to empty string */
    else
	return NULL; /* not found */
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
 */
static int parse_cfg() {
    int nbargs;
    int token;
    int cond;

    memset(cfg_args, 0, sizeof(cfg_args));
    while (*cfg_line) {
	char c, *p = cfg_line;

	/* search beginning of line */
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
	    p++;

	/* now search end of line */
	cfg_line = p;
	while (*cfg_line && *cfg_line != '#' && *cfg_line != '\n' && *cfg_line != '\r')
	    cfg_line++;

	/* terminate the line cleanly to avoid further tests */
	while (c = *cfg_line) {
	    *cfg_line++ = '\0';
	    if (c == '\n')
		break;
	}

	/* skip empty lines */
	if (!*p)
	    continue;
	
	if (*p == '|') {
	    cond = TOK_COND_OR;
	    p++;
	}
	else if (*p == '&') {
	    cond = TOK_COND_AND;
	    p++;
	}
	else
	    cond = 0;

	/* fills the cfg_args[] array with the command itself, followed by all
	 * args.
	 */
	for (nbargs = 0; *p && (nbargs < MAX_CFG_ARGS - 1); nbargs++) {
	    int backslash = 0;
	    cfg_args[nbargs] = p;
	    do {
		if (backslash) {
		    backslash = 0;
		    memmove(p - 1, p, cfg_line - p);
		}
		else {
		    backslash = (*p == '\\');
		    p++;
		}
	    } while (*p && (backslash || (*p != ' ' && *p != '\t')));
	    if (*p) {
		*p = 0;
		do p++; while (*p == ' ' || *p == '\t');
	    }
	}

	/* search a matching token for the command : it can either be a single
	 * char (old language) or a double char (new language)
	 */
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

/* makes a dev entry */
static inline int mknod_chown(mode_t mode, uid_t uid, gid_t gid, uchar major, uchar minor, char *name) {
    if (mknod(name, mode, makedev(major, minor)) == -1) {
	error = 1;
	print("init/error : mknod("); print(name); print(") failed\n");
    }

    if (chown(name, uid, gid) == -1) {
	error = 1;
	print("init/error : chown("); print(name); print(") failed\n");
    }
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

int main(int argc, char **argv) {
    int old_umask;
    int pid1, err;
    int cfg_ok;
    int rebuild;
    int token;
    struct stat statf;
    char *cmdline_arg;
    char *cfg_file;

    /* check if a config file name has been given to preinit : init [ \< <cfg_file> ] [ init args ] */
    if (argc > 2 && *argv[1] == '<') {
	cfg_file = argv[2];
	argv[2] = argv[0];
	argv += 2;
	argc -= 2;
    }
    else
	cfg_file = (char *)cfg_fname;

    /* restore the correct name. Warning: in the case where init is launched       
     * from userspace, the config file is not read again so only the hardcoded
     * name will be used for the executable name.
     */
    *argv = &sbin_init_sysv;/*"sbin/init-sysv"*/;
    old_umask = umask(0);

    /* if "rebuild" is passed as the only argument, then we'll try to rebuild a
     * full /dev even if not pid==1, but only if it was not already populated
     */
    rebuild = (argc == 2 && streq(argv[1], str_rebuild/*"rebuild"*/));

    pid1 = (getpid() == 1) /*|| 1*/;
    cfg_ok = (read_cfg(cfg_file) == 0);

    chdir(root_dir); /* be sure not to stay under /dev ! */
    /* do nothing if we're not called as the first process */
    if (pid1 || rebuild) {
	/* check if /dev is already populated : /dev/console should exist */
	if (stat(dev_console, &statf) == -1) {
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
	}
	else {
	    if (!pid1) {
		/* we don't want to rebuild anything else if pid is not 1 */
	    	print("init/info: /dev is OK.\n");
		return 0;
	    }
	}

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
	while ((token = parse_cfg()) != TOK_EOF) {
	    int cond = 0;
	    int lasterr = error;

	    if (token == TOK_UNK) {
		print("unknown command.\n");
		break;
	    }

	    cond = token & TOK_COND;
	    token &= ~TOK_COND;

	    if (cfg_args[tokens[token].minargs] == NULL) {
		print("Missing args\n");
		break;
	    }

	    /* skip conditionnal executions if unneeded */
	    if ((cond & TOK_COND_OR) && (!lasterr) ||
		(cond & TOK_COND_AND) && (lasterr))
		continue;

	    /* now we can reset the error */
	    error = 0;

	    /* the first command set is always available */	    
	    if (token == TOK_IN) {
		/* I <path> : specify the path to init */
		print("<I>nit : used config name for init\n");
		*argv = cfg_args[1];
		continue;
            }

	    if (!pid1 && !rebuild) { /* other options are reserved for pid 1 only or rebuild mode */
		print("Command ignored since pid not 1\n");
		continue;
	    }

	    /* the second command set is available if pid==1 or if "rebuild" is set */	    
	    switch (token) {
	    case TOK_MD:
		/* D path [ mode ] :  make a directory */
		if (mkdir(cfg_args[1], (cfg_args[2] == NULL) ? 0755 : a2mode(cfg_args[2])) == -1) {
		    error = 1;
		    print("<D>irectory : mkdir() failed\n");
		}
		continue;
	    case TOK_LN:
		/* L from to : make a symlink */
		if (symlink(cfg_args[1], cfg_args[2]) == -1) {
		    error = 1;
		    print("<S>ymlink : symlink() failed\n");
		}
		continue;
	    case TOK_BL:
		/* B <mode> <uid> <gid> <major> <minor> <naming rule> : build a block device */
	    case TOK_CH:
		/* C <mode> <uid> <gid> <major> <minor> <naming rule> : build a character device */
		if (chdir(dev_name) == -1) {
		    print("<B>lock_dev/<C>har_dev : cannot chdir(/dev)\n");
		    error = 1;
		    continue;
		}

		multidev(a2mode(cfg_args[1]) | ((token == TOK_BL) ? S_IFBLK : S_IFCHR),
			 (uid_t)ATOL(cfg_args[2]), (gid_t)ATOL(cfg_args[3]),
			 ATOL(cfg_args[4]), ATOL(cfg_args[5]), cfg_args[6]);
		chdir(root_dir);
		continue;
	    case TOK_FI:
		/* F <mode> <uid> <gid> <name> : build a fifo */
		if (chdir(dev_name) == -1) {
		    error = 1;
		    print("<F>ifo : cannot chdir(/dev)\n");
		}

		mknod_chown(a2mode(cfg_args[1]) | S_IFIFO,
			    (uid_t)ATOL(cfg_args[2]), (gid_t)ATOL(cfg_args[3]),
			    0, 0, cfg_args[4]);

		chdir(root_dir);
		continue;
	    case TOK_EC: {
		/* ec <string> : echo a string */
		int l = strlen(cfg_args[1]);
		cfg_args[1][l] = '\n';
		write(1, cfg_args[1], l + 1);
		continue;
	    }
	    } /* end of switch() */

	    if (!pid1) { /* other options are reserved for pid 1 only */
		print("Command ignored since pid not 1\n");
		continue;
	    }

	    switch (token) {
	    case TOK_MT: {
		/* M dev[(major:minor)] mnt type [ {rw|ro} [ flags ] ]: mount dev on mnt (read-only) */
		char *maj, *min, *end;
		char *mntdev;
		int mntarg;
		
		/* if the device name doesn't begin with a slash, look for it
		 * in /proc/cmdline
		 */
		mntdev = cfg_args[1];
		if ((*mntdev != '/') && ((cmdline_arg = find_arg(mntdev)) != NULL)) {
		    mntdev = cmdline_arg;
		    print("<M>ount : using command line device\n");
		}
		
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
		if (cfg_args[4] != NULL && streq(cfg_args[4], "rw")) {
		    print("<M>ount : 'rw' flag found, mounting read/write\n");
		    mntarg &= ~MS_RDONLY;
		}
		else {
		    print("<M>ount : 'rw' flag not found, mounting read only\n");
		}
		
		if (mount(mntdev, cfg_args[2], cfg_args[3], MS_MGC_VAL | mntarg, cfg_args[5]) == -1) {
		    error = 1;
		    print("<M>ount : error during mount()\n");
		}

		break;
	    }
	    case TOK_EX:
		/* E cmd [cfg_args] : execute cmd with cfg_args, chrooted to dir */
		/* fall through TOK_RX */
	    case TOK_RX: {
		/* R dir cmd [cfg_args] : execute cmd with cfg_args, chrooted to dir */
		char **exec_args, *exec_dir;
		int res;

		exec_args = cfg_args + 1;
		if (token == TOK_EX) {
			exec_dir = (char *)root_dir;
		}
		else {
			exec_dir = cfg_args[1];
			exec_args++;
		}

		res = fork();
		if (res == 0) {
		    chroot(exec_dir);
		    execve(exec_args[0], exec_args, environ);
		    print("<E>xec(child) : execve() failed\n");
		    return 1;
		}
		else if (res > 0) {
		    print("<E>xec(parent) : waiting for termination\n");
		    while (wait(&error) != res)
			print("<E>xec(parent) : signal received\n");
		    
		    error = !WIFEXITED(error);
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
	    case TOK_PR:
		/* P <new_root> <put_old_relative> : pivot root */
		if (chdir(cfg_args[1]) == -1) {
		    error = 1;
		    print("<P>ivot : error during chdir(new root)\n");
		}

		if (pivot_root(".", cfg_args[2]) == -1) {
		    error = 1;
		    print("<P>ivot : error during pivot_root()\n");
		}

		chroot(".");
		if (chdir(root_dir) == -1) {
		    error = 1;
		    print("<P>ivot : error during chdir(/)\n");
		}

		/* replace stdin/stdout/stderr with newer ones */
		reopen_console();
		break;
	    case TOK_MV:
		/* K <old_dir> <new_dir> : keep directory <old_dir> after a pivot. */
		if (mount(cfg_args[1], cfg_args[2], cfg_args[1], MS_MGC_VAL | MS_BIND, NULL) == -1) {
		    error = 1;
		    print("<mv> : error during mount\n");
		}
		/* fall through umount */
	    case TOK_UM:
		/* O <old_dir> : umount <old_dir> after a pivot. */
		if (umount(cfg_args[1]) == -1) {
		    error = 1;
		    print("<um> : error during umount\n");
		}
		break;
	    case TOK_LO: {
		/* l </dev/loopX> <file> : losetup /dev/loopX file */
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
	} /* while (token != TOK_EOF) */
    } else if (pid1) {
	print("init/info : error while opening configuration file\n");

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
		int i;
		print("init/info: /var has been mounted.\n");
		mkdir(var_dir, 0755);
		mkdir(var_tmp, 01777);
		mkdir(var_run,  0755);
		print("init/info: /var has been built.\n");
	    }
	}
    }

    if (rebuild) {
        /* nothing more to do */
	return 0;
    }

    /* handle the lilo command line "INIT=prog" */
    if ((cmdline_arg = find_arg("INIT")) != NULL) {
	argv[0] = cmdline_arg;
	argv[1] = NULL;
    }

    print("init/debug: *argv = "); print (*argv); print("\n");
    umask(old_umask);
    err = execve(*argv, argv, environ);
    print("init/error : last execve() failed\n");
    sleep(200);
    return err;
}
