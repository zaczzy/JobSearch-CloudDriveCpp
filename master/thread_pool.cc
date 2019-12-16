#include "thread_pool.h"
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static __thread int thread_id = -1;

tp_t *thread_pool;

int thread_pool_enqueue(int fd) {
  if (thread_pool == NULL) {
    perror("Cannot enqueue to NULL thread_pool");
    return -1;
  }

  q_item_t *fd_item = (q_item_t *)malloc(sizeof(*fd_item));
  fd_item->fd = fd;
  fd_item->next = NULL;

  pthread_mutex_lock(&thread_pool->mutex);

  if (thread_pool->head == NULL) {
    // new element at the head
    thread_pool->head = fd_item;
    thread_pool->tail = fd_item;
  } else {
    // insert at the tail
    thread_pool->tail->next = fd_item;
    thread_pool->tail = fd_item;
  }
  thread_pool->length += 1;
  pthread_cond_broadcast(&thread_pool->cond);
  pthread_mutex_unlock(&thread_pool->mutex);
  return 0;
}

bool should_terminate() { return thread_pool->terminate_flag; }

static int dequeue() {
  int fd = -1;
  pthread_mutex_lock(&thread_pool->mutex);
  while (thread_pool->head == NULL && !should_terminate())
    pthread_cond_wait(&thread_pool->cond, &thread_pool->mutex);
  if (thread_pool->head != NULL) {
    fd = thread_pool->head->fd;
    q_item_t *to_remove = thread_pool->head;
    thread_pool->head = thread_pool->head->next;
    free(to_remove);
    thread_pool->length -= 1;
  }
  pthread_mutex_unlock(&thread_pool->mutex);
  return fd;
}

void terminate_thread_pool() {
  thread_pool->terminate_flag = true;
  pthread_cond_broadcast(
      &thread_pool->cond);  // wake up all threads waiting on the mutex
  // interrupt the blocking system calls
  for (unsigned long i = 0; i < thread_pool->n_threads; i++) {
    int retval;
    if ((retval = pthread_kill(thread_pool->thread_ids[i], SIGUSR1))) {
      // fprintf(stderr, "Cannot signal SIGUSER1 to thread id %lu, with return value: %d\n",
      //         thread_pool->thread_ids[i], retval);
      // perror("It's okay tho if read is interrupted");
    }
  }
}

static void *process_request(void *v) {
  int ret_val = 0;
  while (!should_terminate() && ret_val == 0) {
    int fd = dequeue();
    if (fd == -1) break;
    ret_val = thread_pool->handle_fn(fd);
  }
  //    if any thread experience an error, then server should shut down
  if (!should_terminate()) terminate_thread_pool();
  return NULL;
}

void init_thread_pool(unsigned long n_threads, handler_fn_t cb) {
  if (n_threads <= 0) {
    printf("Cannot initialize %lu threads", n_threads);
  }

  tp_t *t = (tp_t *)malloc(sizeof(*t));
  thread_pool = t;
  pthread_mutex_init(&t->mutex, NULL);
  pthread_cond_init(&t->cond, NULL);
  t->head = NULL;
  t->tail = NULL;
  t->length = 0;
  t->n_threads = n_threads;
  t->handle_fn = cb;
  t->thread_ids = (pthread_t *)malloc(sizeof(pthread_t) * n_threads);
  t->terminate_flag = false;
  for (unsigned long i = 0; i < n_threads; i++) {
    pthread_create(t->thread_ids + i, NULL, process_request, (void *)NULL);
  }
  thread_pool = t;
}

void join_all_threads() {
  void *retval;
  for (unsigned long i = 0; i < thread_pool->n_threads; i++) {
    if (pthread_join(thread_pool->thread_ids[i], &retval) != 0) {
      perror("Waiting error");
    };
  }
  pthread_mutex_destroy(&thread_pool->mutex);
  pthread_cond_destroy(&thread_pool->cond);
  free(thread_pool->thread_ids);
  free(thread_pool);
}
