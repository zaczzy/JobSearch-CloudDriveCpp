#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include "mailservice.h"
#include "data_types.h"

//#define DEBUG
#define RECV_BUF_SIZE 256
#define LINE_BUF_SIZE 256
//Global variables for program
int remote_port; 
int is_exit = 0;
int state = 0;
char *server_ip;
struct sockaddr_in sin_mail;
socklen_t sin_length = sizeof(struct sockaddr_in);
char log_message[100];
int log_to_terminal = 0;
FILE *log_file = NULL;
char *user = "michal";
char email_msg[1000];
char subject[20] = "Subject";
char rcpt_to[100];
int msg_len = 0;
int email_index = 0;
void server_close(int server_fd);

//Signal SIGINT callback
void sigint_callback(int sig) {
		
	sprintf(log_message, "Inside sigint_callback for sig %d\n", sig);
	printf("Inside sigint_callback for sig %d\n", sig);
	memset(log_message, 0, sizeof(log_message));
//	server_listen_flag = 0;
	is_exit = 1;

}

void server_recv_callback(char *recv_buf, int server_fd) {
 
    memset(recv_buf, 0, RECV_BUF_SIZE);

    int received = recvfrom(server_fd, recv_buf, RECV_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&sin_mail, &sin_length);

    if ( received > 0) {
#ifdef DEBUG
      printf("Received %d bytes \n", received);
      printf("Data is : \n");
      for (int i = 0; i < received; i++)
        printf("%02x ", (unsigned char)recv_buf[i]);
      printf("\n");
#endif
      printf("%s\n", recv_buf);

    } else {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        printf("No message in buffer"); 
      else  
        perror("");
    }

}

void terminal_recv_callback(char *lineptr, int server_fd) {
 
    memset(lineptr, 0, LINE_BUF_SIZE);
    size_t len = LINE_BUF_SIZE;
    int received = getline(&lineptr, &len, stdin); 
    if ( received > 0) {
#ifdef DEBUG
      printf("Received %d bytes \n", received);
      printf("Data is : \n");
      for (int i = 0; i < received; i++)
        printf("%02x ", (unsigned char)lineptr[i]);
      printf("\n");
#endif
       
      char *parse_cmd = (char *)malloc(LINE_BUF_SIZE);
      memcpy(parse_cmd, lineptr, received);

      char *cmd0 = strtok(parse_cmd,":\x0A");
  
      if(cmd0 == NULL) {
        printf("Invalid command\n");
        return;
      }
      
      if(state == 0) {
        if(!strcasecmp(cmd0, "INBOX")) {
          /*Retrieve user inbox */
           printf("250 OK\n");
           char response[1000];
           get_mail_response resp;
           webserver_core(0, user, -1, NULL, NULL, subject, response, server_fd, &resp);
           memset(&resp, 0, sizeof(get_mail_response));
           memset(response, 0, sizeof(response));
           state = 0;

          // send(server_fd, response, strlen(response), 0);
        } else if(!strcasecmp(cmd0, "READ")) {
              /*Read email for current user */

             char *cmd1 = strtok(NULL, "\x0A");
              if(cmd1 == NULL) {
                printf("500 ERR: Invalid command\n");
                return;
               }
              
              email_index = atoi(cmd1); 
              char response[1000];
  
              printf("250 OK\n");
              webserver_core(1, user, email_index, NULL, NULL, subject, response, server_fd, NULL);
              memset(response, 0, sizeof(response));
              email_index = 0 ;
              state = 0;

            //  send(server_fd, response, strlen(response), 0);
  
        } else if(!strcasecmp(cmd0, "DELETE")) {
              /*Delete email for current user*/

             char *cmd1 = strtok(NULL, "\x0A");
              if(cmd1 == NULL) {
                printf("500 ERR: Invalid command\n");
                return;
               }
              
              email_index = atoi(cmd1); 
              char response[1000];
              printf("250 OK\n");
              webserver_core(3, user, email_index, NULL, NULL, subject, response, server_fd, NULL);
              memset(response, 0, sizeof(response));
              email_index = 0 ;
              state = 0;

              //printf("250 OK Delete email id:%d\n", email_id);
  
        } else if(!strcasecmp(cmd0, "SEND")) {
              printf("250 OK\n");
              state = 1;
              return;
  
       } else if(!strcasecmp(cmd0, "QUIT")) {
              printf("250 OK\n");
              is_exit = 1;
              return;
        } else {

            printf("500 ERR: Invalid command\n");
            return;

        }
      } else if(state == 1) {

        if(!strcasecmp(cmd0, "rcpt_to")) {
            //Validate user
            char *tmp = strtok(NULL, "\x0A");
            memcpy(rcpt_to, tmp, strlen(tmp));
            printf("250 OK\n");
            state = 2;

        } else if(!strcasecmp(cmd0, "quit")) {
            printf("250 OK\n");
            state  = 0;

           //Quiting edit mail
        } else {
          printf("Invalid command\n");
          return;
        }

      } else if(state == 2) {

        if(!strcasecmp(cmd0, "data")) {
            state = 3;
            //Accepting data

        } else {

          printf("500 ERR: Please enter DATA:<msg>\n");
          return;
        }
      
      
      } else if(state == 3) { 
          if(!strcasecmp(cmd0, "." )) {
            printf("250 OK DATA");
            email_msg[msg_len] = '\0';
            char response[1000];
            webserver_core(2, user, -1, email_msg, rcpt_to, subject,response, server_fd, NULL);  
            printf("%s\n", email_msg);
            memset(email_msg, 0, sizeof(email_msg));
            memset(rcpt_to, 0, sizeof(rcpt_to));
            memset(response, 0, sizeof(response));
            msg_len = 0;
            state = 0;
            return;
          }
           
          strcpy(email_msg + msg_len, cmd0);
          msg_len += strlen(lineptr);
  
      } 

      /*

      if(lineptr[0] != '\n') {
        ssize_t send_size = sendto(server_fd, lineptr, received - 1, 0, (struct sockaddr *)&sin, sin_length);
        if(send_size < 0) {
          perror("");
        }
      char *temp_buf = strtok(lineptr, " \n");
      if(temp_buf != NULL && !strcasecmp(temp_buf, "/quit"))
        is_exit = 1;
      }
      */
    } else {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        printf("No message in buffer"); 
      else  
        perror("");
    }
  return;
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

//close server connection
void server_close(int server_fd) {

//release resources
close(server_fd);

}


int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "*** Author: Nishanth Shyamkumar (ncshy)\n");
    exit(1);
  }

  //Create client socket
  server_ip = strtok(argv[1], ":");
  remote_port = atoi(strtok(NULL, ":"));

  int ret;
  int server_fd = -1;
  int maxfds = 3;
  fd_set readfds;
  FD_ZERO(&readfds);


  signal(SIGINT, &sigint_callback);

  //Initiate TCP connection
  ret = init_connection(&server_fd);
  if (ret < 0) {
    perror("init_connection failed\n");
  }

  maxfds++;
  FD_SET(server_fd, &readfds);
  FD_SET(STDIN_FILENO, &readfds);
  char recv_buf[RECV_BUF_SIZE];
  char line_buf[LINE_BUF_SIZE];
  struct timeval time;
  time.tv_sec = 10;

  while(!is_exit) {

 //   FD_SET(server_fd, &readfds);
    FD_SET(STDIN_FILENO, &readfds);

    uint8_t sel_ret = select(maxfds, &readfds, NULL, NULL, &time);
    if(sel_ret < 0) {
      perror("Select failed");
      continue;
    }
 
    if(FD_ISSET(server_fd, &readfds)) {
      server_recv_callback(recv_buf, server_fd);
      FD_CLR(server_fd, &readfds);
    }

    if(FD_ISSET(STDIN_FILENO, &readfds)) {
#ifdef DEBUG
    printf("In STDIN_FILENO\n");
#endif
      terminal_recv_callback(line_buf, server_fd);
      FD_CLR(STDIN_FILENO, &readfds);
    }

  }

  server_close(server_fd);
  return 0;
}  
