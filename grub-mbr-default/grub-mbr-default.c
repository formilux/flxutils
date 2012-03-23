/*
 *  grub-mbr-default - a boot entry validation tool for the GRUB bootloader.
 *
 *  Copyright (C) 2005,2007 Willy Tarreau
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* from stage1/stage1.h */
#define COMPAT_VERSION_MAJOR	3
#define COMPAT_VERSION_MINOR	2

/* from stage2/shared.h */
#define STAGE2_VER_MAJ_OFFS     0x6
#define STAGE2_INSTALLPART      0x8
#define STAGE2_SAVED_ENTRYNO    0xc
#define STAGE2_STAGE2_ID        0x10
#define STAGE2_FORCE_LBA        0x11
#define STAGE2_VER_STR_OFFS     0x12
#define SECTOR_SIZE             0x200


void usage(char *name);
void error(int err, char *msg);
void die(int err, char *msg);

void error(int err, char *msg) {
    perror(msg);
    exit(err);
}

void die(int err, char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(err);
}

void usage(char *name) {
    char *n;
    n = strrchr(name, '/');
    if (n)
	name = n + 1;

    fprintf(stderr,
	    "%s: validate last boot entry in GRUB MBR.\n"
	    "Usage:\n"
	    "\t%s [-q] <device>\n"
	    "\t\tshows what next boot entry will be used on device <device>.\n"
	    "\t\tThe -q argument makes the program quiet.\n"
	    "\n\t%s [-q] <device> <entry>\n"
	    "\t\tsets next boot entry to <entry> on device <device>.\n"
	    "\n\t%s [-q] <device> <-round>\n"
	    "\t\tsets next boot entry to (current-1) rounded to <round>.\n"
	    "\t\tThis permits to revalidate the first entry in a series of\n"
	    "\t\t<round> retries leading to a successful boot.\n"
	    "", name, name, name, name);

    exit(1);
}

int main(int argc, char **argv) {
    char sector[SECTOR_SIZE];
    char *device, *progname;
    int fd, readonly, quiet, stop;
    int entryno, new_entryno;

    progname = argv[0];

    argc--; argv++;
    quiet = stop = new_entryno = 0;
    while (argc && !stop && **argv == '-') {
	switch (argv[0][1]) {
	case 'q':
	    quiet = 1;
	    break;
	case '-':
	    stop = 1;
	    break;
	default:
	    usage(progname);
	}
	argc--; argv++;
    }

    if (argc < 1)
	usage(progname);

    device = argv[0];
    readonly = (argc < 2);
    if (!readonly)
	new_entryno = atoi(argv[1]);

    fd = open(device, readonly ? O_RDONLY : O_RDWR);
    if (fd < 0)
	error(1, "Failed to open the device :");

    if (lseek(fd, SECTOR_SIZE * 2, SEEK_SET) < 0)
	error(2, "Cannot seek to the sector :");

    if (read(fd, sector, SECTOR_SIZE) != SECTOR_SIZE)
	error(2, "Read error or partial read :");

    /* sanity checks */
    if (sector[STAGE2_VER_MAJ_OFFS]   != COMPAT_VERSION_MAJOR ||
	sector[STAGE2_VER_MAJ_OFFS+1] != COMPAT_VERSION_MINOR ||
	*(unsigned *)&sector[STAGE2_INSTALLPART] != 0xFFFFFF) {
	die(3, "Aborting: GRUB MBR signature not found.");
    }

    if (!quiet)
	printf("Found GRUB %c%c%c%c MBR signature.\n",
	       sector[STAGE2_VER_STR_OFFS],
	       sector[STAGE2_VER_STR_OFFS+1],
	       sector[STAGE2_VER_STR_OFFS+2],
	       sector[STAGE2_VER_STR_OFFS+3]
	       );

    entryno = *(unsigned *)&sector[STAGE2_SAVED_ENTRYNO];
    if (readonly) {
	if (!quiet)
	    printf("Next boot will use entry #");
	printf("%d\n", entryno);
	exit(0);
    }

    /* This needs some explanation: the 'savedefault fallback' config statement
     * saves the next entry number before booting the current one. So assuming
     * the configuration uses consecutive fallbacks, the current entry number
     * equals the saved entry number minus 1. This mode allows one to dedicate
     * several entries to a number of retries, so that if entry 2 fails because
     * of power failure, a strictly equivalent entry 3 gives it a second chance
     * to boot. If entry 3 succeeds and if it is a backup of entry 2, we must
     * validate 2 and not 3. This is what is provided here when new_entryno is
     * negative. It reflects the opposite of the number of consecutive equivalent
     * entries in the configuration file. For instance, if each entry is doubled,
     * then new_entryno should be -2.
     */

    if (new_entryno < 0)
	new_entryno = (entryno - 1) - (entryno - 1) % (-new_entryno);

    *(unsigned *)&sector[STAGE2_SAVED_ENTRYNO] = new_entryno;

    if (lseek(fd, SECTOR_SIZE * 2, SEEK_SET) < 0)
	error(4, "Cannot seek to the sector :");

    if (write(fd, sector, SECTOR_SIZE) != SECTOR_SIZE)
	error(4, "Write error :");

    if (!quiet)
	printf("Next boot entry changed to #%d\n", new_entryno);
    fsync(fd);
    close(fd);
    exit(0);
}
