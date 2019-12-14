#ifndef KEY_VALUE_H
#define KEY_VALUE_H

#include "data_types.h"

#include <stdbool.h> 

int add_user(char* username, char* password);
int delete_user(char* username, char* password);
int store_email(put_mail_request* request);
bool change_password(char* username, char* old_password, char* new_password);
bool process_command(char* buffer, int len, int fd);
int delete_mail(delete_mail_request* request);
int delete_file(delete_file_request* request);
bool store_file(put_file_request* request);
bool change_password(char* username, char* old_password, char* new_password);
int create_folder(create_folder_request* request);
int delete_folder(delete_folder_content_request* request);

#endif
