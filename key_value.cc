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
//}tablet_column;

typedef struct
{
    char* value;
}tablet_column;

typedef std::unordered_map<std::string, tablet_column> map_tablet_row;
typedef struct
{
    std::string email_id;
    std::string password;
    map_tablet_row columns;
}tablet_row;

typedef std::unordered_map<std::string, tablet_row> map_tablet;

extern bool verbose;

/** Map to store key value pairs */
map_tablet tablet; 

bool put(char* row, char* column, char* data)
{
    // TODO: Remove
    printf("row: %s column: %s column len: %d\n", row, column, strlen(column));

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    /** If this column doesn't already exist */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("col doesn't already exist\n");

        /** Add the new column to this row */
        // TODO: Allocate dynamic memory to data when put() function is called
        tablet_column col;
        col.value = data;
        itr->second.columns.insert(std::make_pair(std::string(column), col));

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }
    /** Column already exists */
    else
    {
        if (verbose)
            printf("col already exists, just update valu, old value : %s\n", row_itr->second.value);

       row_itr->second.value = data; 
    }

    return SUCCESS;
}

int get(char* row, char* column, char** data)
{
    printf("row: %s column: %s column len: %d\n", row, column, strlen(column));

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    printf("inter: row: %s column: %s\n", row, column);
    
    /** If this column doesn't already exist */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("given column doesn't exist in row %s\n", row);

        return -1;
    }
    
    /** Get and return the value in column */
    // TODO : Remove
    *data = row_itr->second.value;
    return strlen(row_itr->second.value); 
#if 0
    // TODO: Remove
    printf("row: %s column: %s\n", row, column);

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return -1;
    }

    /** Check if the column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("given column doesn't exist in row %s\n", row);

        return -1;
    }

    /** Get and return the value in column */
    data = row_itr->second.value;
    return strlen(row_itr->second.value); 
#endif
}

bool cput(char* row, char* column, char* old_data, char* data)
{
    // TODO: Remove
    printf("row: %s column: %s\n", row, column);

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    /** Check if this column exists */
    map_tablet_row:: iterator row_itr; 
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
    // TODO: Remove
    printf("row: %s column: %s\n", row, column);

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row %s exists\n", row);

        return FAILURE;
    }

    /** Check if this column exists */
    map_tablet_row:: iterator row_itr; 
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
    map_tablet::iterator itr;
    
    /** Check if this username already exists */
    if ((itr = tablet.find(std::string(username))) != tablet.end())
    {
        if (verbose)
            printf("user %s already exists\n", username);

        return FAILURE;
    }

    /** Add a new user with this username and password */
    tablet_row row;
    row.password = std::string(password);

    tablet.insert(std::make_pair(username, row));

    return SUCCESS;
}

bool delete_user(char* username, char* password)
{
    map_tablet::iterator itr;
    
    /** Check if this username exists */
    if ((itr = tablet.find(std::string(username))) == tablet.end())
    {
        if (verbose)
            printf("user %s doesn't exist\n", username);

        return FAILURE;
    }

    /** Delete the user with this username */
    tablet.erase(itr);

    return SUCCESS;
}

bool process_command(char* buffer, int len, int fd)
{
    char message[64];

    // TODO: remove
    printf("command: %send len: %d\n", buffer, len);

    char* command = strtok(buffer, " ");

    if (command == NULL)
        return SUCCESS;

    /** add user command */
    if (strcmp(command, "add") == 0 || strcmp(command, "ADD") == 0)
    {
        char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");

        /** Add the user */
        bool res = add_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user added", strlen("+OK user added"));
        else
            strncpy(message, "-ERR can't add user", strlen("-ERR can't add user"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message) + 1, fd);

        return SUCCESS;
    }
    /** delete user command */
    else if (strcmp(command, "delete") == 0 || strcmp(command, "DELETE") == 0)
    {
        char* username = strtok(NULL, " ");
        char* password = strtok(NULL, " ");

        /** Delete the user */
        bool res = delete_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user deleted", strlen("+OK user deleted"));
        else
            strncpy(message, "-ERR user not deleted", strlen("-ERR user not deleted"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;
    }
    /** put command */
    else if (strcmp(command, "put") == 0 || strcmp(command, "PUT") == 0)
    {
        char* row = strtok(NULL, " ");
        char* col = strtok(NULL, " ");
        char* value = strtok(NULL, " ");

        // TODO: value NULL check

        char* data = (char*)malloc(strlen(value));

        // TODO: Malloc error check

        strncpy(data, value, strlen(value));
        
        /** Put the new entry */
        bool res = put(row, col, data);

        if (res == SUCCESS)
            strncpy(message, "+OK value put", strlen("+OK value put"));
        else
            strncpy(message, "-ERR can't put value", strlen("-ERR can't put value"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get command */
    else if (strcmp(command, "get") == 0 || strcmp(command, "GET") == 0)
    {
        char* row = strtok(NULL, " ");
        char* col = strtok(NULL, " ");
        char* value = NULL;

        /** Get the entry */
        int len = get(row, col, &value);

        printf("data: %s len: %d\n", value, strlen(value));
        
        // TODO: CHeck for NULL value

        message[strlen(message)] = '\0';

        send_msg_to_socket(value, len, fd);
            
        return SUCCESS;
    }
    /** cput command */
    else if (strcmp(command, "cput") == 0 || strcmp(command, "CPUT") == 0)
    {
        char* row = strtok(NULL, " ");
        char* col = strtok(NULL, " ");
        char* old_value = strtok(NULL, " ");
        char* value = strtok(NULL, " ");

        // TODO: value, old_value NULL check

        char* data = (char*)malloc(strlen(value));

        // TODO: Malloc error check

        strncpy(data, value, strlen(value));
        
        /** Put the new entry */
        bool res = cput(row, col, old_value, data);

        if (res == SUCCESS)
            strncpy(message, "+OK value put", strlen("+OK value put"));
        else
            strncpy(message, "-ERR can't put value", strlen("-ERR can't put value"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** delete command */
    else if (strcmp(command, "del") == 0 || strcmp(command, "DEL") == 0)
    {
        char* row = strtok(NULL, " ");
        char* col = strtok(NULL, " ");

        /** Delete this column */
        bool res = delete_entry(row, col);

        if (res == SUCCESS)
            strncpy(message, "+OK value deleted", strlen("+OK value deleted"));
        else
            strncpy(message, "-ERR can't delete value", strlen("-ERR can't delete value"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    else
    {
        strncpy(message, "-ERR unknown command", strlen("-ERR unknown command"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }

   return FAILURE;
}


