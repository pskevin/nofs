#include <math.h>

#define tv_to_double(t) (t.tv_sec + (t.tv_usec / 1000000.0))

void timeDiff(struct timeval *d, struct timeval *a, struct timeval *b)
{
  d->tv_sec = a->tv_sec - b->tv_sec;
  d->tv_usec = a->tv_usec - b->tv_usec;
  if (d->tv_usec < 0) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  }
}

double elapsed(struct timeval *starttime, struct timeval *endtime)
{
  struct timeval diff;

  timeDiff(&diff, endtime, starttime);
  return tv_to_double(diff);
}

double calc_mean(double a[], int n) 
{ 
    double sum = 0; 
    for (int i = 0; i < n; i++)  
        sum += a[i]; 
      
    return sum / n; 
}

double calc_stdev(double a[], double mean, int n) {
  double stdev = 0;
  for (int i = 0; i < n; ++i)
        stdev += pow(a[i] - mean, 2);
    return sqrt(stdev / n);
}