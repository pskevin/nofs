#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include "time_helpers.h"

#define REPETITIONS 1000

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Missing root directory\n");
    return 1;
  }
  const char* root_dir = argv[1];
  double time_stats[REPETITIONS];
  double bytes_read[REPETITIONS];

  char buf[sizeof(char) * 13];

  for (int i = 0; i < REPETITIONS; i++) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    char fname[100];
    sprintf(fname, "%s/indus.txt", root_dir);
    int fd1 = open(fname, O_RDONLY);
    int nb1 = read(fd1, buf, sizeof(char) * 13);
    close(fd1);
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    time_stats[i] = elapsed(&start_time, &end_time);
    bytes_read[i] = (double)nb1;
  }

  printf("Report:\n");
  double timing_mean = calc_mean(time_stats, REPETITIONS);
  double timing_std = calc_stdev(time_stats, timing_mean, REPETITIONS);
  printf("Timing: Mean %f, StDev %f\n", timing_mean, timing_std);
  double bytes_mean = calc_mean(bytes_read, REPETITIONS);
  double bytes_std = calc_stdev(bytes_read, bytes_mean, REPETITIONS);
  printf("Bytes: Mean %f, StDev %f\n", bytes_mean, bytes_std);

  return 0;
}