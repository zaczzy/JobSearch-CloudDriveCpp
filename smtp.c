#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include "smtp_thread.h"

//TODO: Signal handling code

#define MAX_CONNECTIONS 10000

int is_verbose = 0;
int is_shutdown_server = 0;
char dir_name[100];
pthread_mutex_t glbl_mutex;
char *server_ip;
int remote_port; 
int backend_fd;
struct sockaddr_in sin_mail;
socklen_t sin_length = sizeof(struct sockaddr_in);

/* Signal handler for CTRL+C */
void signal_interrupt(int sig_no) {
   
   is_shutdown_server = 1;

}
//Socket creation
int socket_create() {

  int fd;
  fd = socket(AF_INET, SOCK_STREAM, 0);

  return fd;
}


//Socket connect to name space
int sock_connect(int fd){ 
    int ret;
    sin_mail.sin_family  = AF_INET;
    sin_mail.sin_port = htons(remote_port);
    if(server_ip == NULL) {
      printf("server_ip illegal value\n");
      exit(-1);
    }
    sin_mail.sin_addr.s_addr = inet_addr(server_ip);
       
    ret = connect(fd, (struct sockaddr *)&sin_mail, sin_length);
    if (ret < 0) {
    	perror("connect on server_fd failed \n");
    	return -1;
    }
    printf("ret from connect was %d\n", ret);


    return ret;
    
}

//close server connection
void server_close(int server_fd) {

//release resources
close(server_fd);

}

//Initialization thread
int init_connection(int *server_fd_addr) {
  int ret;
  //call socket_create
  *server_fd_addr = socket_create();
  if(*server_fd_addr < 0) {
    perror("Socket creation failed \n");
    return -1;
  }

  //Fill in ip and port of server.
  ret = sock_connect(*server_fd_addr);
  if(ret < 0) {
    perror("CONNECT FAILED \n");
    server_close(*server_fd_addr);
    return -1;
  }

  return ret;
}


int main(int argc, char *argv[])
{
  int server_fd; 
  int client_fd[MAX_CONNECTIONS];
  int ret;
  short int port_num = 2500;
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr[MAX_CONNECTIONS];
  pthread_t pth_client[MAX_CONNECTIONS];
  char opt;
  char buf[MAX_MSG_LENGTH];
  //char *client_buf[MAX_CONNECTIONS];

  if(argc > 10) {
    printf("USAGE: %s -p <custom port number> -a<print> -v<verbose> -s<backend ip> -r<backend port>", argv[0]);
    exit(-1);
  }
    //parse input arguments using getopt()
  while ((opt = getopt(argc , argv,"avdp:s:r:")) != -1) {
    switch(opt) {
        printf("opt:%d\n", opt);
    //Get port number if specified
        case 'p':
                  port_num = atoi(optarg); 
                  
                  if(port_num <= 1024 || port_num >= 65535) {
                    printf("EXITING!!Please enter a port number between 1024 and 65535\n");
                    exit(-1);
                  }
                  break;
        case 'r':
                  remote_port = atoi(optarg); 
                  
                  if(remote_port <= 1024 || remote_port >= 65535) {
                    printf("EXITING!!Please enter a port number between 1024 and 65535\n");
                    exit(-1);
                  }
                  break;
        case 's':
                  server_ip = strtok(optarg, ":");
                  break;
        //Enable verbose mode
        case 'v':
                  is_verbose = 1;
                  break;
        case 'a':
                  printf("Group 24\n");
                  exit(0);
        case 'd':
                  //Initiate TCP connection to storage
                  printf("Initiating connection\n");
                  ret = init_connection(&backend_fd);
                  if (ret < 0) {
                    perror("init_connection failed\n");
                  }
                  break;
        default:
                //Handle wrong arguments
                  printf("USAGE: %s -p <custom port number> -a<print> -v<verbose>", argv[0]);
                  exit(-1);

    }
  }
  //Initialize signal handler
  signal(SIGINT, &signal_interrupt);

  ret = pthread_mutex_init(&glbl_mutex, NULL);
  if(ret < 0) {
    printf("MUTEX INIT FAIL:");
    exit(-1);
  }

  //Open socket with NONBLOCK set
  server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);  
  if(server_fd < 0)
    perror("ERROR on SOCKET call\n");

  server_addr.sin_family = AF_INET;  
  server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  server_addr.sin_port = htons(port_num);
  //Bind socket
  ret = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
  if(ret < 0) {
    perror("ERROR on BIND\n");
  }
  //Listen on socket
  ret = listen(server_fd, MAX_CONNECTIONS);
  if(ret < 0) {
    perror("ERROR on LISTEN\n");
  }
  
  int i = 0;
  int num_connections = 0;

  socklen_t addrlen = sizeof(struct sockaddr_in);
  const char server_send_msg[60] = "220 localhost (Author: Nishanth Shyamkumar / ncshy)\r\n";
  //Accept client connections and start child threads
  while(i < MAX_CONNECTIONS && !is_shutdown_server) {
    client_fd[i] = accept(server_fd, (struct sockaddr *)&client_addr[i], &addrlen);
    
    if(client_fd[i] < 0) {
      if(errno == EAGAIN) {
        continue;
      } else {
        perror("ERROR on ACCEPT\n");
        i++;
      }
      break;
    }

    send(client_fd[i], server_send_msg, strlen(server_send_msg), 0);
    if(is_verbose)
      printf("[%d] New connection\n", client_fd[i]);
    //Thread creation to communicate with client
    ret = pthread_create(&pth_client[i], 0, &client_handler, &client_fd[i]);
    if(ret < 0) {
      perror("ERROR on THREAD CREATE\n");
    }

    i++;
    num_connections = i;
  }
  printf("EXIT loop\n");

  /* Parent waits for child threads to complete before exiting */
  for(int i = 0; i < num_connections; i++) {
    ret = pthread_join(pth_client[i], NULL);
    printf("[%d] Connection closed\n", client_fd[i]);
    if(ret < 0) {
      perror("ERROR on THREAD JOIN\n");
    }
  }
  ret = close(server_fd);
  if(ret < 0) {
    perror("SERVER FD CLOSE FAIL:");
  }
  
  ret = pthread_mutex_destroy(&glbl_mutex);
  if(ret < 0) {
    printf("MUTEX DESTROY FAIL:");
  }
  return 0;
}
