#include "thread.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "socket.h"
#include "key_value.h"
#include "key_value.h"
#define MAX_REQUEST_SIZE 4096
extern bool verbose;
extern volatile bool terminate;

void* read_commands(void* args)
{
    /** Detach the thread from the parent thread */
    pthread_detach(pthread_self());

    std::vector<int64_t> my_nums;
    int* client_fd = (int*)(args);

    char buffer[MAX_REQUEST_SIZE];
    while(!terminate)
    {
        memset(buffer, 0, MAX_REQUEST_SIZE);

        /** Read the command from the client */
        int ret;
        ret = read(*client_fd, buffer, MAX_REQUEST_SIZE);

        if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            printf("error code : %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (ret == 0)
        {
            goto term;
        }
        else if (ret > 0)
        {
            printf("read %d bytes\n", ret);
            printf("<%s>\n", buffer);
            if (terminate)
                goto term;
        
            process_command(buffer, ret, *client_fd);
        }
    }

term:

    /** Close the socket connection */
    close(*client_fd);
    delete(client_fd);

    if (verbose)
        printf("exiting thread\n");
    pthread_exit(NULL);
}

void create_thread(int* fd)
{
    int flags = fcntl(*fd, F_GETFL, 0);
    fcntl(*fd, F_SETFL, flags | O_NONBLOCK);

    pthread_t thread;
    int iret1 = pthread_create(&thread, NULL, read_commands, (void*) (fd));

    if (iret1 != 0)
    {
        if  (verbose)
            fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }
}
