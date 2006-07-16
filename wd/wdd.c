/*
 * wdd - simple watchdog daemon - 2003-2004 - willy tarreau
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char dev_wd_str[] = "/dev/watchdog";
const char root_str[] = "/";

/*
 * This function checks if the system can allocate memory
 * In case of failure, we exit so that the watchdog device
 * notices it and can reboot.
 */
static inline void try_malloc() {
    void *heap;

    heap = (void*)sbrk(NULL);
    if (brk(heap + 4096))
	exit(1);
    memset(heap, 0, 4096);
    if (brk(heap))
	exit(1);
}

/*
 * This function checks if the system can fork
 * In case of failure, we exit so that the watchdog device
 * notices it and can reboot.
 */
static inline void try_fork() {
    int pid;
    pid = fork();
    if (pid < 0) /* exit on error */
	exit(1);
    else if (pid == 0) /* child returns cleanly */
	exit(0);
    if (waitpid(pid, NULL, 0) != pid) /* father checks child */
	exit(1);
}


/*
 * This function checks if the system can stat a given directory entry on the
 * VFS. In case of failure, we either report the problem, or exit so that the
 * watchdog device notices it and can reboot.
 */
static inline int try_stat(const char *file, int do_exit) {
    void *heap;
    int ret;

    heap = (void*)sbrk(NULL);
    if (brk(heap + sizeof (struct stat)))
	exit(1);
    memset(heap, 0, sizeof (struct stat));
    ret =  stat(file, heap);
    if (brk(heap))
	exit(1);

    if (ret == -1) {
	if (do_exit)
	    exit(1);
	else
	    return 0;
    }
    return 1;
}

int main (int argc, char **argv) {
    int dev;
    int curr_file;

    if (argc > 1) {
	/* we'll do a quick check on all the arguments to
	 * ensure that they are valid at load time, and avoid
	 * an accidental start of the watchdog which could be
	 * a disaster in case of a file name error.
	 */
	for (curr_file = 1; curr_file < argc; ) {
	    if (try_stat(argv[curr_file], 0))
		curr_file++;
	    else {
		/* remove this file from the list, and make it noticeable from 'ps' */
		*argv[curr_file] = '!';
		argv[curr_file] = argv[--argc];
	    }
	}
    }

    if (fork() > 0)
	return 0;
    for (dev = 2; dev >= 0; dev--)
	close(dev);
    chdir(root_str);
    setsid();

    curr_file = 1; /* start with first file in the list */
    /* let's try indefinitely to open the watchdog device */
    /* note that dev is -1 now ;-) */
    while (1) {
	if (dev == -1)
	    dev = open(dev_wd_str, O_RDWR);
	if ((dev != -1) && (write(dev, dev_wd_str, 1) != 1)) {
	    /* write error, we'll restart */
	    close(dev);
	    dev = -1;
	}
	try_malloc();
	try_fork();

	if (argc > 1) {
	    try_stat(argv[curr_file], 1);
	    curr_file++;
	    if (curr_file >= argc)
		curr_file = 1;
	} else {
	    try_stat(root_str, 1);
	}

	/* avoid a fast loop */
	sleep(1);
    }
    /* we never get there theorically... */
    return 0;
}

