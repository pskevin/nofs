#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>
#include "time_helpers.h"

#define REPETITIONS 10

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Missing root directory\n");
    return 1;
  }
  const char* root_dir = argv[1];

  double time_stats[REPETITIONS];
  double bytes_written[REPETITIONS];

  char buf[sizeof(char) * 13];

  for (int i = 0; i < REPETITIONS; i++) {
    struct timeval start_time;
    gettimeofday(&start_time, NULL);
    int fds[5];
    for(int k = 0; k < 5; k++) {
      char fname[100];
      sprintf(fname, "%s/random_%d_%d.txt", root_dir, rand(), rand());
      fds[k] = open(fname, O_CREAT | O_EXCL | O_WRONLY, S_IRWXU);
    }

    for(int k = 0; k < 5; k++) {
      int nb = write(fds[k], "content", sizeof(char) * 7);
      bytes_written[i] = (double) nb;
    }

    for(int k = 0; k < 5; k++) {
      close(fds[k]); 
    }

    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    time_stats[i] = elapsed(&start_time, &end_time);
  }

  printf("Report:\n");
  double timing_mean = calc_mean(time_stats, REPETITIONS);
  double timing_std = calc_stdev(time_stats, timing_mean, REPETITIONS);
  printf("Timing: Mean %f, StDev %f\n", timing_mean, timing_std);
  double bytes_mean = calc_mean(bytes_written, REPETITIONS);
  double bytes_std = calc_stdev(bytes_written, bytes_mean, REPETITIONS);
  printf("Bytes: Mean %f, StDev %f\n", bytes_mean, bytes_std);

  return 0;
}