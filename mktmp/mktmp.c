/*
 * mklock - create a secured lock entry with given data
 * (c) 2003 - willy tarreau - willy@ant-computing.com
 *
 * You may use, copy and redistribute with no restriction. Use at your own
 * risk, I will not be held responsible for any loss or damage.
 *
 * usage :
 *   # mklock <directory> [data]
 *
 * This will create a symlink under <directory>, either pointing to <data> if
 * <data> is specified, or pointing to /proc/<getppid()> if no data is given.
 * This way, the link will automatically be broken as soon as the father dies.
 * The name of the link will be generated randomly until no conflict happens
 * with an existing entry. The full pathname of the resulting link is returned
 * to stdout so that a script can easily use it.
 * This method ensures the use of a secure lock.
 *
 * Example :
 *   # file=`mklock /tmp $$`
 */
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define writetxt(fd, text) write(fd, text, strlen(text))
#define LOCK_PREFIX ".lock."
#define PROC_PREFIX "/proc/"
#define MAX_PID_DIGITS	10	/* 10 digits do code a 32bit pid in decimal */

const char lock_prefix[] = LOCK_PREFIX;
const char proc_prefix[] = PROC_PREFIX;

/* writes <l> in at most 9 decimal digits to <to> */
void ultoa(char *to, unsigned long l) {
    unsigned long div = 1000000000;
    int zero = 1;
    while (div > 0) {
	*to = (l / div % 10) + '0';
	div /= 10;
	zero &= (*to == '0');
	if (!zero)
	    to++;
    }
    *to = 0;
}

/* writes <l> as 8 hex digits to <to> */
void ultoh(char *to, unsigned long l) {
    int shift = 32;
    while ((shift -= 4) >= 0) {
	int digit = (l >> shift) & 0xf;
	if (digit > 9)
	    digit += 'A' - '9' - 1;
	*to++ = digit + '0';
    }
    *to = 0;
}

void usage() {
    writetxt(2, "Usage: mklock [ -d ] [ -p prefix ] <directory> [data]\n");
    exit(1);
}

main(int argc, char **argv) {
    const char *prefix;
    char *text;
    char *directory;
    char *fullname;
    int len, num = 0, ret;
    int use_dir = 0;
    int plen;

    prefix = lock_prefix;

    argv++; argc--;

    while (argc > 0 && **argv == '-') {
	if (!strcmp(*argv, "-d")) {
	    use_dir = 1;
	}
	else if (!strcmp(*argv, "-p")) {
	    if (argc > 1) {
		argv++; argc--;
		prefix = argv[0];
	    }
	    else
		usage();
	}
	argv++; argc--;
    }

    if (argc < 1)
	usage();

    plen = strlen(prefix);
    directory = argv[0];
    len = strlen(directory);
    while (len > 0 && directory[len - 1] == '/')
	directory[--len] = 0;

    fullname = (char *)calloc(1, len + 1 + plen + 10);
    memcpy(fullname, directory, len);
    fullname[len++] = '/';
    memcpy(fullname + len, prefix, plen + 1);
    len += plen;

    argv++; argc--;

    if (argc > 0) /* use this arg as where the link points to */
	text = argv[0];
    else { /* point to /proc/<getppid()> */
	text = (char *)malloc(strlen(PROC_PREFIX) + MAX_PID_DIGITS + 1);
	memcpy(text, proc_prefix, strlen(PROC_PREFIX) + 1);
	ultoa(text + strlen(PROC_PREFIX), getppid());
    }

    do {
	/* try to create the entry with an increasing suffix until we succeed */
	ultoh(fullname+len, num++);
	
	if (use_dir)
	    ret = mkdir(fullname, 0700);
	else
	    ret = symlink(text, fullname);

	if (ret == 0) {
	    fullname[len + 8]='\n';
	    /* success, return the link or directory name */
	    write(1, fullname, len + 9);
	    return 0;
	}
    } while (errno == EEXIST);
    /* other error, let's return 2 */
    return 2;
}
