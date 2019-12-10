#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include "socket.h"
#include <unistd.h>

extern bool verbose;

int prepare_socket(int port_no)
{
    int server_fd;
    struct sockaddr_in serv_addr;

    // Create socket file descriptor
    if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == 0)
    {
        if (verbose)
            fprintf(stderr, "socket failed\n");
        return -1;
    }

    /** Set reuse option for socket */
    int enable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        if (verbose)
            fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
        return -1;
    }

    /** Update the server address structure */
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_no);

    /** Bind port no to the socket */
    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        if (verbose)
            fprintf(stderr, "bind failed\n");
        return -1;
    }

    /** Start listening for incoming connections */
    if (listen(server_fd, 100) < 0)
    {
        if (verbose)
            fprintf(stderr, "listen failed\n");
        return -1;
    }

    return server_fd;
}

void send_msg_to_socket(const char* msg, int msg_len, int client_fd)
{
    int sent = 0, len;

    while(sent < msg_len)
    {
        len = write(client_fd, msg, msg_len - sent);

         if (len < 0)
        {
            if (verbose)
                fprintf(stderr, "write error while msg\n");

            exit(EXIT_FAILURE);
        }
        sent += len;
    }
}
