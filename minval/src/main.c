#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
 * You are given an array of integers,
 * find the minimum value of the array while 
 * using multiple threads.
 *
 * This will use thread lock.
 */

#define MAX_THREADS 512
#define MIN(a, b) ((a>b)?b:a)
#define MIN_INT 1000000000
#define NULLP(p) (p == NULL)


int min_value_global; 
pthread_mutex_t min_val_lock;

static int size_per_thread = 0;

static void
find_min(void *s)
{
  int i, min_val = MIN_INT;
  int *list_ptr = (int *) s;

  for (i = 0; i < size_per_thread; i++)
    min_val = MIN(min_val, list_ptr[i]);

  /* lock the global min value and update it */
  pthread_mutex_lock(&min_val_lock);

  if (min_val < min_value_global)
    min_value_global = min_val;

  printf("min_val is :%d, min_val_global (after) is: %d\n", min_val, min_value_global);
  
  pthread_mutex_unlock(&min_val_lock);
  pthread_exit(0);
}

int main(int argc, char **argv)
{
  pthread_attr_t attr;
  pthread_t p_thread[MAX_THREADS];
  int *array = NULL, *list_ptr = NULL;
  int count = 8388608, nthreads = 24;
  int i, offset = 12;

  min_value_global = MIN_INT;

  /* 1. Init pthread attribute */
  pthread_attr_init(&attr);
  pthread_mutex_init(&min_val_lock, NULL);

  array = (int *) malloc(sizeof(int) * count);
  if (NULLP(array))
    fprintf(stderr, "Out of memory in %s:%d\n", __FILE__, __LINE__);

  memset(array, 0, sizeof(int) * count);
  for (i = 0; i < count; i++)
    array[i] = i + offset;

  size_per_thread = count/nthreads; /* perfectly divisible here */

  /* fork the threds */
  for (i = 0; i < nthreads; i++)
  {
    list_ptr = &(array[i * size_per_thread]);
    pthread_create(&(p_thread[i]), &attr, (void *) &find_min, (void *) list_ptr);
  }

  /* Join the threads */
  for (i = 0; i < nthreads; i++)
    pthread_join(p_thread[i], NULL);

  printf("\n Minimum value in the array is: %d\n", min_value_global);

  return 0;
}
