#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CELLSIZE 1000

enum mail_opt {
    
    SHOW_MAIL =0,
    READ_MAIL

}mOpt;


struct email_data {

 uint16_t email_ID; 
 char Date[50];
 char Sender[30];
 char Message[CELLSIZE];

};

int webserver_core(int mailOpt, char *user) {

//Invokes backend to enquire about user
  if(user == NULL) {
    printf("Invalid user\n");
    return -1;
  }

  int validate = validateUser(user);
  if(validate < 0) {
    printf("Invalid user %s\n", user);
    return -1;
  }

  mOpt = mailOpt;

  switch(mOpt) {
    case SHOW_MAIL:                 //Downloads email headers from backend
      numlist = retrieveIDList();

      for(i in numlist) {
        retrieveMailHeader(id_i);
        //Packs this data in struct and sends it to UI service
        pack_struct(header);
      }

      return email_data;

    case READ_MAIL:

      valid = validateMailId(user, id);
      //Query backend with specific ID
      if(valid == 0) {
        downloadMail(user, id, email);
        return email;
      } else {
        printf("Failed to read email!!\n");
      }
      //Pack the output into a string and return. 
    
      break;
    case SEND_MAIL:
  
      valid = validaterUser(rcpt_user);
      if(valid < 0) {
        printf("No such user exists!!\n");
        break;
      }

      sprintf(send_msg, "put <%s> <%s> <%s>", rcpt_user, mail_msg);
      send(send_msg);

      break;
    case DELETE_MAIL:
   
        //TODO: Request to backend to delete
         valid = validateMailId();
          if(valid == 0) {
            sprintf(send_msg, "delete <username> <mailId>");
            send();
          } else {
            printf("No such mail ID"); 
          }
          //TODO: Return success or failure
      break;
    default: 
      return wrong_option;
  
  }//switch between email options

}


int validateUser(char *user) {

  sprintf(send_msg, "get <%s>");
  send(backend, send_msg, strlen(send_msg));

  recv(backend, reply_msg, msg_len);

  //TODO: Check reply_msg

  if (failure) 
    return -1;
  else
    return 0;

}

int validateMailId(char *user, uint16_t mailId) {

  sprintf(send_msg, "validatemail <%s> <%d>", user, mailId);
  send(send_msg);
  recv(validatedMsg);

  if(validateMsg != "SUCCESS")
    return -1;
  else  
    return 0;

}

int downloadEmail(user, mailId, msg) {

  sprintf(send_msg, "readmail <%s> <%d>", user, mailId);
  send(send_msg);

  recv(recv_msg);

  printf("EMAIL\n %s", recv_msg);
  
}
