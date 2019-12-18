#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <map>
#include <string.h>
#include "data_types.h"

int is_exit = 0;
int num_gserver_fd = 0;
extern int myserver_id;
struct sockaddr_in sin_gp;
socklen_t sin_length = sizeof(struct sockaddr_in);
void server_close(int server_fd);
extern std::map<int, char*> server_group;
std::map<int, int>sid2cfd;
int gserver_fd[10];

//Socket creation
int socket_create() {

  int fd;
  fd = socket(AF_INET, SOCK_STREAM, 0);

  return fd;
}


//Socket connect to name space
int sock_connect(int fd, char server_ip[], int remote_port){ 
    int ret;
    sin_gp.sin_family  = AF_INET;
    sin_gp.sin_port = htons(remote_port);
    if(server_ip == NULL) {
      printf("server_ip illegal value\n");
      exit(-1);
    }
    #ifdef DEBUG
    printf("server_ip in sock_connect %d\n", server_ip);
    printf("remote_port: %d\n", remote_port);
    #endif
    sin_gp.sin_addr.s_addr = inet_addr(server_ip);
       
    ret = connect(fd, (struct sockaddr *)&sin_gp, sin_length);
    if (ret < 0) {
    	perror("connect on server_fd failed \n");
    	return -1;
    }


    return ret;
    
}


//Initialization thread
int init_connection(int *server_fd_addr, char server_ip[], int remote_port) {
  int ret;
  //call socket_create
  *server_fd_addr = socket_create();
  if(*server_fd_addr < 0) {
    perror("Socket creation failed \n");
    return -1;
  }

  //Fill in ip and port of server.
  ret = sock_connect(*server_fd_addr, server_ip, remote_port);
  if(ret < 0) {
    perror("CONNECT FAILED \n");
    server_close(*server_fd_addr);
    return -1;
  }

  return ret;
}

//close server connection
void server_close(int server_fd) {

//release resources
close(server_fd);

}


int init_client_gossip(int myport)
{

  std::map<int, char*>::iterator it;
  char buf[30];
  char buf2[30];
  int config_count = 1;
  logging_consensus log_struct;

  sprintf(buf, "Hello world %d", myport);
  sprintf(buf2, "%d:0@%s", myserver_id, buf);

  for (it = server_group.begin(); it != server_group.end(); ++it, config_count++)
  {
    int gport = it->first + GOSSIP_OFFSET;
//    printf("myport: %d\n", myport); 
  //  printf("gport is %d\n", gport);
    #ifdef DEBUG
    printf("it->first %d\n", it->first);
    printf("it->second %s\n", it->second);
    #endif
    if(gport != myport) {
      //connect(gport);
      int ret = init_connection(&gserver_fd[config_count], it->second, gport);
      if(ret < 0) {
        printf("init connection failed\n");
      }
            sid2cfd.insert(std::pair<int, int>(config_count, gserver_fd[config_count]));
            printf("config_count: %d, gserver_fd[%d]: %d myserver_id: %d", config_count, config_count, gserver_fd[config_count], myserver_id);
/*
      log_struct.primaryId = -1;
      log_struct.glbl_seq_num = -1;
      log_struct.requestor_id = myserver_id;
      log_struct.seq_num = 0;
    memcpy(log_struct.data, buf, strlen(buf));
      ret = send(gserver_fd[num_gserver_fd], &log_struct, sizeof(logging_consensus), 0);
      if(ret < 0) {
        printf("send failed\n");
      }
      
      sleep(1);
      ret = send(gserver_fd[num_gserver_fd], buf2, strlen(buf2), 0);
      if(ret < 0) {
        printf("send failed\n");
      }*/
      num_gserver_fd++;
    #ifdef DEBUG
      printf("gport is %d\n", gport);
      #endif
    } else {
            gserver_fd[config_count] = -1;
            sid2cfd.insert(std::pair<int, int>(config_count, gserver_fd[config_count]));
            printf("num_gserver_fd: %d, gserver_fd[%d]: %d", config_count, config_count, gserver_fd[config_count]);
    }
  }

}


