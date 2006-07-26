/*
 * Fake Uname - version 0.1 - 2002/08/03 - Willy Tarreau <willy@w.ods.org>
 *
 * This uname implementation allows the user to display values according to
 * environment variables, which is particularly helpful when compiling packages
 * for other architectures or CPUs.
 *
 * Usage: uname [-amnrspv]  (same as standard uname)
 * -> machine (-m) will display the contents of UNAME_M if defined.
 *
 */

#include <sys/utsname.h>
#include <stdlib.h>

#define	F_MACH	1
#define	F_NODE	2
#define	F_REL	4
#define	F_SYS	8
#define	F_PROC	16
#define	F_VER	32
#define	F_ALL	63

char proc_name[] = "unknown";
char u_mach_env[] = "UNAME_M";
char usage_msg[] = "Usage: uname [-amnrspv]\n";

char opt[] = {
    'a', F_ALL,
    'm', F_MACH,
    'n', F_NODE,
    'r', F_REL,
    's', F_SYS,
    'p', F_PROC,
    'v', F_VER,
    0
};

char msg[1024];
struct utsname utsname;

/*
 * copies at most <end-dst-2> chars from <src> to <dst>, adds a space, and
 * always ends with a zero, unless <end-dst> is 0. <end> is always set to
 * <msg+sizeof(msg)>, because in our case, the function is always used
 * with this string. The pointer to the terminating zero is returned.
 */
static char *msgcpyspc(char *dst, const char *src) {
    char *end = msg + sizeof(msg);

    if (dst < end) {
	end--;
	while ((dst < end) && (*dst = *src)) {
	    src++; dst++;
	}
	if (dst < end)
	    *dst++ = ' ';
	*dst = 0;
    }
    return dst;
}

static inline usage() {
    write(2, usage_msg, sizeof(usage_msg)-1);
    return 1;
}

main(int argc, char **argv) {
    int i, options = 0;
    char *next, *mach;

    while (argc-- > 1) {
	if (**++argv != '-')
	    return usage();

	for (i = 0; ; i += 2) {
	    if (!opt[i])
		return usage();

	    if (opt[i] == (*argv)[1]) {
		options |= opt[i + 1];
		break;
	    }
	}
    }

    uname(&utsname);
    mach = getenv(u_mach_env);

    if (!options)
	options = F_SYS; /* default behaviour is to return sysname */

    next = msg;
    if (options & F_SYS)	next = msgcpyspc(next, utsname.sysname);
    if (options & F_NODE)	next = msgcpyspc(next, utsname.nodename);
    if (options & F_REL)	next = msgcpyspc(next, utsname.release);
    if (options & F_VER)	next = msgcpyspc(next, utsname.version);
    if (options & F_MACH) {
	if (mach)		next = msgcpyspc(next, mach);
	else			next = msgcpyspc(next, utsname.machine);
    }
    if (options & F_PROC)	next = msgcpyspc(next, proc_name);

    if (next > msg) {
	*(next - 1) = '\n';	/* changes last space for a newline */
	write(1, msg, next - msg);
    }
    return 0;
}
