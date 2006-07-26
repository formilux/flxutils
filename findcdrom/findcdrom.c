#include <fcntl.h>

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


int main() {
  int fd,fd2,s,t=0;
  char tmp[4096],buff[4096],*p,*f;
  static const char nocdrom[]="No CDROM found\n";

  // find CDROM detection in /proc/sys/dev/cdrom/info
  if ((fd=open("/proc/sys/dev/cdrom/info",O_RDONLY)) < 0) {
    write(2,nocdrom,sizeof(nocdrom));
    exit(1);
  }
  if (chdir("/dev")) { 
    PRINTF("Cannot chdir to /dev\n");
    exit(2);
  }
  // looking for "driver name:"
  while ((s=read(fd,tmp+t,4096-t)) > 0) {
    t+=s;
    if ((p=(char*)strstr(tmp,"\ndrive name:")) && strchr(p+=13,'\n')) {
      // have found it, looking for drive name(s)
      while (*p != '\n') {
        while (*p == ' ' || *p == '\t') p++;
        for (f=p;*f > ' ' ; f++) ;
        if (*f == '\n') *f=0; else *f++=0; 
        // found and now try
        PRINTF("Trying [%s]\n",p);
        if ((fd2=open(p,O_RDONLY)) >= 0) {
          // read a small packet to detect valid iso9660
          if (read(fd2,buff,4096) > 0) {
            close(fd2);
            close(fd);
            // creat the symbolic link to /dev/cdrom
            if (symlink(p,"cdrom") == 0) {
#ifdef DEBUG
              write(2,"  ",2);
#endif
              write(2,"Found CDROM: ",13);
              write(2,p,strlen(p)); 
              write(2,"\n",1);
              exit(0);
            }
            exit(3);
          } 
          PRINTF("  read failed\n");
          close(fd2);
        } else {
          PRINTF("  open failed\n");
        }
        p=f;
      }
      break;
    }
  }
  if (s >= 0) close(fd);
  write(2,nocdrom,sizeof(nocdrom));
  return (4);
}

