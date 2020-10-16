#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include "time_helpers.h"

#define REPETITIONS 100

#define TEST_NOFS 1
#if TEST_NOFS
  #define FPATH "/var/tmp/nofs[mount]/indus" 
#else
  #define FPATH "/u/pskevin/indus" 
#endif

int main() {
  char file_name[100];
  double time_stats[REPETITIONS];
  double bytes_written[REPETITIONS];

  for (int i = 0; i < REPETITIONS; i++) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    int fd0 = open(FPATH, O_RDWR);
    char buf[sizeof(char) * 14];
    strcpy(buf, "small update\n");
    int nb0 = write(fd0, buf, sizeof(char) * 14);
    close(fd0);
    int fd1 = open(FPATH, O_RDONLY);
    int nb1 = read(fd1, buf, sizeof(char) * 14);
    close(fd1);

    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    time_stats[i] = elapsed(&start_time, &end_time);
    bytes_written[i] = nb0;
  }
  
  printf("Timing Report:\n");
  double mean = calc_mean(time_stats, REPETITIONS);
  double std = calc_stdev(time_stats, mean, REPETITIONS);
  printf("Write: Mean %f, StDev %f\n", mean, std);
  double bytes_mean = calc_mean(bytes_written, REPETITIONS);
  double bytes_std = calc_stdev(bytes_written, bytes_mean, REPETITIONS);
  printf("Bytes: Mean %f, StDev %f\n", bytes_mean, bytes_std);


  return 0;
}
