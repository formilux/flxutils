/*
 * wdd - simple watchdog daemon - 2003 - willy tarreau
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

const char dev_wd_str[] = "/dev/watchdog";
const char root_str[] = "/";

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
	/* avoid a fast loop */
	sleep(1);
    }
    /* we never get there theorically... */
    return 0;
}

