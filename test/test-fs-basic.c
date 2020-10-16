#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


int
main() {
   int fd0 = open("/tmp/kavan-fuse-mount/test2.txt", O_RDWR);
   int fd1 = open("/tmp/kavan-fuse-mount/test2.txt", O_RDONLY);
   char buf[100];
   strcpy(buf, "hello, it's me");
   int nb0 = write(fd0, buf, 100);
   int nb1;
   close(fd0);
   nb1 = read(fd1, buf, 100);
   assert(!strcmp(buf, "hello, it's me"));
   close(fd1);
   printf("Wrote %d, then %d bytes\n", nb0, nb1);
   return 0;
}