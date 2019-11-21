#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "data_types.h"

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

int validateUser(char *user);
int validateMailId(char *user, uint16_t mailId);
int downloadEmail(char *user, uint16_t mailId, char *msg);

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

  mOpt = mailOpt;

  switch(mOpt) {
    case SHOW_MAIL:                 //Downloads email headers from backend
      printf("SHOW_MAIL in core\n");
      printf("User: %s\n\n", user);
      char subject[20] = "My struggles";
      char date[20] = "Sept 1st, 3:14";
      sprintf(send_msg, "<!doctype html>\n<html>\n\n<head>\n\n\t<title>\n\t\tUser inbox\n\t\t\t</title>\n\n</head>\n\n<body>\n\n\t<ol>\n\t\t<li>\n\t\t\tInbox:\n\t\t\t<ul>\n\t\t\t\t<li><pre> <b>Charles Aranguiz</b>     My travels     Monday 14th Sept, 23:44 <button> Read me </button> </pre> </li>\n\t\t\t\t<li><pre> <b>%s</b>   %s   %s  <button> Read me> </button> </pre> </li>\n\t\t\t\t<li>grandpa</li>\n\t\t\t</ul>\n\t\t\t</li>\n\t\t</ol>\n\t\n\n</body>\n\n</html>\r\n", user, subject, date);
      strcpy(html_response, send_msg);
      //send(server_fd, send_msg, strlen(send_msg), 0);
      return 0;
      /*numlist = retrieveIDList(user, email_id);

      for(i in numlist) {
        retrieveMailHeader(id_i);
        //Packs this data in struct and sends it to UI service
        pack_struct(header);
      }

      return email_data;
*/
    case READ_MAIL:

      printf("READ_MAIL in core\n");
      sprintf(send_msg, "<!doctype html>\n<html>\n\n<head>\n\n\t<title>\n\t\tEMAIL\n\t\t\t</title>\n\n</head>\n\n<body>\n\n\t<h1>\n\t\t\tInbox:\n\t\t\t</h1>\n\t\t\t\t<p>TEXT OF EMAIL</p>\n\t\n\n</body>\n\n</html>\r\n");
      strcpy(html_response, send_msg);
      return 0;
      valid = validateMailId(user, email_id);
      //Query backend with specific ID
      if(valid == 0) {
        downloadEmail(user, email_id, mail_msg);
      } else {
        printf("Failed to read email!!\n");
      }
      //Pack the output into a string and return. 
    
      break;
    case SEND_MAIL:
  
      printf("SEND_MAIL in core\n");
      printf("User: %s\n Recipient: %s\n", user, rcpt_user);
      return 0 ;
      valid = validateUser(rcpt_user);
      if(valid < 0) {
        printf("No such user exists!!\n");
        break;
      }

      sprintf(send_msg, "put <%s> <%s> <%s>", rcpt_user, mail_msg);
      //send(send_msg);

      break;
    case DELETE_MAIL:
   
      printf("DELETE_MAIL in core\n");
      printf("User: %s\n email_id: %s\n");
      return 0;
        //TODO: Request to backend to delete
         valid = validateMailId(user, email_id);
          if(valid == 0) {
            sprintf(send_msg, "delete <username> <mailId>");
            //send();
          } else {
            printf("No such mail ID"); 
          }
          //TODO: Return success or failure
      break;
    default: 
            printf("Wrong option\n");
            return FAILURE;
  
  }//switch between email options

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

int downloadEmail(char *user, uint16_t mailId, char *msg) {

  char send_msg[1000];
  char recv_msg[1000];
  sprintf(send_msg, "readmail <%s> <%d>", user, mailId);
 // send(send_msg);

  //recv(recv_msg);

  printf("EMAIL\n %s", recv_msg);
  
}
