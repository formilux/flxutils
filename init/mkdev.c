/*
  WARNING ! THIS CODE IS OPTIMIZED FOR SIZE WITH COMPILED-IN ARGUMENTS. IT IS
  SUBJECT TO BUFFER OVERFLOWS SO DON'T USE IT WITH RUNTIME ARGUMENTS !!!
*/

/* mkdev - from preinit-12 - Willy Tarreau <willy AT meta-x.org>

  usage : mkdev expression

  **** needs to rework the doc a bit since it's not up-to-date. ****

      L source dest
      	make a symlink from <source> to <dest>
      D path [ mode ]
      	create a directory named <path> with the mode <mode>. If mode is left
	undefined, 0755 is assumed.
      B mode uid gid major minor naming_rule
        create a set of block devices
      C mode uid gid major minor naming_rule
        create a set of char devices
      F mode uid gid name
	create a fifo

    The devices naming rules consist in strings mixed with numbering rules delimited with
    brackets. Each numbering rule has 4 comma-separated fields :
      - type of this string portion : 'c' for a single char, 'i' for an int, 'I' for an int
        for which 0 is not printed, 'h' for an hex digit
      - the range : 
          - chars: any concatenation of character ranges separated with a dash '-': 'a-fk-npq'
	  - ints : either an int or a range composed of 2 ints separated with a dash : '1-16'
	  - hex  : same as int, but with hex digits (case insensitive)
      - the scale : how much to add to the minor device for each step in the range.

    example :
      L hda3 /dev/disk  => symlinks /dev/disk to hda3
      D /var/tmp 1777	=> creates a directory /var/tmp with mode 1777
      B 0600 0 0 3 1 hd[c,ab,64][i,1-16,1] => makes all hdaX and hdbX with X ranging from 1 to 16
      C 0600 0 5 2 0 pty[c,p-za-f,16][h,0-f,1] => makes all 256 pty*

*/

#ifndef NOLIBC
#include <stdio.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/loop.h>
#endif


//#ifdef DEBUG
static void print(char *c) {
    char *p = c;
    while (*p)
        p++;
    write(0, c, p-c);
}
//#else
//#define print(a,...)	do{}while(0)
//#endif

typedef unsigned char uchar;

#define UID_ROOT	0
#define GID_ROOT	0
#define GID_TTY		5
#define GID_KMEM	9

/* used by naming rules */
#define MAX_FIELDS	8
#define MAX_DEVNAME_LEN	64
#define	MAX_CFG_ARGS	16

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

static char *cfg_args[MAX_CFG_ARGS];
static char *cst_str[MAX_FIELDS];
static char *var_str[MAX_FIELDS];
static struct dev_varstr var[MAX_FIELDS];

/* makes a dev entry */
static inline int mknod_chown(mode_t mode, uid_t uid, gid_t gid, uchar major, uchar minor, char *name) {
    if (mknod(name, mode, makedev(major, minor)) == -1) {
	print("init/error : mknod("); print(name); print(") failed\n");
    }

    if (chown(name, uid, gid) == -1) {
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
    *scale = atol(res[2]);
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
    int leading0 = 1;
    for (div = 100; div > 0; div /= 10) {
	if (!leading0 || (div == 1) || (num / div)) {
	    *dest++ = (num / div) + '0';
	    leading0 = 0;
	}
	num %= div;
    }
    *dest = '\0';
    return dest;
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
    int err, arg;
    char cmd;


    if (argc > 1)
	cmd = *argv[1];
    else {
	print("Expect command: [BCDFL]\n");
	exit(0);
    }

    argc-=2; argv+=2;
    for (arg = 0; arg < MAX_CFG_ARGS; arg++) {
	if (arg < argc)
	    cfg_args[arg] = argv[arg];
	else
	    cfg_args[arg] = NULL;
    }


    switch (cmd) {
    case 'D':
	/* D path [ mode ] :  make a directory */
	if (cfg_args[0] != NULL) {
	    int mode = 0755;
	    if (cfg_args[1] != NULL)
		mode = a2mode(cfg_args[1]);

	    if (mkdir(cfg_args[0], mode) == -1)
		print("mkdir() failed\n");
	}
	else {
	    print("D path [ mode ]\n");
	}
	break;
    case 'L':
	/* L from to : make a symlink */
	if (cfg_args[1] != NULL) {
	    if (symlink(cfg_args[0], cfg_args[1]) == -1)
		print("symlink() failed\n");
	}
	else {
	    print("L source dest\n");
	}
	break;
    case 'B':
	/* B <mode> <uid> <gid> <major> <minor> <naming rule> : build a block device */
	if (cfg_args[5] == NULL) {
	    print("B mode uid gid major minor rule\n");
	    break;
	}

	multidev(a2mode(cfg_args[0]) | S_IFBLK,
		 (uid_t)atol(cfg_args[1]), (gid_t)atol(cfg_args[2]),
		 atol(cfg_args[3]), atol(cfg_args[4]), cfg_args[5]);

	break;
    case 'C':
	/* C <mode> <uid> <gid> <major> <minor> <naming rule> : build a character device */
	if (cfg_args[5] == NULL) {
	    print("C mode uid gid major minor rule\n");
	    break;
	}

	multidev(a2mode(cfg_args[0]) | S_IFCHR,
		 (uid_t)atol(cfg_args[1]), (gid_t)atol(cfg_args[2]),
		 atol(cfg_args[3]), atol(cfg_args[4]), cfg_args[5]);

	break;
    case 'F':
	/* F <mode> <uid> <gid> <name> : build a fifo */
	if (cfg_args[3] == NULL) {
	    print("F mode uid gid name\n");
	    break;
	}

	mknod_chown(a2mode(cfg_args[0]) | S_IFIFO,
		    (uid_t)atol(cfg_args[1]), (gid_t)atol(cfg_args[2]),
		    0, 0, cfg_args[3]);

	break;
    default:
	print("Unknown command\n");
	break;
    }
    return 0;
}
