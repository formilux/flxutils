/* simply write all args to /dev/lcd, and don't complain if it doesn't work */
#include <sys/fcntl.h>

int main(int argc, char **argv) {
  int fd;
  char buf[1024];
  int len;

  fd = open("/dev/lcd", O_WRONLY, 0);

  while ((len = read(0, buf, sizeof(buf))) > 0) {
		write(1, buf, len);
		if (fd > 0)
			write(fd, buf, len);
  }

  if (fd > 0)
	close(fd);
  return 0;
}

