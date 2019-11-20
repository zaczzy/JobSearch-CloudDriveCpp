#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CELLSIZE 1000

enum mail_opt {
    
    SHOW_MAIL =0,
    READ_MAIL,
    SEND_MAIL,
    DELETE_MAIL,

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
    return -1;
  }
  int validate = validateUser(user);
  if(validate < 0) {
    //TODO: Return error message
  }

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

      //Query backend with specific ID
      downloadMail(user, id);
      //Pack the output into a string and return. 
      return email;


    case SEND_MAIL:



    case DELETE_MAIL:
   
        //TODO: Request to backend to delete
          //TODO: Return success or failure

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


