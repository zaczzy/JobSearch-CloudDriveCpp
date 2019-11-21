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
#include "echothread.h"

//TODO: Signal handling code

#define MAX_CONNECTIONS 10000

int is_verbose = 0;
int is_shutdown_server = 0;

/* Signal handler for CTRL+C */
void signal_interrupt(int sig_no) {
   
   is_shutdown_server = 1;

}

int main(int argc, char *argv[])
{
  
  int server_fd;
  int client_fd[MAX_CONNECTIONS];
  int ret;
  short int port_num = 10000;
  struct sockaddr_in server_addr;
  struct sockaddr_in client_addr[MAX_CONNECTIONS];
  pthread_t pth_client[MAX_CONNECTIONS];
  char opt;
  //char *client_buf[MAX_CONNECTIONS];

  if(argc > 5) {
    printf("USAGE: %s -p <custom port number> -a<print> -v<verbose>", argv[0]);
    exit(-1);
  }
    //parse input arguments using getopt()
  while ((opt = getopt(argc , argv,"avp:")) != -1) {
    switch(opt) {

    //Get port number if specified
        case 'p':
                  port_num = atoi(optarg); 
                  
                  if(port_num <= 1024 || port_num >= 65535) {
                    printf("EXITING!!Please enter a port number between 1024 and 65535\n");
                    exit(-1);
                  }
                  break;
        //Enable verbose mode
        case 'v':
                  is_verbose = 1;
                  break;
        case 'a':
                  printf("Nishanth Shyamkumar / ncshy\n");
                  exit(0);
        default:
                //Handle wrong arguments
                  printf("USAGE: %s -p <custom port number> -a<print> -v<verbose>", argv[0]);
                  exit(-1);

    }
  }
  //Initialize signal handler
  signal(SIGINT, &signal_interrupt);

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
  const char server_send_msg[60] = "+OK Server ready (Author: Nishanth Shyamkumar / ncshy)\r\n";
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

  return 0;
}
