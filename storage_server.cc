#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <vector>
#include <errno.h>
#include <unordered_map>

#include "socket.h"
#include "thread.h"

#define IP_ADDRESS_LEN  16
#define SUCCESS         0
#define FAILURE         1

bool verbose = false;
volatile bool terminate = false;

volatile int fds[100];
int client_count = 0;
int server_fd;
std::unordered_map<int, char*> server_addresses; 
uint8_t total_servers = 0;
uint8_t server_id;

void run_storage_server(char* ip_address, int port_no)
{
    printf("running storage server\n");
    char success_msg[] = "+OK Server ready (Author: Team 24)\r\n";
    int msg_len = strlen(success_msg);

    /** Prepare the socket - create, bind, and listen to port */
    server_fd = prepare_socket(port_no);

    if (server_fd == -1)
        exit(EXIT_FAILURE);

    /** Run until terminated by a SIGINT signal */
    while(!terminate)
    {
        int *client_fd = new int;
        *client_fd = -1;
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);

        /** Accept the connection */
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addrlen);

        /** Check if terminate flag is set, if yes terminate the fd immediately */
        if (terminate)
        {
            close(*client_fd);
            exit(EXIT_SUCCESS);
        }
        else
        {
            fds[client_count++] = *client_fd;
        }

        if (*client_fd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            if (verbose)
                fprintf(stderr, "accept failed\n");
            exit(EXIT_FAILURE);
        }
        
        if (verbose)
            printf("accepted client connection\n");

        /** Send the connection OK msg */
        send_msg_to_socket(success_msg, msg_len, *client_fd);

        /** Create a separate thread for communication */
        create_thread(client_fd);
    }
}

bool read_server_config(char* config_file, int server_no, char* ip_address, int* port_no)
{
    /** Open config file */
    FILE* file = fopen(config_file, "r");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open server config file\n");

        return FAILURE;
    }
    
    int line_no = 0;
    ssize_t ret;
    char *line, *token;
    size_t len = 0;
    memset(ip_address, 0, IP_ADDRESS_LEN);
    *port_no = 0;

    while ((ret = getline(&line, &len, file)) != -1)
    {
        line_no++;

        if (line_no == server_no)
        {
            server_id = server_no;
            token = strtok(line, ":");

            if (token != NULL)
            {
                strncpy(ip_address, token, strlen(token));

                token = strtok(NULL, ":");
                if (token != NULL)
                    *port_no = atoi(token);
                
                if (verbose)
                {
                    printf("ip address: %s\n pert no: %d\n", ip_address, *port_no);
                }
            }
        }
        else
        {
            /** Save other server's address */
            token = strtok(token, ":");

            if (token != NULL)
            {
                char* port_no_str = strtok(NULL, ":");

                char *ip = (char*)malloc(32);
                strncpy(ip, token, strlen(token));
                if (port_no_str != NULL)
                {
                    server_addresses.insert(std::make_pair(atoi(port_no_str), ip));
                }
            }
        }

        total_servers++;
    }
        if (verbose)
            printf("total servers: %u\n", total_servers);
    return SUCCESS;
}

void parse_args(int argc, char *argv[], char* config_file, int* server_no)
{
    int opt;

    while((opt = getopt(argc, argv, ":v")) != -1)
    {
        switch(opt)
        {
            case 'v':
                verbose = true;
                break;
            case ':':
                printf("option %c needs a value\n", optopt);
                exit(EXIT_FAILURE);    
                break;
            case '?':
                printf("unknown option %c\n", opt);
                exit(EXIT_FAILURE);    
                break;
        }
    }
    
    if (optind >= argc)
    {
        if (verbose)
            printf("no config file and server no specified\n");
        exit(EXIT_FAILURE);    
    }

    /** Read config file name */
    strncpy(config_file, argv[optind], strlen(argv[optind]) + 1);
    optind++;

    /** Read server line no */
    if (optind >= argc)
    {
        if (verbose)
            printf("no server line no. specified\n");
        exit(EXIT_FAILURE);    
    }
    *server_no = atoi(argv[optind]);
}


int main(int argc, char *argv[])
{
    /** Register the signal handler */
    //signal(SIGINT, handle_sigint);

    /** Process the command-line args */
    char ip_address[IP_ADDRESS_LEN];
    int server_port_no;
    char config_file[256];
    int server_no;

    parse_args(argc, argv, config_file, &server_no);

    /** Read ip address and port no from config file */
    bool res = read_server_config(config_file, server_no, ip_address, &server_port_no);

    if (res == FAILURE)
        exit(EXIT_FAILURE);

    run_storage_server(ip_address, server_port_no);

    exit(EXIT_SUCCESS);
}
