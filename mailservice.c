#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

int webserver_core(int mailOpt, char *user, int email_id, char *mail_msg, char *rcpt_user) {
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
