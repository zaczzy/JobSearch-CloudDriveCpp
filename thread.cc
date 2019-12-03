#include "thread.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "socket.h"
#include "key_value.h"
#include "key_value.h"

extern bool verbose;
extern volatile bool terminate;

void* read_commands(void* args)
{
    /** Detach the thread from the parent thread */
    pthread_detach(pthread_self());

    std::vector<int64_t> my_nums;
    int* client_fd = (int*)(args);

    /** Register the signal handler */
    //signal(SIGINT, handle_sigint);

    char buffer[2048];
    while(!terminate)
    {
        memset(buffer, 0, sizeof(buffer));

        //while (!command_completed)
        {
            /** Read the command from the client */
            int ret;
            ret = read(*client_fd, buffer, sizeof(buffer));

            if (ret == -1)
            {
                printf("error code : %s\n", strerror(errno));
            }
            if (ret == 0)
            {
                continue;
            }
            printf("read %d bytes\n", ret);
            if (terminate)
                goto term;

            if (ret < 0)
            {
                // ERror
                exit(EXIT_FAILURE);
            }
            
            //printf("buffer: %s\n", buffer);
            //bool close_connection = process_command(buffer, ret, *client_fd);
            process_command(buffer, ret, *client_fd);
            
            //if (close_connection)
            //    goto term;
        }
    }

term:

    /** Close the socket connection */
    close(*client_fd);

    pthread_exit(NULL);

}

void create_thread(int* fd)
{
    pthread_t thread;
    int iret1 = pthread_create(&thread, NULL, read_commands, (void*) fd);

    if (iret1 != 0)
    {
        if  (verbose)
            fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }
}
