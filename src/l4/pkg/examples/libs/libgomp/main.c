#include <omp.h>
#include <stdio.h>

#include <semaphore.h>

int main(void)
{
  int id, nthreads;
  printf("Program launching\n");
#pragma omp parallel private (id)
  {
    id = omp_get_thread_num();
    printf("Hello World from thread %d\n", id);
#pragma omp barrier
    if (id == 0) {
      nthreads = omp_get_num_threads();
      printf("There are %d threads\n", nthreads);
    }
  }
  printf("DONE\n");
  return 1;
}
