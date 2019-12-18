#ifndef SMTP_THREAD_H
#define SMTP_THREAD_H
#include <pthread.h>
#include <semaphore.h>

#define MAX_FILES 10
#define MAX_MSG_LENGTH 1000

void *client_handler(void *args);

#endif
