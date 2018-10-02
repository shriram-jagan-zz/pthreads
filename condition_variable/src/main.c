#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>

/*
 * Reference: https://www.cs.ucsb.edu/~rich/class/cs170/notes/CondVar/index.html
 *
 * Objective:
 * ---------
 *  1. Find the number of transactions per second for diff client and trader threads.
 *  2. Find how it varies with different locking mechansims
 *     -  use condition variable and mutex
 *     -  ensure the thread can yield instead of spin
 *
 *  Known:
 *  -----
 *   - Buffer size (this is the bounded buffer prorblme recast)
 *   - generate random stock quanitity, buy/sell action etc
 *   - nClient and ntrader threads
 *   - 
 */

#ifndef NULLP
#define NULLP(p) (p == NULL)
#endif

#ifndef NNULLP
#define NNULLP(p) (!NULLP(p))
#endif


#define MEMERROR_CHECK(p) \
  if (NULLP(p)) \
  { \
    fprintf(stderr, "Unable to allocate memory (%s: %d)\\\n", __FILE__, __LINE__);\
    return NULL; \
  }\

typedef struct order_t_
{
  int stockID;
  int quantity;
  int action;
  bool status;
} order_t;

/* The bounded buffer is a global variable, but it also needs metadata
 * to keep track of what's next, a mutex lock.
 */
typedef struct BoundedBuffer_t_
{
  order_t ** queue;
  int size;
  int head; /* use by client thread */
  int tail; /* used by trader thread */
  pthread_mutex_t bufLock;

} BoundedBuffer_t;

typedef struct market_t_
{
  int *stocks;
  int quantity;

  pthread_mutex_t marketLock;
} market_t;

/* client arguments; client will generate 
 * random stock ticker, quanity etc */
typedef struct client_arg_t_
{
  int id;
  int nClientThreads;
  int nOrderPerThread;
  BoundedBuffer_t *BoundedBuffer;
  
} client_arg_t;

typedef struct trader_arg_t_
{
  int id;
  int nTraderThreads;
  market_t *market;

  BoundedBuffer_t *BoundedBuffer;

} trader_arg_t;

BoundedBuffer_t *
Create_Bounded_Buffer(int bufSize)
{
  BoundedBuffer_t *BoundedBuffer;

  BoundedBuffer = (BoundedBuffer_t *) malloc(sizeof(BoundedBuffer_t));
  if (NULLP(BoundedBuffer))
  {
    fprintf(stderr, "Unable to allocate memory (%s:%d)\n", __FILE__, __LINE__);
    return NULL;
  }

  BoundedBuffer->size = bufSize;
  BoundedBuffer->queue = (order_t **) malloc(sizeof(order_t *) * BoundedBuffer->size);
  if (BoundedBuffer->queue == NULL)
  {
    fprintf(stderr, "Unable to allocate memory (%s:%d)\n", __FILE__, __LINE__);
    return NULL;
  }
  BoundedBuffer->head = 0;
  BoundedBuffer->tail = 0;

  return BoundedBuffer;
}

static void
Free_BoundedBuffer(BoundedBuffer_t *BoundedBuffer)
{
  int i;

  for (i = 0; i < BoundedBuffer->size; i++)
  {
    int k;
  }
}

static int
isBufferFull(BoundedBuffer_t *BoundedBuffer)
{
  if (BoundedBuffer->size == BoundedBuffer->head)
    return 1;
  else
    return 0;
}

/* Client thread will call Create_Order */

order_t *
Create_Order(int stockID, int quantity, int action)
{
  order_t *order;

  order = (order_t *) malloc(sizeof(order_t));
  MEMERROR_CHECK(order);

  order->stockID   = stockID;
  order->quantity  = quantity;
  order->action    = action;
  order->status    = 0;  /* 0: not fulfilled, 1: fulfilled */ 

  return order;
}

static void
Free_Order(order_t *order)
{
  if (NNULLP(order))
  {
    free(order); 
    order = NULL;
  }
}

/* Client thread will create the order(s)
 * and wait for the consumers to finish their task.
 */

static void
clientThread(void *clientArg)
{
  int i;
  order_t *order = NULL;
  int clientQueued = 0;
  client_arg_t *ca;
  BoundedBuffer_t *BoundedBuffer = NULL;

  ca = (client_arg_t *) clientArg;
  BoundedBuffer = ca->BoundedBuffer;

  for (i = 0; i < ca->nOrderPerThread; i++)
  {
    order = Create_Order(0, 10, 0);

    clientQueued = 0;

    /* Update the order to boundedbuffer. This needs a lock */
    while (clientQueued == 0)
    {
      /* lock the buffer before updating it */
      pthread_mutex_lock(&(ca->BoundedBuffer->bufLock));

      /* If the buffer is full, unlock the mutex so that the trader
       * thread can work on it, and wait until trader thread has 
       * finished. Update the buffer only if there is some space in it */
      {
        /* this is to idex the ring buffer */
        /*       | Both client and trader start here, and they move left
         *       v if client moves faster, it will wait for trader to finish
         * - - - - 
         * 3 2 1 0
         */
        int next = (BoundedBuffer->head + 1) % BoundedBuffer->size;

        /* if head is 3 and size is 4, next will be 0. and if traderThread
         * is still working on 0, then we need to wait for the trader
         * to finish
         */
        if (next == BoundedBuffer->tail)
        {
          /* trader needs to work, unlock the buffer */
          pthread_mutex_unlock(&(ca->BoundedBuffer->bufLock));
          continue;
        }

        BoundedBuffer->queue[next] = order;
        BoundedBuffer->head = next;

        /* Update that the client has queued one order */
        clientQueued = 1;
        pthread_mutex_unlock(&(ca->BoundedBuffer->bufLock));
      }
    }

    /* wait for the order to be consumed by the trader */
    while (order->status == 1);

    Free_Order(order);
  }
}

static void
traderThread(void *traderArg)
{
  int i;
}

int main(int argc, char **argv)
{

  /* Known variables */
  int nBuf = 50;               /* Buffer size */
  int nClientThreads = 1;
  int nTraderThreads = 1;

  int nMaxStocks = 500; 

  BoundedBuffer_t *BoundedBuffer = NULL;

  BoundedBuffer = Create_Bounded_Buffer(nBuf);

  /* A routine to free the memory */

  return 0;


}
