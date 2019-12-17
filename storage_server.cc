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
#include <fcntl.h>
#include <vector>
#include <errno.h>
#include <unordered_map>

#include "data_types.h"
#include "logging.h"
#include "socket.h"
#include "thread.h"
#include "key_value.h"

#define IP_ADDRESS_LEN  16
//#define ADMIN_PORT      3333
#define MASTER_PORT     2333

bool verbose = false;
volatile bool terminate = false;
char ip_address[IP_ADDRESS_LEN];
int server_port_no;
bool restart = true;
unsigned short server_no, group_no;
extern int master_sockfd; 

volatile int fds[100];
std::unordered_map<int, char*> server_addresses; 
std::unordered_map<int, char*> server_group; 
uint8_t total_servers = 0;
uint8_t server_id;

void* run_server_for_admin(void* args);

void* run_storage_server(void* params)
{
    int server_fd;
    int client_count = 0;
    char success_msg[] = "+OK Server ready (Author: Team 24)\r\n";
    int msg_len = strlen(success_msg);

    printf("running storage server\n");
    /** Prepare the socket - create, bind, and listen to port */
    server_fd = prepare_socket(server_port_no);

    //int flags = fcntl(server_fd, F_GETFL, 0);
    //fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    if (server_fd == -1)
        exit(EXIT_FAILURE);

    /** Run until terminated by admin*/
    while(!terminate)
    {
        int *client_fd = new int;
        
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);

        /** Accept the connection */
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addrlen);

        /** Check if terminate flag is set, if yes terminate the fd immediately */
        if (terminate)
        {
            close(*client_fd);
            goto term;
        }

        if (*client_fd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            if (verbose)
                fprintf(stderr, "accept failed\n");
            exit(EXIT_FAILURE);
        }
        else if (*client_fd == -1)
        {
            continue;
        }
        
        fds[client_count++] = *client_fd;
        
        if (verbose)
            printf("accepted client connection\n");

        /** Send the connection OK msg */
        send_msg_to_socket(success_msg, msg_len, *client_fd);

        /** Create a separate thread for communication */
        create_thread(client_fd);
    }

term:
    close(server_fd);
    if (verbose)
        printf("Closing storage server\n");
    pthread_exit(NULL);

    return NULL;
}

bool read_server_config_master(char* config_file, unsigned short group_no, unsigned short server_no, char* ip_addr, int* port_no)
{
    /** Open config file */
    FILE* file = fopen(config_file, "r");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open server config file\n");

        return FAILURE;
    }
    
    unsigned short line_no = 0, position_no = 0;
    ssize_t ret;
    char *line, *token;
    size_t len = 0;
    memset(ip_addr, 0, IP_ADDRESS_LEN);
    *port_no = 0;

    while ((ret = getline(&line, &len, file)) != -1)
    {
        /** Remove the next line char */
        line[ret - 1] = '\0';

        line_no++;

        /** Check if the group no matches */
        if (line_no == group_no)
        {
            token = strtok(line, ",");

            while (token != NULL)
            {
                position_no++;
                
                /** Check if the server position no matches */
                if (position_no == server_no)
                {
                    /** Save this server's configuration */
                    int ip_len = 0;
                    char* ptr = token;

                    while(*ptr != ':' && *ptr != '\0')
                    {
                        ip_len++;
                        ptr++; 
                    }

                    if (ip_len > 0 && ip_len <= 32)
                    {
                        strncpy(ip_addr, token, ip_len);
                        *port_no = atoi(token + ip_len + 1);

                        printf("this server ip address: %s port no: %d\n", ip_addr, *port_no);
                    }
                }
                else
                {
                    /** Save configuration of other servers of this group */
                    char *ip = (char*)malloc(32);
                    int ip_len = 0;
                    char* ptr = token;

                    while(*ptr != ':' && *ptr != '\0')
                    {
                        ip_len++;
                        ptr++; 
                    }

                    if (ip_len > 0 && ip_len <= 32)
                    {
                        strncpy(ip, token, ip_len);
                        int port_no = atoi(token + ip_len + 1);
                        server_group.insert(std::make_pair(port_no, ip));
                        
                        printf("other server ip address: %s port no: %d\n", ip, port_no);
                    }
                }
                total_servers++;

                token = strtok(NULL, ",");
            }
        }
        else
        {
            /** Save other group servers info, if needed */
        }
    }

    if (verbose)
        printf("total servers in group: %u\n", total_servers);
    
    return SUCCESS;
}

bool read_server_config(char* config_file, unsigned short server_no, char* ip_address, int* port_no)
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

void parse_args(int argc, char *argv[], char* config_file, unsigned short* group_no, unsigned short* server_no)
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

    /** Read group no */
    if (optind >= argc)
    {
        if (verbose)
            printf("no group no. specified\n");
        exit(EXIT_FAILURE);    
    }
    *group_no = atoi(argv[optind++]);

    /** Read server no */
    if (optind >= argc)
    {
        if (verbose)
            printf("no server line no. specified\n");
        exit(EXIT_FAILURE);    
    }
    *server_no = atoi(argv[optind]);
}

void create_socket_for_master()
{
    if (verbose)
        printf("running client for communication with master\n");
    struct sockaddr_in servaddr; 
  
    // socket create and varification 
    master_sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    if (master_sockfd == -1) { 
        printf("socket creation failed for connection with master\n"); 
        exit(EXIT_FAILURE); 
    } 
    else
    {
        printf("Socket successfully created..\n"); 
    }
    
    bzero(&servaddr, sizeof(servaddr)); 
  
    // assign IP, PORT 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    servaddr.sin_port = htons(MASTER_PORT); 
  
    // connect the client socket to server socket 
    if (connect(master_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) { 
        printf("connection with the master failed...\n"); 
    } 
    else
    {
        printf("connected to the master server..\n"); 
    }
}
void* read_admin_commands(int client_fd)
{
    char buffer[2048];
    while(true)
    {
        memset(buffer, 0, sizeof(buffer));

        /** Read the command from the client */
        int ret;
        ret = read(client_fd, buffer, sizeof(buffer));

        if (ret == -1)
        {
            printf("error code : %s\n", strerror(errno));
        }
        if (ret == 0)
        {
            break;
        }

        if (ret < 0)
        {
            // ERror
            exit(EXIT_FAILURE);
        }

        printf("command received from admin: %s\n", buffer);
        
        if (strncmp(buffer, "disable", strlen("disable")) == 0)
        {
            terminate = true;
            
            /** Clear the map */
            clear_tablet();

            /** Close the socket to master */
            close(master_sockfd);

            /** TODO: Free locks if acquiree any */
        }
        else if (strncmp(buffer, "restart", strlen("restart")) == 0)
        {
            terminate = false;

            /** Replay logs from log file */
            replay_log();

            /** Open the connection to master */
            create_socket_for_master();

            /** Run the main listener thread again */
            //pthread_t thread;
            //int iret1 = pthread_create(&thread, NULL, run_storage_server, NULL);

            //if (iret1 != 0)
            //{
            //    if  (verbose)
            //        fprintf(stderr, "Error creating thread\n");
            //    exit(EXIT_FAILURE);
            //}
        }
    }
    return NULL; // zac inserted
}


void* run_server_for_admin(void* args)
{
    if (verbose)
        printf("running server for admin\n");

    int admin_port = (group_no * 1200) + server_no;
    if (verbose)
        printf("admin port : %d\n", admin_port);
    int fd = prepare_socket(admin_port);

    if (fd == -1)
    {
        if (verbose)
            printf("Couldn't create socket for communication with admin \n");
        exit(EXIT_FAILURE);
    }

    /** Run until terminated by a SIGINT signal */
    while(true)
    {
        int client_fd;
        client_fd = -1;
        struct sockaddr_in client_addr;
        socklen_t client_addrlen = sizeof(client_addr);

        /** Accept the connection */
        client_fd = accept(fd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addrlen);

        if (client_fd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) 
        {
            if (verbose)
                fprintf(stderr, "admin client accept failed\n");
            exit(EXIT_FAILURE);
        }

        if (verbose)
            printf("accepted client connection\n");

        read_admin_commands(client_fd); 
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    /** Register the signal handler */
    //signal(SIGINT, handle_sigint);

    /** Process the command-line args */
    char config_file[256];

    parse_args(argc, argv, config_file, &group_no, &server_no);

    /** Read ip address and port no from config file */
    bool res = read_server_config_master(config_file, group_no, server_no, ip_address, &server_port_no);

    if (res == FAILURE)
        exit(EXIT_FAILURE);

    /** Create and write 0 to log sequence no file */
    FILE* fd = fopen(LOG_SEQ_NO_FILE, "w");

    if (fd == NULL)
    {
        if (verbose)
            printf("Couldn't create log seq file\n");
        exit(EXIT_FAILURE);
    }

    unsigned long long seq_no = 0;
    fprintf(fd, "%llu", seq_no);
    fclose(fd);

    /** Create and write 0 to checkpoint version no file */
    FILE* c_fd = fopen(CHECKPOINT_VERSION_FILE,  "w");

    if (c_fd == NULL)
    {
        if (verbose)
            printf("Couldn't create log seq file\n");
        exit(EXIT_FAILURE);
    }

    unsigned long long version_no = 0;
    fprintf(fd, "%llu", version_no);
    fclose(c_fd);

    pthread_t thread;
    int iret1 = pthread_create(&thread, NULL, run_server_for_admin, NULL);

    if (iret1 != 0)
    {
        if  (verbose)
            fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_t thread2;
    int iret2 = pthread_create(&thread2, NULL, run_storage_server, NULL);

    if (iret2 != 0)
    {
        if  (verbose)
            fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }

    /** Create socket for communication with master */
    create_socket_for_master();

    pthread_join(thread, NULL);
    pthread_join(thread2, NULL);
    exit(EXIT_SUCCESS);
}
