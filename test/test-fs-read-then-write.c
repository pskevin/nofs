#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


int
main() {
   int fd1 = open("/tmp/kavan-fuse-mount/test2.txt", O_RDONLY);
   printf("Opened for reading...\n");
   char buf[100];
   int nb;
   nb = read(fd1, buf, 100);
   printf("Read content: %s\n", buf);

   int fd2 = open("/tmp/kavan-fuse-mount/test2.txt", O_RDWR);
   nb = read(fd2, buf, 100);
   printf("Read content: %s\n", buf);
   strcpy(buf, "New Content");
   nb = write(fd2, buf, 100);
   printf("Wrote content: %s\n", buf);
   return 0;
}