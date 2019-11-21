#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#define BUFLENGTH 1000

extern int is_verbose;
extern int is_shutdown_server;

/* Thread function to handle communication 
   with client. */
void *client_handler(void *args) {

  int client_fd = *(int *)args;
  // Linear buffer which can be expanded into circular buffer later 
  char *circ_buf = malloc(sizeof(char) * BUFLENGTH);
  if(circ_buf == NULL) {
    printf("Buffer for child is NULL\n");
    return;
  }

  char *read_ptr = circ_buf;
  char *temp_ptr;
  char *write_ptr = circ_buf;
  //Buffers for command and send message
  char msg[50];
  char send_msg[50];
  char cmd[30] = "";
  int bytes_read = 0;
  int is_quit = 0;

  memset(cmd, 0, sizeof(cmd));

  while(!is_quit && !is_shutdown_server) {
      //Non blocking recv
      bytes_read = recv(client_fd, write_ptr, BUFLENGTH, MSG_DONTWAIT);
      if(bytes_read <= 0)
        if(errno == EAGAIN || errno == EWOULDBLOCK)
          continue;
        else
          break;
      //Track write and read pointers for buffer 
      write_ptr += bytes_read;

      temp_ptr = read_ptr;

      while(temp_ptr <= write_ptr) {
        if(*temp_ptr == '\r' && *(temp_ptr+1) == '\n') {
          temp_ptr += 2;
          memcpy(msg, read_ptr, temp_ptr - read_ptr);
          read_ptr = temp_ptr;
          if(is_verbose)
            printf("[%d] C :%s", client_fd, msg);

          int i = 0;
          while(msg[i] != ' ' && msg[i] != '\0' && msg[i] != '\r') i+=1;
          memcpy(cmd, msg, i);
          //Check for valid command
          if(!strcmp(cmd, "ECHO")) {
            sprintf(send_msg, "+OK%s", msg + i);
          } else if(!strcmp(cmd, "QUIT")) {
            is_quit = 1;
            sprintf(send_msg, "+OK Goodbye!%s", msg + i);
            int a = 255;
            send(client_fd, &a, sizeof(int), 0);
          } else {
            sprintf(send_msg, "-ERR Unknown command\r\n");
          }
          if(is_verbose)
            printf("[%d] S :%s", client_fd, send_msg);
          //Send message to client
          send(client_fd, send_msg, strlen(send_msg), 0);

          memset(msg, 0, sizeof(msg));
          memset(cmd, 0, sizeof(cmd));
         
        }
        temp_ptr++;
      }
        
  }

  close(client_fd);
  if(is_verbose)
    printf("[%d] Connection closed\n", client_fd);
  free(circ_buf);
  return; 
}
