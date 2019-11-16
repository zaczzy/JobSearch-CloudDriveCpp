#include "key_value.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include "socket.h"

/** Map to store key value pairs */
std::unordered_map<std::string, void*> bigtable_row;
std::unordered_map<std::string, 

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


