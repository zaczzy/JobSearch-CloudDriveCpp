#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/file.h>
#include<time.h>
#include"smtp_thread.h"
#include"mailservice.h"
#define BUFLENGTH 1000

enum smtp_state {
  INVALID = 0,
  SESSION,
  FROM,
  RECV,
  DATA
};

extern int is_verbose;
extern int is_shutdown_server;
extern char dir_name[100];
extern int backend_fd;
extern pthread_mutex_t glbl_mutex;

smtp_state operator ++( smtp_state &id, int )
{
  smtp_state currentID = id;

  id = static_cast<smtp_state>( id + 1 );

  return ( currentID );
}

int check_state(enum smtp_state cur_state, enum smtp_state test_state) {

  if(test_state != cur_state)
    return -1;

  return 0;  

}

int send_message(int client_fd, const void *send_msg, size_t len) {
    
    int ret = send(client_fd, send_msg, len, 0);
    if(ret < 0) {
      perror("SEND message FAIL\n");
    }

    if(is_verbose)
      printf("[%d] S: %s", client_fd, (char *)send_msg);

    return ret;

}
/* Thread function to handle communication 
   with client. */
void *client_handler(void *args) {

  int client_fd = *(int *)args;
  // Linear buffer which can be expanded into circular buffer later 
  char *circ_buf = (char *)malloc(sizeof(char) * BUFLENGTH);
  if(circ_buf == NULL) {
    printf("Buffer for child is NULL\n");
    return NULL;
  }

  enum smtp_state cur_state = INVALID;
  //int cur_state = INVALID;
  char *read_ptr = circ_buf;
  char *temp_ptr;
  char *write_ptr = circ_buf;
  //Buffers for command and send message
  char recv_msg[50];
  char send_msg[60];
  char from_msg[100];
  char cmd[30] = "";
  char from_user[100]; 
  char from_domain[40];
  char rcpt_user[100]; 
  char rcpt_domain[40];
  int bytes_read = 0;
  int is_quit = 0;
  int smtp_code = 250;
  int err_code = 550;
  int cmd_err_code = 500;
  //Flag specifying data availability.
  int is_data = 0;
  FILE *fp[20];
  int open_file_cnt = 0;
  char dir[100];
  //Buffer to hold message written into file
  char file_msg[MAX_MSG_LENGTH];
  //track bytes written to above buffer.
  int bytes_written = 0;
  int first_time = 0;
  char response[100];

  memset(cmd, 0, sizeof(cmd));
  memset(dir, 0, sizeof(dir));
  memset(file_msg, 0, sizeof(file_msg));
  memset(from_user, 0, sizeof(from_user));
  memset(from_domain, 0, sizeof(from_domain));
  memset(rcpt_user, 0, sizeof(rcpt_user));
  memset(rcpt_domain, 0, sizeof(rcpt_domain));

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

      while(temp_ptr < write_ptr) {
        if(*temp_ptr == '\r' && *(temp_ptr+1) == '\n') {
          temp_ptr += 2;
          int msg_len = temp_ptr - read_ptr;
          memcpy(recv_msg, read_ptr, msg_len);
          read_ptr = temp_ptr;
          if(is_verbose)
            printf("[%d] C :%s", client_fd, recv_msg);

          if(is_data == 1) {
            //Write using opened fd
            if(!strcasecmp(recv_msg, ".\r\n")) {
              sprintf(send_msg, "%d OK\r\n", smtp_code);
              is_data = 0;
              //lock write with mutex
              pthread_mutex_lock(&glbl_mutex);
              /*for(int j = 0; j < open_file_cnt; j++) {
                int fd = fileno(fp[j]);
                flock(fd, LOCK_EX);
                fwrite(file_msg, bytes_written, 1 ,fp[j]);
                flock(fd, LOCK_UN);
                fclose(fp[j]); 
                fp[j] = NULL;
              }*/
              //unlock mutex
              pthread_mutex_unlock(&glbl_mutex);
              printf("from_user:%s\n, rcpt_user:%s\n, data: %s\n", from_user, rcpt_user, file_msg);
              cur_state =  SESSION;
              send_message(client_fd, send_msg, strlen(send_msg));
              webserver_core(2, from_user, -1, file_msg, rcpt_user, "Subject", response, backend_fd, NULL);
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(file_msg, 0, sizeof(recv_msg));
              bytes_written = 0;
              //temp_ptr++;
              continue;  
            } else if(!strcasecmp(recv_msg,"\r\n")) {		    
              memcpy(file_msg + bytes_written, recv_msg, msg_len);
              bytes_written += msg_len;
              memset(recv_msg, 0, sizeof(recv_msg));
              //temp_ptr++;
              continue;   
	    } else {  
              if(!first_time) {
                memcpy(file_msg + bytes_written, from_msg, strlen(from_msg));
                bytes_written += strlen(from_msg);
                first_time++;
              }
              memcpy(file_msg + bytes_written, recv_msg, msg_len);
              bytes_written += msg_len;
              memset(recv_msg, 0, sizeof(recv_msg));
              //temp_ptr++;
              continue;  
            }
              
          }

          int i = 0;
          while(recv_msg[i] != ':' && recv_msg[i] != '\r') i+=1;
          memcpy(cmd, recv_msg, i);

	  char helo_cmd[50];
	  memset(helo_cmd, 0, sizeof(helo_cmd));
	  char *helo_str;
	  memcpy(helo_cmd, recv_msg, i);
          //Check for valid command
	  helo_str = strtok(helo_cmd, " ");
    if(helo_str == NULL) {
    
     sprintf(send_msg, "%d Unknown command\r\n", cmd_err_code);

    } else if(!strcasecmp(helo_str, "HELO")) {
            if(check_state(cur_state, INVALID) < 0) {
              sprintf(send_msg, "%d Send commands in order\r\n", err_code);
            } else {
	      helo_str = strtok(NULL, " ");
	      if(helo_str == NULL) {
		      sprintf(send_msg, "%d Unrecognizable host\r\n", err_code);
		      send_message(client_fd, send_msg, strlen(send_msg));
		      memset(recv_msg, 0, sizeof(recv_msg));
		      memset(cmd, 0, sizeof(cmd));
		      temp_ptr++;
		      continue;  
	      }
	      sprintf(send_msg, "%d localhost\r\n", smtp_code);
              cur_state++;
              printf("cur_state: %d\n", cur_state);
           }
          } else if(!strcasecmp(cmd, "QUIT")) {
            is_quit = 1;
            sprintf(send_msg, "221 OK Goodbye!\r\n");
            printf("%s\n", send_msg);
            send(backend_fd, send_msg, strlen(send_msg), 0);
          } else if(!strcasecmp(cmd, "RCPT TO")) {
            if((check_state(cur_state,  FROM) < 0) && (check_state(cur_state,  RECV) < 0)) {
              sprintf(send_msg, "%d Send commands in order\r\n", err_code);
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            }
            cur_state =  RECV;
            //Parse using@
            char *temp = &recv_msg[i];
            while(*temp != '@' && *temp != '\0') temp++; //Do nothing

            if(*temp == '\0') {
              sprintf(send_msg, "550 No such domain!\r\n");
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            }
            
            memset(rcpt_user, 0, sizeof(rcpt_user));
            memset(rcpt_domain, 0, sizeof(rcpt_domain));
            
            memcpy(rcpt_user, (recv_msg + i + 2), (temp - (recv_msg + i + 2)));
            memcpy(rcpt_domain, temp + 1, (recv_msg + msg_len - 3) - (temp + 1));

            printf("rcpt_user : %s\n", rcpt_user);
            printf("rcpt_domain: %s\n", rcpt_domain);

            //If no localhost specified, fail.
            if(strcasecmp(rcpt_domain, "localhost")) {
              sprintf(send_msg, "550 No such domain!\r\n");
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            } 

            //Try opening file name.mbox
          /*  sprintf(dir,"%s/%s.mbox", dir_name, rcpt_user);

            if(access(dir, F_OK) == -1) {
              sprintf(send_msg, "550 No such user!\r\n");
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            }

            fp[open_file_cnt] = fopen(dir, "a+");

            if(fp[open_file_cnt] == NULL) {
              sprintf(send_msg, "550 No such user!\r\n");
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            } else {
              sprintf(send_msg, "%d OK\r\n", smtp_code);
              //Write initial message using opened fd. 
              open_file_cnt++;

            }
*/            
            //If fails, return error.
            sprintf(send_msg, "%d OK\r\n", smtp_code);
          
          } else if(!strcasecmp(cmd, "MAIL FROM")) {

            if(check_state(cur_state,  SESSION) < 0) {
              sprintf(send_msg, "%d Send commands in order\r\n", err_code);
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            }
            cur_state++;
            printf("cur_state: %d\n", cur_state);
            //Parse using @
            char *temp = &recv_msg[i];
            
            while(*temp != '@' && *temp != '\0')temp++; //Do nothing

            if(*temp == '\0') {
              sprintf(send_msg, "550 No such domain!\r\n");
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            }
            memset(from_user, 0, sizeof(from_user));
            memset(from_domain, 0, sizeof(from_domain));
            memcpy(from_user, (recv_msg + i + 2), (temp  - (recv_msg + i + 2)));
            memcpy(from_domain, temp + 1, (recv_msg + msg_len - 3) - (temp + 1));

            time_t msg_time = time(0);
            
            sprintf(from_msg, "From %s@%s %s\r\n", from_user, from_domain, ctime(&msg_time));
            
            sprintf(send_msg, "%d OK\r\n", smtp_code);
          
          } else if(!strcasecmp(cmd, "DATA")) {

            if(check_state(cur_state,  RECV) < 0) {
              sprintf(send_msg, "%d Send commands in order\r\n", err_code);
              send_message(client_fd, send_msg, strlen(send_msg));
              memset(recv_msg, 0, sizeof(recv_msg));
              memset(cmd, 0, sizeof(cmd));
              temp_ptr++;
              continue;  
            }
            cur_state++;
            printf("cur_state: %d\n", cur_state);

            sprintf(send_msg, "354 Data\r\n");
            is_data = 1;
          } else if(!strcasecmp(cmd, "NOOP")) {
            //Do nothing operation 
            memset(recv_msg, 0, sizeof(recv_msg));
            memset(cmd, 0, sizeof(cmd));
            temp_ptr++;
            continue;
          } else {
            sprintf(send_msg, "%d Unknown command\r\n", cmd_err_code);
          }
          /*if(is_verbose)
            printf("[%d] S :%s", client_fd, send_msg);
            */
          //Send message to client
          send_message(client_fd, send_msg, strlen(send_msg));

          memset(recv_msg, 0, sizeof(recv_msg));
          memset(cmd, 0, sizeof(cmd));
         
        }
        temp_ptr++;
      }
        
  }

  close(client_fd);
  if(is_verbose)
    printf("[%d] Connection closed\n", client_fd);
  free(circ_buf);
  int i = 0;

  while(i < open_file_cnt) {
    if(fp[i] != NULL) {
      fclose(fp[i]);
      fp[i] = NULL;
    }
    i++;  
  }

  return NULL; 
}
