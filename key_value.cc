#include "key_value.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include "socket.h"

#include "data_types.h"

typedef enum 
{
   DRIVE,
   MAIL
}request_type;

typedef union
{
   mail_request mail_r;
   drive_request drive_r;
}request_content;

typedef struct
{
    request_type r_type;
    request_content content;
}bigtable_column;

typedef struct
{
    std::string password;
    std::unordered_map<std::string, bigtable_column> row;

}bigtable_row;

/** Map to store key value pairs */
std::unordered_map<std::string, bigtable_row> bigtable; 

bool process_command(char* buffer, int len, int fd)
{
    char* command = strtok(buffer, " \r");

    /** echo command */
    if (strcmp(command, "echo") == 0 || strcmp(command, "ECHO") == 0)
    {
        strncpy(buffer + 1, "+OK ", 4);
        send_msg_to_socket(buffer + 1, len - 1, fd);
    }
    /** quit command */
    else if (strcmp(command, "quit") == 0 || strcmp(command, "QUIT") == 0)
    {
        const char* response = "+OK Goodbye!\r\n";
        send_msg_to_socket(response, strlen(response), fd);

        return true;
    }
    /** unknown command */
    else
    {
        const char* response = "-ERR Unknown command\r\n";
        send_msg_to_socket(response, strlen(response), fd);
    }

    return false;
}


