/* simply write all args to /dev/lcd, and don't complain if it doesn't work */
#ifndef NOLIBC
#include <sys/fcntl.h>
#include <string.h>
#endif

int main(int argc, char **argv) {
  int fd;
  char buf[1024];
  int len;


  if ((fd = open("/dev/lcd", O_WRONLY, 0)) == -1)
	return 0;


  if (--argc == 0) {
	while ((len = read(0, buf, sizeof(buf))) > 0)
		write(fd, buf, len);
  }
  else {
	argv++;

	while (argc--) {
		len = strlen(*argv);
		if (argc) /* still other args */
			argv[0][len++]=' '; /* add a delimitor */
		else
			argv[0][len++]='\n'; /* end with a newline */
		write(fd, *argv, len);
		argv++;
	}
  }
  close(fd);
  return 0;
}

