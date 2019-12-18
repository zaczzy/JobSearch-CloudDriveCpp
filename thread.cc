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
#include "seq_consistency.h"
#define MAX_REQUEST_SIZE 4096

extern bool verbose;
extern volatile bool terminate;

int check_if_write(char *command) 
{

  if (strncmp(command, "add", strlen("add")) == 0 || strncmp(command, "ADD", strlen("ADD")) == 0 || strncmp(command, "delete", strlen("delete")) == 0 || strncmp(command, "DELETE", strlen("DELETE")) == 0 || strncmp(command, "changepw", strlen("changepw")) == 0 || strncmp(command, "CHANGEPW", strlen("CHANGEPW")) == 0 || strncmp(command, "putmail", 7) == 0 || strncmp(command, "PUTMAIL", 7) == 0 || strncmp(command, "delmail", 8) == 0 || strncmp(command, "DELMAIL", 8) == 0 || strncmp(command, "putfile", 7) == 0 || strncmp(command, "PUTFILE", 7) == 0 || strncmp(command, "delfile", 7) == 0 || strncmp(command, "DELFILE", 7) == 0 || strncmp(command, "mkfolder", 8) == 0 || strncmp(command, "MKFOLDER", 8) == 0 || strncmp(command, "delfolder", 9) == 0 || strncmp(command, "DELFOLDER", 9) == 0) {
    return 1;
  } else {
    return 0;
  }

}


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

	int write_op = 0;
        /** Read the command from the client */
        int ret;
        ret = read(*client_fd, buffer, MAX_REQUEST_SIZE);


        if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNRESET)
        {
            printf("error code : %s\n", strerror(errno));
            printf("errno : %d\n", errno);
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

            write_op = check_if_write(buffer); 
            //printf("buffer: %s\n", buffer);
            //bool close_connection = process_command(buffer, ret, *client_fd);
            if(write_op == 1) {
              printf("write_op : %d\n", write_op);
              ensure_consistency((uint8_t *)buffer, sizeof(buffer), *client_fd);
            } else {
              process_command(buffer, ret, *client_fd);
            }
            write_op = 0;
            //if (close_connection)
            //    goto term;

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
