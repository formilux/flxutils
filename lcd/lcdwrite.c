/* simply write all args to /dev/lcd, and don't complain if it doesn't work */
#ifndef NOLIBC
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#endif

/* waits for ~10ms, keeping track of the cumulated wait time, and returns < 0
 * if the cumulated wait time reaches one second. 0 is returned otherwise.
 */
int wait_1smax()
{
	static unsigned int total_wait;
	struct timeval tv;

	if (total_wait >= 1000)
		return -1;
	tv.tv_sec  = 0;
	tv.tv_usec = 10000;
	select(0, NULL, NULL, NULL, &tv);
	total_wait += 10;
	return 0;
}

int write_lcd(int fd, const char *buf, int len)
{
	int count = len;
	int ret;

	while (len > 0) {
		ret = write(fd, buf, len);

		if (ret < 0) {
			if (errno != EBUSY && errno != EINTR)
				return ret;

			/* conflict with another writer, wait a bit */
			if (wait_1smax() < 0)
				return ret;
			continue;
		}

		buf += ret;
		len -= ret;
	}
	return count;
}

int main(int argc, char **argv) {
  int fd;
  char buf[1024];
  int len;


  while ((fd = open("/dev/lcd", O_WRONLY, 0)) == -1) {
	if (errno != EBUSY)
		return 0;
	if (wait_1smax() < 0)
		return 0;
  }


  if (--argc == 0) {
	while ((len = read(0, buf, sizeof(buf))) > 0)
		if (write_lcd(fd, buf, len) < 0)
			return 0;
  }
  else {
	argv++;

	while (argc--) {
		len = strlen(*argv);
		if (argc) /* still other args */
			argv[0][len++]=' '; /* add a delimitor */
		else
			argv[0][len++]='\n'; /* end with a newline */
		if (write_lcd(fd, *argv, len) < 0)
			return 0;
		argv++;
	}
  }
  close(fd);
  return 0;
}

