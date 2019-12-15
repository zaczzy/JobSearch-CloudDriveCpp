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

//#define DEBUG
#define RECV_BUF_SIZE 256
#define LINE_BUF_SIZE 256
//Global variables for program
int remote_port; 
int is_exit = 0;
char *server_ip;
struct sockaddr_in sin;
socklen_t sin_length = sizeof(struct sockaddr_in);
char log_message[100];
int log_to_terminal = 0;
FILE *log_file = NULL;

void server_close(int server_fd);

//Signal SIGINT callback
void sigint_callback(int sig) {
		
	sprintf(log_message, "Inside sigint_callback for sig %d\n", sig);
	fprintf(log_file, "DEBUG : %s", log_message);
	memset(log_message, 0, sizeof(log_message));
//	server_listen_flag = 0;
	is_exit = 1;

}

void server_recv_callback(char *recv_buf, int server_fd) {
 
    memset(recv_buf, 0, RECV_BUF_SIZE);

    int received = recvfrom(server_fd, recv_buf, RECV_BUF_SIZE, MSG_DONTWAIT, (struct sockaddr *)&sin, &sin_length);

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

      /*TODO: Command handling.
  
          Handle 4 commands: 1) GET INBOX
                             2) GET MAIL
                             3) SEND MAIL
                             4) DELETE MAIL

      */


      if(lineptr[0] != '\n') {
        ssize_t send_size = sendto(server_fd, lineptr, received - 1, 0, (struct sockaddr *)&sin, sin_length);
        if(send_size < 0) {
          perror("");
        }
      char *temp_buf = strtok(lineptr, " \n");
      if(temp_buf != NULL && !strcasecmp(temp_buf, "/quit"))
        is_exit = 1;
      }
    } else {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
        printf("No message in buffer"); 
      else  
        perror("");
    }

}

//Socket creation
int socket_create() {

  int fd;
  fd = socket(AF_INET, SOCK_STREAM, IPPROTO_UDP);

  return fd;
}


//Socket connect to name space
int sock_connect(int fd){ 
    int ret;
    sin.sin_family  = AF_INET;
    sin.sin_port = htons(remote_port);
    if(server_ip == NULL) {
      printf("server_ip illegal value\n");
      exit(-1);
    }
    sin.sin_addr.s_addr = inet_addr(server_ip);
       
    ret = connect(fd, (struct sockaddr *)&sin, sin_length);
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
fclose(log_file);

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
  fd_set readfds;
  FD_ZERO(&readfds);


  signal(SIGINT, &sigint_callback);

  //Initiate TCP connection
  ret = init_connection(&server_fd);
  if (ret < 0) {
    perror("init_connection failed\n");
  }

  FD_SET(server_fd, &readfds);
  FD_SET(STDIN_FILENO, &readfds);
  char recv_buf[RECV_BUF_SIZE];
  char line_buf[LINE_BUF_SIZE];
  struct timeval time;
  time.tv_sec = 10;

  while(!is_exit) {

    FD_SET(server_fd, &readfds);
    FD_SET(STDIN_FILENO, &readfds);

    uint8_t sel_ret = select(server_fd + 1, &readfds, NULL, NULL, &time);
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
