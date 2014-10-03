#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
#  define PRINTF printf
#else
#  define PRINTF(...) 
#endif

// RETURN VALUE:
// 0 : ok
// 1 : no file /proc/sys/dev/cdrom
// 2 : failed to chdir /dev 
// 3 : cdrom found but cannot creat /dev/cdrom
// 4 : no cdrom data found

#define BUFF_SIZE	4096
#define TMP_SIZE	4096

#define STR_SECT ".rodata"
#define STRING static const char __attribute__ ((__section__(STR_SECT),__aligned__(1)))

STRING dev_str[] = "/dev";
STRING cdrom_str[] = "cdrom";
STRING info_str[] = "/proc/sys/dev/cdrom/info";
STRING driver_name[] = "\ndrive name:";
STRING found_str[] = "Found CDROM: ";
STRING nocdrom_str[] = "No CDROM found\n";
STRING eol_str[] = "\n";

int main() {
    int fd, fd2, s, t=0;
    char *p, *f;
    char tmp[TMP_SIZE], buff[BUFF_SIZE];

    // find CDROM detection in /proc/sys/dev/cdrom/info
    if ((fd = open(info_str, O_RDONLY)) < 0) {
	write(2, nocdrom_str, sizeof(nocdrom_str) - 1);
	exit(1);
    }
    if (chdir(dev_str)) { 
	PRINTF("Cannot chdir to /dev\n");
	exit(2);
    }
    // looking for "driver name:"
    while ((s = read(fd, tmp + t, TMP_SIZE - t)) > 0) {
	t += s;
	if (t < TMP_SIZE)
	    tmp[t] = 0;
	else
	    tmp[TMP_SIZE] = 0;

	if ((p = (char*)strstr(tmp, driver_name)) && strchr(p += 13, '\n')) {
	    // have found it, looking for drive name(s)
	    while (*p && *p != '\n') {
		while (*p == ' ' || *p == '\t')
		    p++;

		for (f = p; *f > ' ' ; f++);

		if (*f == '\n')
		    *f = 0;
		else
		    *f++ = 0; 

		// found and now try
		PRINTF("Trying [%s]\n", p);
		if ((fd2 = open(p, O_RDONLY)) >= 0) {
		    // read a small packet to detect valid iso9660
		    if (read(fd2, buff, BUFF_SIZE) > 0) {
			close(fd2);
			close(fd);
			// creat the symbolic link to /dev/cdrom
			if (symlink(p, cdrom_str) == 0) {
#ifdef DEBUG
			    write(2, "  ", 2);
#endif
			    write(2, found_str, sizeof(found_str) - 1);
			    write(2, p, strlen(p)); 
			    write(2, eol_str, 1);
			    exit(0);
			}
			exit(3);
		    } 
		    PRINTF("  read failed\n");
		    close(fd2);
		} else {
		    PRINTF("  open failed\n");
		}
		p = f;
	    }
	    break;
	}
    }
    if (s >= 0)
	close(fd);
    write(2, nocdrom_str, sizeof(nocdrom_str) - 1);
    return (4);
}
