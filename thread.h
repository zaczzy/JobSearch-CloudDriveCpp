#pragma once

#include <vector>
#include <pthread.h>

typedef struct
{
    int pipe_up[2];
    int pipe_down[2];
}pipe_fd;

void create_thread(int* fd);
