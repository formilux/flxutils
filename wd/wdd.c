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
 * This function checks if the system can access its root FS
 * In case of failure, we exit so that the watchdog device
 * notices it and can reboot.
 */
static inline void try_stat() {
    void *heap;

    heap = (void*)sbrk(NULL);
    if (brk(heap + sizeof (struct stat)))
	exit(1);
    memset(heap, 0, sizeof (struct stat));
    if (stat(root_str, heap) == -1)
	exit(1);
    if (brk(heap))
	exit(1);
}

int main (void) {
    int dev;

    if (fork() > 0)
	return 0;
    for (dev = 2; dev >= 0; dev--)
	close(dev);
    chdir(root_str);
    setsid();
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
	try_stat();
	/* avoid a fast loop */
	sleep(1);
    }
    /* we never get there theorically... */
    return 0;
}

