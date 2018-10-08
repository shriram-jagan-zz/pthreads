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
 *   - generate random stock quantity, buy/sell action etc
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
  int *done; /* this is a global variable; main updates it */
  market_t *market;

  BoundedBuffer_t *BoundedBuffer;

} trader_arg_t;

BoundedBuffer_t *
Create_Bounded_Buffer_And_Init_Order(int bufSize)
{
  BoundedBuffer_t *BoundedBuffer;

  BoundedBuffer = (BoundedBuffer_t *) malloc(sizeof(BoundedBuffer_t));
  if (NULLP(BoundedBuffer))
  {
    fprintf(stderr, "Unable to allocate memory (%s:%d)\n", __FILE__, __LINE__);
    return NULL;
  }

  BoundedBuffer->size = bufSize + 1;
  BoundedBuffer->queue = (order_t **) malloc(sizeof(order_t *) * BoundedBuffer->size);
  if (BoundedBuffer->queue == NULL)
  {
    fprintf(stderr, "Unable to allocate memory (%s:%d)\n", __FILE__, __LINE__);
    return NULL;
  }
  BoundedBuffer->head = 0;
  BoundedBuffer->tail = 0;

  /* init order */
  memset(BoundedBuffer->queue, 0, sizeof(order_t *) * BoundedBuffer->size);

  /* init the lock */
  pthread_mutex_init(&(BoundedBuffer->bufLock), NULL);

  return BoundedBuffer;
}

static void
Free_BoundedBuffer(BoundedBuffer_t *BoundedBuffer)
{
  int i;

  for (i = 0; i < BoundedBuffer->size; i++)
  {
  }
  printf("\n free function not implemented yet");
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

void*
clientThreadWorker(void *clientArg)
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
    order = Create_Order(0, 1, 0);

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
        /*       | Both client and trader start here, and they move left.
         *       v if client moves faster, it will wait for trader to finish
         * - - - - 
         * 3 2 1 0
         */
        int next = (BoundedBuffer->head + 1) % BoundedBuffer->size;

        /* if head is 3 and size is 4, next will be 0. and if traderThreadWorker
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

  return(NULL);
}

void*
traderThreadWorker(void *traderArg)
{
  int i, next = 0;
  int dequequed = 1;
  int stockId = 0;
  int debugPosition = 0;

  trader_arg_t *ta;
  BoundedBuffer_t *BoundedBuffer = NULL;
  order_t *order = NULL;

  /* the trader should lock the queue, take the order, 
   * unlock, and consume it. During consumption it should also lock
   * the market when it updates the stock quantity
   */

  ta = (trader_arg_t *) traderArg;

  /* the trader thread just follows the clientthread, so this
   * runs until the pthread_exit function has been called on
   * all trader threads
   */
  while (1)
  {
    dequequed = 0;
    while (dequequed == 0)
    {
      pthread_mutex_lock(&(ta->BoundedBuffer->bufLock));

      /* empty order, wait for clientThreadWorker to queue order */
      if (ta->BoundedBuffer->head == ta->BoundedBuffer->tail)
      {
        pthread_mutex_unlock(&(ta->BoundedBuffer->bufLock));

        /* check if exit condition met, else continue;
         * remember the done value is udated my main
         */
        if (*(ta->done) == 1)
          pthread_exit(NULL);

        continue;
        /* check if we are done, if yes, then exit */
      }
      next = (ta->BoundedBuffer->tail + 1) % ta->BoundedBuffer->size;
      ta->BoundedBuffer->tail = next;

      order = ta->BoundedBuffer->queue[next];

      /* Let go of this lock */
      pthread_mutex_unlock(&(ta->BoundedBuffer->bufLock));
      dequequed = 1;
    }

    /* Now consume this order with the market */
    pthread_mutex_lock(&(ta->market->marketLock));

    stockId = order->stockID;
    if (order->action == 0)
      ta->market->stocks[stockId] += order->quantity;
    else
    {
      if ((ta->market->stocks[stockId] - order->quantity) >= 0)
       ta->market->stocks[stockId] -= order->quantity;
    }

    pthread_mutex_unlock(&(ta->market->marketLock));

    /* let the client know that the order has been fulfilled */
    order->status = 1;

    printf("Trader thread id %d.\n", ta->id);
  }

  return(NULL);
}

market_t *
Allocate_Market(int marketSize)
{
  int i;
  market_t *market = NULL;

  market = (market_t *) malloc(sizeof(market_t));

  market->quantity = marketSize;
  market->stocks = (int *) malloc(market->quantity * sizeof(int));

  /* init the memory */
  memset(market->stocks, 0, sizeof(market->quantity * sizeof(int)));

  /* At first we have 100000 of all stocks */
  for (i = 0; i < marketSize; i++)
    market->stocks[i] = 100000;

  /* init the mutex lock */
  pthread_mutex_init(&(market->marketLock), NULL);

  return market;
}

static void
Free_Market(market_t *market)
{
  if (NNULLP(market->stocks))
  {
    free(market->stocks);
  }

  market->stocks = NULL;
  market->quantity = 0;
  market = NULL;
}

int main(int argc, char **argv)
{
  int i, err = 0;
  int done = 0;

  BoundedBuffer_t *BoundedBuffer = NULL;

  /* Known variables */
  int nBuf = 50;               /* Buffer size */
  int nClientThreads = 4;
  int nTraderThreads = 2;
  int nOrderPerThread = 1000;

  int nMaxStocks = 500; 

  /* pthread related variables */
  pthread_t *traderThreads;
  pthread_t *clientThreads;

  trader_arg_t *ta = NULL;
  client_arg_t *ca = NULL;

  /* Market */
  market_t *market = NULL;

  BoundedBuffer = Create_Bounded_Buffer_And_Init_Order(nBuf);

  /* allocate the market */
  market = Allocate_Market(nMaxStocks);

  /* Allocate threads, args etc */
  traderThreads = (pthread_t *) malloc(sizeof(pthread_t) * nTraderThreads);
  clientThreads = (pthread_t *) malloc(sizeof(pthread_t) * nClientThreads);
  
  ta = (trader_arg_t *) malloc(sizeof(trader_arg_t) * nTraderThreads); 
  ca = (client_arg_t *) malloc(sizeof(client_arg_t) * nClientThreads); 

  /* now set the arguments to threads */
  for (i = 0; i < nClientThreads; i++)
  {
    ca[i].id = i;
    ca[i].nOrderPerThread = nOrderPerThread;
    ca[i].nClientThreads = nClientThreads;
    ca[i].BoundedBuffer = BoundedBuffer;

    err = pthread_create(&(clientThreads[i]), NULL, clientThreadWorker, (void *) &(ca[i]));;
    if (err)
      printf("\n Client thread creation errored out!");
  }

  done = 0; /* for trader thread */
  for (i = 0; i < nTraderThreads; i++)
  {
    ta[i].id = i;
    ta[i].nTraderThreads = nTraderThreads;
    ta[i].BoundedBuffer = BoundedBuffer;
    ta[i].done = &done;
    ta[i].market = market;

    err = pthread_create(&(traderThreads[i]), NULL, traderThreadWorker, (void *) &(ta[i]));
    if (err)
      printf("\n Trader thread creation errored out!");
  }

  /* Once the client threads are done, join their threads */
  for (i = 0; i < nClientThreads; i++)
  {
    pthread_join(clientThreads[i], NULL);
  }

  /* Now let the trader threads know that we are DONE! */
  done = 1;
  for (i = 0; i < nTraderThreads; i++)
  {
    pthread_join(traderThreads[i], NULL);
  }

  Free_Market(market);
  free(ca);
  free(ta);
  free(traderThreads);
  free(clientThreads);

  return 0;


}
