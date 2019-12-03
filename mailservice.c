#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "data_types.h"
#include "mailservice.h"
#include <time.h>

#define CELLSIZE 1000

enum mail_opt {
    
    SHOW_MAIL =0,
    READ_MAIL,
    SEND_MAIL,
    DELETE_MAIL

}mOpt;


struct email_data {

 uint16_t email_ID; 
 char Date[50];
 char Sender[30];
 char Message[CELLSIZE];

};

time_t rawtime;
struct tm * timeinfo;


int webserver_core(int mailOpt, char *user, int email_id, char *mail_msg, char *rcpt_user, char *html_response, int server_fd) {
  int valid = FAILURE;    //Defensive programming
  char send_msg[1000];
  char recv_msg[1000];
//Invokes backend to enquire about user
  if(user == NULL) {
    printf("Invalid user\n");
    return FAILURE;
  }

  int validate = validateUser(user);
  if(validate < 0) {
    printf("Invalid user %s\n", user);
    return FAILURE;
  }

  mOpt = (enum mail_opt)mailOpt;

  switch(mOpt) {
    case SHOW_MAIL:  {               //Downloads email headers from backend
#ifdef DEBUG
      printf("SHOW_MAIL in core\n");
#endif
      printf("User: %s\n\n", user);
      char subject[20] = "My struggles";
      char date[20] = "Sept 1st, 3:14";
      sprintf(send_msg, "<!doctype html>\n<html>\n\n<head>\n\n\t<title>\n\t\tUser inbox\n\t\t\t</title>\n\n</head>\n\n<body>\n\n\t<ol>\n\t\t<li>\n\t\t\tInbox:\n\t\t\t<ul>\n\t\t\t\t<li><pre> <b>Charles Aranguiz</b>     My travels     Monday 14th Sept, 23:44 <button> Read me </button> </pre> </li>\n\t\t\t\t<li><pre> <b>%s</b>   %s   %s  <button> Read me> </button> </pre> </li>\n\t\t\t\t<li>grandpa</li>\n\t\t\t</ul>\n\t\t\t</li>\n\t\t</ol>\n\t\n\n</body>\n\n</html>\r\n", user, subject, date);
      strcpy(html_response, send_msg);
      retrieveMailHeader(user, rcpt_user, server_fd);
      //send(server_fd, send_msg, strlen(send_msg), 0);
      /*numlist = retrieveIDList(user, email_id);

      for(i in numlist) {
        //Packs this data in struct and sends it to UI service
        pack_struct(header);
      }

      return email_data;
*/  }break;
    case READ_MAIL: {

#ifdef DEBUG
      printf("READ_MAIL in core\n");
#endif
      sprintf(send_msg, "<!doctype html>\n<html>\n\n<head>\n\n\t<title>\n\t\tEMAIL\n\t\t\t</title>\n\n</head>\n\n<body>\n\n\t<h1>\n\t\t\tInbox:\n\t\t\t</h1>\n\t\t\t\t<p>TEXT OF EMAIL</p>\n\t\n\n</body>\n\n</html>\r\n");
      strcpy(html_response, send_msg);
      /*
      valid = validateMailId(user, email_id);
      //Query backend with specific ID
      if(valid == 0) {
        downloadEmail(user, email_id, mail_msg);
      } else {
        printf("Failed to read email!!\n");
      }
      //Pack the output into a string and return. 
    */
       downloadEmail(user, rcpt_user, email_id, mail_msg, server_fd);
       return 0;
      
     }break;
    case SEND_MAIL: {
  
#ifdef DEBUG
      printf("SEND_MAIL in core\n");
      printf("User: %s\n Recipient: %s\n", user, rcpt_user);
      printf("put <%s> <%s>", rcpt_user, mail_msg);
#endif
      /*valid = validateUser(rcpt_user);
      if(valid < 0) {
        printf("No such user exists!!\n");
        break;
      }
*/
      valid = send_email(user, rcpt_user, mail_msg, server_fd);
      //send(send_msg);

      } break;
    case DELETE_MAIL: {
   
#ifdef DEBUG
      printf("DELETE_MAIL in core\n");
      printf("User: %s\n email_id: %d\n", user, email_id);
#endif
        //TODO: Request to backend to delete
 /*        valid = validateMailId(user, email_id);
          if(valid == 0) {
            sprintf(send_msg, "delete <username> <mailId>");
            //send();
          } else {
            printf("No such mail ID"); 
          }
          //TODO: Return success or failure
          */
       deleteEmail(user, rcpt_user, email_id, mail_msg, server_fd);
          
     } break;
    default: {
            printf("Wrong option\n");
            return FAILURE;
    }break;
  }//switch between email options

}


int send_email(char * user, char *rcpt_user, char *mail_msg, int server_fd) {

  char recv_msg[1000];

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
#ifdef DEBUG
  printf ( "The current date/time is: %s", asctime (timeinfo) );
#endif

  email_header header;
  put_mail_request request;
 
  strcpy(header.from, user);
  strcpy(header.to, rcpt_user);
  strcpy(header.subject, "Subject1");
  strcpy(header.date, asctime(timeinfo));
  strcpy(request.prefix, "putmail");
  strcpy(request.username, user);
  memcpy(&(request.header), &header, sizeof(email_header));
  request.email_len = strlen(mail_msg);
  strcpy(request.email_body, mail_msg);
  
  send(server_fd, &request, sizeof(put_mail_request), 0);
  recv(server_fd, recv_msg, sizeof(recv_msg), 0);

#ifdef DEBUG
  printf("recv_msg: %s\n", recv_msg);
#endif
  return SUCCESS;
}

int validateUser(char *user) {

  char send_msg[1000];
  char recv_msg[1000];
  int failure;
  sprintf(send_msg, "get <%s>", user);
//  send(backend, send_msg, strlen(send_msg));

  //recv(backend, reply_msg, msg_len);

  //TODO: Check reply_msg

  if (failure) 
    return FAILURE;
  else
    return SUCCESS;

}

int validateMailId(char *user, uint16_t mailId) {

  char send_msg[1000];
  char recv_msg[1000];
  sprintf(send_msg, "validatemail <%s> <%d>", user, mailId);
  char validateMsg[10];
 // send(send_msg);
 // recv(validatedMsg);

  if(validateMsg != "SUCCESS")
    return FAILURE;
  else  
    return SUCCESS;

}

int deleteEmail(char *user, char *rcpt_to, uint16_t mailId, char *msg, int server_fd) {

  delete_mail_request del_req;
  char recv_msg[1000];

  strcpy(del_req.prefix, "delmail");
  strcpy(del_req.username, user);
  del_req.email_id =  (unsigned long) mailId;
 // send(send_msg);
  send(server_fd, &del_req, sizeof(delete_mail_request), 0);

  recv(server_fd, recv_msg, sizeof(recv_msg), 0);

#ifdef DEBUG
  printf("username: %s\n", (char *)mail_resp.username);
  printf("email_id: %s\n", (char *)mail_resp.email_id);
#endif
  printf("%s\n", (char *)recv_msg);
  
}

int downloadEmail(char *user, char *rcpt_to, uint16_t mailId, char *msg, int server_fd) {

  get_mail_body_request mail_req;
  get_mail_body_response mail_resp;
  memset(mail_resp.mail_body, 0, sizeof(mail_resp.mail_body));

  strcpy(mail_req.prefix, "mailbody");
  strcpy(mail_req.username, user);
  mail_req.email_id =  (unsigned long) mailId;
 // send(send_msg);
  send(server_fd, &mail_req, sizeof(get_mail_body_request), 0);

  recv(server_fd, (get_mail_body_response *)&mail_resp, sizeof(get_mail_body_response), 0);

#ifdef DEBUG
  printf("username: %s\n", (char *)mail_resp.username);
  printf("email_id: %d\n", mail_resp.email_id);
#endif
  printf("email_len: %d\n", mail_resp.mail_body_len);
  printf("email_body: %s\n", mail_resp.mail_body);
  
}

int retrieveMailHeader(char *user, char *rcpt_user, int server_fd) {

  get_mail_request request;
  get_mail_response resp;

  strcpy(request.prefix, "getmail");
  strcpy(request.username, user);

  send(server_fd, &request, sizeof(get_mail_request), 0);
  
  recv(server_fd, (get_mail_response *)&resp, sizeof(get_mail_response), 0);
  printf("\t\t\t\t\tINBOX\n\n\n");
#ifdef DEBUG
  printf("username: %s\n", (char *)resp.username);
  printf("num_emails: %d\n", (int)resp.num_emails);
#endif
  for(int i=0 ; i < resp.num_emails ; i++) {
    
  printf("From: %s To: %s Subject: %s, date: %s\n", resp.email_headers[i].from,  resp.email_headers[i].to,  resp.email_headers[i].subject,  resp.email_headers[i].date);

  }
}
