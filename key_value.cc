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

/** TODO: For later use */
//typedef struct
//{
//    request_type r_type;
//    request_content content;
//}bigtable_column;

typedef struct
{
    char* value;
}bigtable_column;

typedef std::unordered_map<std::string, bigtable_column> map_bigtable_row;
typedef struct
{
    std::string password;
    map_bigtable_row columns;
}bigtable_row;

typedef std::unordered_map<std::string, bigtable_row> map_bigtable;

extern bool verbose;

/** Map to store key value pairs */
map_bigtable bigtable; 

bool put(char* row, char* column, char* data)
{
    map_bigtable::iterator itr;
    
    /** Check if the row exists */
    if ((itr = bigtable.find(std::string(row))) == bigtable.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    /** If this column doesn't already exist */
    map_bigtable_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        /** Add the new column to this row */
        // TODO: Allocate dynamic memory to data when put() function is called
        bigtable_column col;
        col.value = data;
        itr->second.columns.insert(std::make_pair(std::string(column), col));

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }
    /** Column already exists */
    else
    {
       row_itr->second.value = data; 
    }

    return SUCCESS;
}

char* get(char* row, char* column)
{
    map_bigtable::iterator itr;
    
    /** Check if the row exists */
    if ((itr = bigtable.find(std::string(row))) == bigtable.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return NULL;
    }

    /** Check if the column exists */
    map_bigtable_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("no column %s exists in row %s\n", column, row);

        return NULL;
    }

    /** Get and return the value in column */
    return row_itr->second.value; 
}

bool cput(char* row, char* column, char* old_data, char* data)
{
    map_bigtable::iterator itr;
    
    /** Check if the row exists */
    if ((itr = bigtable.find(std::string(row))) == bigtable.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    /** Check if this column exists */
    map_bigtable_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("Column %s in row %s doesn't exist\n", column, row);

        return FAILURE;
    }
    
    /** Column already exists */
    /** Check if its current value matches the given old_value */
    if (strcmp(row_itr->second.value, old_data) != 0)
    {
        if (verbose)
            printf("column value doesnt match the given value\n");

        return FAILURE;
    }
    
    /** Update the value in this column */
    row_itr->second.value = data;

    return SUCCESS;
}

bool delete_entry(char* row, char* column)
{
    map_bigtable::iterator itr;
    
    /** Check if the row exists */
    if ((itr = bigtable.find(std::string(row))) == bigtable.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    /** Check if this column exists */
    map_bigtable_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("Column %s in row %s doesn't exist\n", column, row);

        return FAILURE;
    }

    /** Delete the column */
    itr->second.columns.erase(row_itr);

    return SUCCESS;
}

bool add_user(char* username, char* password)
{
    map_bigtable::iterator itr;
    
    /** Check if this username already exists */
    if ((itr = bigtable.find(std::string(username))) != bigtable.end())
    {
        if (verbose)
            printf("user %s already exists\n", username);

        return FAILURE;
    }

    /** Add a new user with this username and password */
    bigtable_row row;
    row.password = std::string(password);

    bigtable.insert(std::make_pair(username, row));

    return SUCCESS;
}

bool delete_user(char* username, char* password)
{
    map_bigtable::iterator itr;
    
    /** Check if this username exists */
    if ((itr = bigtable.find(std::string(username))) == bigtable.end())
    {
        if (verbose)
            printf("user %s doesn't exist\n", username);

        return FAILURE;
    }

    /** Delete the user with this username */
    bigtable.erase(itr);

    return SUCCESS;
}

bool process_command(char* buffer, int len, int fd)
{
#if 0
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
#endif
    return false;
}


