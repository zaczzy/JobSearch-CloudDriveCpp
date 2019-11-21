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
}column_type;

class hash_fn{
public:

    std::string operator()(const email_header& header) const
    {
        return std::string(header.date) + std::string(header.subject);
    }
};

typedef struct
{
    uint64_t num_emails;
    std::vector<email_header> header_list;
    std::vector<char*> body_list;
}mail_content;

// TODO: For later use
//typedef struct
//{
//    uint64_t num_emails;
//    std::unordered_map<email_header, char*, hash_fn> email_list;
//}mail_content;

typedef struct
{
    uint64_t file_len;
    char* file_data;
}file_content;

typedef struct
{
    column_type type;
    void* content;
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

bool store_email(put_mail_request* request)
{
    char* row = request->username;
    char* column = request->email_id;

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
    }

    /** Check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        /** Add the new email */
        mail_content* content = (mail_content*)malloc(sizeof(mail_content) * sizeof(char));
        // TODO: Check NULL
        content->num_emails = 1;

        char* mail_body = (char*)malloc(request->email_len * sizeof(char));
        // TODO: Check NULL
        strncpy(mail_body, request->email_body, request->email_len);

        content->header_list.push_back(request->header);
        content->body_list.push_back(mail_body);
       
        tablet_column* col = &(row_itr->second);
        col->content = content;
    }
    else /** Create a column with the given email ID */
    {
        /** Add the new column to this row */
        tablet_column col;
        col.type = MAIL;

        mail_content* content = (mail_content*)malloc(sizeof(mail_content) * sizeof(char));
        // TODO: Check NULL
        content->num_emails = 1;

        char* mail_body = (char*)malloc(request->email_len * sizeof(char));
        // TODO: Check NULL
        strncpy(mail_body, request->email_body, request->email_len);

        content->header_list.push_back(request->header);
        content->body_list.push_back(mail_body);
       
        col.content = content; 
        /** Add the entry to the map */
        itr->second.columns.insert(std::make_pair(std::string(column), col));

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }

    return SUCCESS;
}

bool get_email_list(get_mail_request* request, get_mail_response* response)
{
    char* row = request->username;
    char* column = request->email_id;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
    }

    /** check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        tablet_column* col = &(row_itr->second);
        mail_content* content = (mail_content*)col;
        
        response->num_emails = content->header_list.size();
        strncpy(response->prefix, request->prefix, strlen(request->prefix));
        strncpy(response->username, request->username, strlen(request->username));
        strncpy(response->email_id, request->email_id, strlen(request->email_id));
        for (unsigned int i = 0; i < content->header_list.size(); i++)
        {
            response->email_headers[i] = content->header_list[i];
        }
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return FAILURE;
    }
    
   return SUCCESS; 
}

bool get_file_data(get_file_request* request, get_file_response* response)
{
    char* row = request->username;
    char* column = request->filename;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
    }

    /** check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        tablet_column* col = &(row_itr->second);
        file_content* content = (file_content*)col;
        
        response->file_len = content->file_len;
        strncpy(response->prefix, request->prefix, strlen(request->prefix));
        strncpy(response->username, request->username, strlen(request->username));
        strncpy(response->filename, request->filename, strlen(request->filename));

        strncpy(response->file_content, content->file_data, content->file_len);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return FAILURE;
    }
    
   return SUCCESS; 
}

bool store_file(put_file_request* request)
{
    char* row = request->username;
    char* column = request->filename;

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
    }

    /** Check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        // TODO: Allow to change existing file later
        if (verbose)
            printf("A column with this filename already exists\n");
        return FAILURE;
    }
    else /** Create a column with the given email ID */
    {
        /** Add the new column to this row */
        tablet_column col;
        col.type = MAIL;

        /** Add the new file */
        file_content* content = (file_content*)malloc(sizeof(file_content) * sizeof(char));
        // TODO: Check NULL

        char* data = (char*)malloc(request->file_len * sizeof(char));
        // TODO: Check NULL
        strncpy(data, request->file_content, request->file_len);

        content->file_len = request->file_len;
        content->file_data = data;

        col.content = content; 
        /** Add the entry to the map */
        itr->second.columns.insert(std::make_pair(std::string(column), col));

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }

    return SUCCESS;
}

bool process_command(char* buffer, int len, int fd)
{
    char message[64];

    printf("command: %s len: %d\n", buffer, len);

    char* command = strtok(buffer, " ");

    if (command == NULL)
    {
        if (verbose)
            printf("command is NULL\n");
        return SUCCESS;
    }

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
    /** put email command */
    else if (strncmp(command, "putmail", 7) == 0 || strncmp(command, "PUTMAIL", 7) == 0)
    {
        put_mail_request* mail_request = (put_mail_request*)command;

        /** Store the new mail */
        bool res = store_email(mail_request);

        if (res == SUCCESS)
            strncpy(message, "+OK email stored", strlen("+OK email stored"));
        else
            strncpy(message, "-ERR can't store email", strlen("-ERR can't store email"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get email list command */
    else if (strncmp(command, "getmail", 7) == 0 || strncmp(command, "GETMAIL", 7) == 0)
    {
        get_mail_request* mail_request = (get_mail_request*)command;
        get_mail_response response;

        /** Get mails */
        bool res = get_email_list(mail_request, &response);

        if (res == SUCCESS)
        {
            send_msg_to_socket((char*)(&response), sizeof(response), fd);
        }
        else
        {
            strncpy(message, "-ERR can't get email", strlen("-ERR can't get email"));
            message[strlen(message)] = '\0';
            send_msg_to_socket(message, strlen(message), fd);
        }

        return SUCCESS;
    }
    /** put file command */
    else if (strncmp(command, "putfile", 7) == 0 || strncmp(command, "PUTFILE", 7) == 0)
    {
        put_file_request* file_request = (put_file_request*)command;

        /** Store the new file */
        bool res = store_file(file_request);
        
        if (res == SUCCESS)
            strncpy(message, "+OK file stored", strlen("+OK file stored"));
        else
            strncpy(message, "-ERR can't store file", strlen("-ERR can't store file"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get file command */
    else if (strncmp(command, "getfile", 7) == 0 || strncmp(command, "GETFILE", 7) == 0)
    {
        get_file_request* file_request = (get_file_request*)command;
        get_file_response response;

        /** Get file data */
        bool res = get_file_data(file_request, &response);

        if (res == SUCCESS)
        {
            send_msg_to_socket((char*)(&response), sizeof(response), fd);
        }
        else
        {
            strncpy(message, "-ERR can't get file", strlen("-ERR can't get file"));
            message[strlen(message)] = '\0';
            send_msg_to_socket(message, strlen(message), fd);
        }

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

