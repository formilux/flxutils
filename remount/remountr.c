#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char mount_str[] = "/bin/mount";
const char remount_str[] = "remount";
const char slash_str[] = "/";
const char ro_str[] = "-ro";
const char wo_str[] = "-wo";

int main (int argc, char **argv) {
   const char *flag, *mntpnt;

   if (argv[0][strlen(argv[0])-1] == 'w')
	flag = wo_str;
   else
	flag = ro_str;

   setreuid(0, 0);
   if (argc > 1)
	mntpnt = argv[1];
   else
	mntpnt = slash_str;

   return execl(mount_str, mount_str, flag, remount_str, mntpnt, NULL);
}

