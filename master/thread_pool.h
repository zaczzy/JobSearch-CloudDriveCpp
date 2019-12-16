#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_
#include <stdbool.h>
#include <pthread.h>

typedef struct queue_item {
  int fd;
  // data for the queue
  struct queue_item *next;
} q_item_t;

typedef int (*handler_fn_t)(int fd);

typedef struct thread_pool {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  q_item_t *head;
  q_item_t *tail;
  int length;
  /** Handles the processing of the connection */
  handler_fn_t handle_fn;
  unsigned long n_threads;
  pthread_t *thread_ids;
  bool terminate_flag;
} tp_t;

extern tp_t *thread_pool;

void init_thread_pool(unsigned long n_threads, handler_fn_t);

int thread_pool_enqueue(int fd);

bool should_terminate();
int current_thread_id();
void terminate_thread_pool();
void join_all_threads();
#endif
