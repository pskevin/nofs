#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


int
main() {
   int fd1 = open("/tmp/kavan-fuse-mount/test3.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
   printf("Opened for create...\n");
   char msg[] = "My stuff\n";
   int nb;
   nb = write(fd1, msg, strlen(msg));
   printf("wrote content: %s\n", msg);
   close(fd1);
   return 0;
}