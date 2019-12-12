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
#include "error.h"

#define MAX_CHUNK_SIZE      10240   // 10 KB TODO: Check with Zach
#define MAX_TABLET_USERS    100     // TODO: Check with team 

typedef enum 
{
   DRIVE,
   MAIL
}column_type;

typedef struct
{
    unsigned long email_id;
    email_header* header;
    char* email_body;
    unsigned long body_len;
    
#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & email_id;
       ar & header;
       ar & body_len;
       SerializeCStringHelper helper(email_body);
       ar & helper; 
   }
#endif
}mail_content;

typedef struct 
{
    bool type;
    char name[1024];
}fd_entry;

typedef struct
{
    bool type;  // 0 - Directory, 1 - File
    
    /** If it is a file */
    uint64_t file_len;
    char* file_data;

    /** If it is a directory */
    uint64_t num_files;
    std::vector<fd_entry> entry;
    
#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & file_len;
       SerializeCStringHelper helper(file_data);
       ar & helper; 
       ar & num_files;
       // TODO: Serialize vector
   }
#endif
}file_content;

#ifdef SERIALIZE
class VoidPtrHelper {
public:
  VoidPtrHelper(column_type& _type, void*& _content) : type(_type), content(_content) {}

private:

  friend class boost::serialization::access;

  template<class Archive>
  void save(Archive& ar, const unsigned version) const {
    
      ar & type;
      if (type == MAIL)
      {
        file_content* f_content = (file_content*)content;
        ar & f_content;
      }
      else if (type == DRIVE)
      {
        mail_content* m_content = (mail_content*)content;
        ar & m_content;
      }
  }

  template<class Archive>
  void load(Archive& ar, const unsigned version) {
      column_type type;
      ar & type;
      if (type == MAIL)
      {
        file_content* f_content;
        ar & f_content;
        content = f_content;
      }
      else if (type == DRIVE)
      {
        mail_content* m_content;
        ar & m_content;
        content = m_content;
      }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

private:
 column_type& type;
 void*& content;
};
#endif

typedef struct
{
    column_type type;
    void* content;

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & type;
       VoidPtrHelper vp_helper(type, content);
       ar & vp_helper;
   }
#endif
}tablet_column;

typedef std::unordered_map<std::string, tablet_column> map_tablet_row;
typedef struct
{
    std::string email_id;
    std::string password;
    unsigned long num_emails;
    unsigned long num_files;
    map_tablet_row columns;

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & email_id;
       ar & password;
       ar & num_emails;
       ar & num_files;
       ar & columns;
   }
#endif
}tablet_row;

typedef std::unordered_map<std::string, tablet_row> map_tablet;

extern bool verbose;

/** Map to store key value pairs */
map_tablet tablet; 

int add_user(char* username, char* password)
{
    if (tablet.size() == MAX_TABLET_USERS)
        return ERR_EXCEEDED_MAX_TABLET_USER_LIMIT;

    map_tablet::iterator itr;
    
    /** Check if this username already exists */
    if ((itr = tablet.find(std::string(username))) != tablet.end())
    {
        if (verbose)
            printf("user %s already exists\n", username);

        return ERR_USER_ALREADY_EXISTS;
    }

    /** Add a new user with this username and password */
    tablet_row row;
    row.password = std::string(password);
    row.num_emails = 0;
    row.num_files = 0;

    if (verbose)
        printf("Added user %s\n", username);
    tablet.insert(std::make_pair(username, row));

    return SUCCESS;
}

int delete_user(char* username, char* password)
{
    map_tablet::iterator itr;
    
    /** Check if this username exists */
    if ((itr = tablet.find(std::string(username))) == tablet.end())
    {
        if (verbose)
            printf("user %s doesn't exist\n", username);

        return ERR_USER_DOESNT_EXIST;
    }

    /** Delete the user with this username */
    tablet.erase(itr);

    return SUCCESS;
}

bool auth_user(char* username, char* password)
{
    map_tablet::iterator itr;
    
    /** Check if this username exists */
    if ((itr = tablet.find(std::string(username))) == tablet.end())
    {
        if (verbose)
            printf("user %s doesn't exist\n", username);

        return ERR_USER_DOESNT_EXIST;
    }

    /** verify password */
    if (strncmp(itr->second.password.c_str(), password, strlen(itr->second.password.c_str())) == 0)
    {
        if (verbose)
            printf("user authenticated\n");

        return SUCCESS;
    }

    return ERR_WRONG_PASSWORD;
}

#if 1
int store_email(put_mail_request* request)
{
    char* row = request->username;

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** Generate the ID for this email */
    unsigned long email_id = ++(itr->second.num_emails);
    printf("num emails: %lu\n", itr->second.num_emails);
    char email_id_str[21];  /** Max. no of digits in an unsigned long number is 20 */
    sprintf(email_id_str, "%lu", email_id);

    /** Add a column for this email */
    tablet_column col;
    col.type = MAIL;

    mail_content* content = (mail_content*)malloc(sizeof(mail_content) * sizeof(char));
    // TODO: Check NULL

    content->email_body = (char*)malloc(request->email_len * sizeof(char));
    content->header = (email_header*)malloc(sizeof(email_header) * sizeof(char));
    // TODO: Check NULL
    strncpy(content->email_body, request->email_body, request->email_len);
    *(content->header) = request->header;
    content->email_id = email_id;

    col.content = content; 
    /** Add the entry to the map */
    itr->second.columns.insert(std::make_pair(std::string(email_id_str), col));

    if (verbose)
        printf("Added email id %lu to row %s\n", email_id, row);
    
    return SUCCESS;
}

#else
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
        tablet_column* col = &(row_itr->second);
        mail_content* content = (mail_content*)col->content;

        /** Add the new email */
        //mail_content* content = (mail_content*)malloc(sizeof(mail_content) * sizeof(char));
        // TODO: Check NULL
        //content->num_emails++;

        char* mail_body = (char*)malloc(request->email_len * sizeof(char));

        email_header* mail_header = (email_header*)malloc(sizeof(email_header) * sizeof(char));
        // TODO: Check NULL for  both header and body

        strncpy(mail_body, request->email_body, request->email_len);
        printf("storing mail body : %s\n", mail_body);
        *mail_header = request->header;

        content->header_list.push_back(mail_header);
        content->body_list.push_back(mail_body);
        content->num_emails++;
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
        email_header* mail_header = (email_header*)malloc(sizeof(email_header) * sizeof(char));
        // TODO: Check NULL
        strncpy(mail_body, request->email_body, request->email_len);
        *mail_header = request->header;

        content->header_list.push_back(mail_header);
        content->body_list.push_back(mail_body);
       
        col.content = content; 
        /** Add the entry to the map */
        itr->second.columns.insert(std::make_pair(std::string(column), col));

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }
    return SUCCESS;
}
#endif

#if 1
int get_email_list(get_mail_request* request, get_mail_response* response)
{
    char* row = request->username;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }
    unsigned long num_emails = itr->second.num_emails;

    if (num_emails == 0)
    {
        response->num_emails = 0;
        strncpy(response->prefix, request->prefix, strlen(request->prefix));
        strncpy(response->username, request->username, strlen(request->username));

        return SUCCESS;
    }

    /** Traverse through all the columns and return all the email headers found */
    map_tablet_row::iterator col_itr = itr->second.columns.begin();

    int email_ctr = 0;
    while(col_itr != itr->second.columns.end())
    {
        /** Check if this column contains an email or a file */
        if (col_itr->second.type == MAIL)
        {
            mail_content* content = (mail_content*)col_itr->second.content; 
            response->email_headers[email_ctr] = *content->header;
            email_ctr++;
        }
        col_itr++;
    }

    response->num_emails = num_emails;
    strncpy(response->prefix, request->prefix, strlen(request->prefix));
    strncpy(response->username, request->username, strlen(request->username));
    
   return SUCCESS; 
}

#else
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
        printf("column name: %s\n", row_itr->first.c_str());
        tablet_column* col = &(row_itr->second);
        mail_content* content = (mail_content*)col->content;
        
        printf("num emails: %lu header list size: %lu mail list size: %lu\n", content->num_emails, content->header_list.size(), content->body_list.size());
        response->num_emails = content->header_list.size();
        strncpy(response->prefix, request->prefix, strlen(request->prefix));
        strncpy(response->username, request->username, strlen(request->username));
        strncpy(response->email_id, request->email_id, strlen(request->email_id));
        for (unsigned int i = 0; i < content->header_list.size(); i++)
        {
            printf("mail content: %s\n", content->body_list[i]);
            response->email_headers[i] = *(content->header_list[i]);
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
#endif

#if 1
int delete_mail(delete_mail_request* request)
{
    char* row = request->username;
    unsigned long email_id = request->email_id;

    map_tablet::iterator itr;

    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** check if this email_id  exists */
    char email_id_str[21];  /** Max. no of digits in an unsigned long number is 20 */
    sprintf(email_id_str, "%lu", email_id);
    map_tablet_row:: iterator row_itr;
    if ((row_itr = itr->second.columns.find(std::string(email_id_str))) != itr->second.columns.end())
    {
        itr->second.num_emails--;
        itr->second.columns.erase(row_itr);
    }
    else /** email id doesn't exist */
    {
        if (verbose)
            printf("no email with id %lu exists\n", email_id);
        return ERR_INVALID_EMAILID;
    }

   return SUCCESS;
}
#else
bool delete_mail(delete_mail_request* request)
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
        if(request->index < 0)
            return FAILURE;

        tablet_column* col = &(row_itr->second);
        mail_content* content = (mail_content*)col->content;
        
        // TODO: Check for index value greater than num_emails before using it
        content->body_list.erase(content->body_list.begin() + request->index);
        content->header_list.erase(content->header_list.begin() + request->index);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return FAILURE;
    }
    
   return SUCCESS; 
}
#endif

#if 1
int get_mail_body(get_mail_body_request* request, get_mail_body_response* response)
{
    char* row = request->username;
    unsigned long email_id = request->email_id;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** check if this email id exists */
    char email_id_str[21];  /** Max. no of digits in an unsigned long number is 20 */
    sprintf(email_id_str, "%lu", email_id);

    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(email_id_str))) != itr->second.columns.end())
    {
        tablet_column* col = &(row_itr->second);
        mail_content* content = (mail_content*)col->content;
        
        strncpy(response->prefix, request->prefix, strlen(request->prefix));
        strncpy(response->username, request->username, strlen(request->username));
        response->email_id = request->email_id;

        // TODO: Check for index value greater than num_emails before using it
        strncpy(response->mail_body, content->email_body, strlen(content->email_body));
        response->mail_body_len = strlen(response->mail_body);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no email with id %lu  exists\n", email_id);
        return ERR_INVALID_EMAILID;
    }
    
   return SUCCESS; 
}

#else
bool get_mail_body(get_mail_body_request* request, get_mail_body_response* response)
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
        if(request->index < 0)
            return FAILURE;

        tablet_column* col = &(row_itr->second);
        mail_content* content = (mail_content*)col->content;
        
        strncpy(response->prefix, request->prefix, strlen(request->prefix));
        strncpy(response->username, request->username, strlen(request->username));
        strncpy(response->email_id, request->email_id, strlen(request->email_id));

        // TODO: Check for index value greater than num_emails before using it
        printf("getting email body for index: %lu\n", request->index);
        printf("sending email body: %s\n", content->body_list[request->index]);
        strncpy(response->mail_body, content->body_list[request->index], strlen(content->body_list[request->index]));
        response->mail_body_len = strlen(response->mail_body);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return FAILURE;
    }
    
   return SUCCESS; 
}
#endif

int delete_file(delete_file_request* request)
{
    char* row = request->username;
    /** Concatenate the path and name */
    char full_path[1024 + 256];
    sprintf(full_path, "%s/%s", request->directory_path, request->filename);

    char* column = full_path;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        itr->second.columns.erase(row_itr);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return ERR_FILE_DOESNT_EXIST;
    }
    
   return SUCCESS; 
    
}

#if 1
int get_file_data(get_file_request* request, int fd)
{
    char* row = request->username;
    /** Concatenate the path and name */
    char full_path[1024 + 256];
    sprintf(full_path, "%s/%s", request->directory_path, request->filename);

    char* column = full_path;


    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        tablet_column* col = &(row_itr->second);
        file_content* content = (file_content*)col->content;
        
        get_file_response response;
        uint64_t bytes_sent = 0;

        while (bytes_sent != content->file_len)
        {
            if ((content->file_len - bytes_sent) > CHUNK_SIZE)
            {
                response.has_more = true;
                strncpy(response.chunk, content->file_data, CHUNK_SIZE);
                bytes_sent += CHUNK_SIZE;
            }
            else
            {
                response.has_more = false;
                strncpy(response.chunk, content->file_data, content->file_len);
                bytes_sent += content->file_len;
            }
        
            send_msg_to_socket((char*)(&response), sizeof(response), fd);
        }
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return ERR_FILE_DOESNT_EXIST;
    }
    
   return SUCCESS; 
}
#else
int get_file_data(get_file_metadata* request, int fd)
{
    get_file_metadata response;
    char* row = request->username;
    char* column = request->filename;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        tablet_column* col = &(row_itr->second);
        file_content* content = (file_content*)col->content;
        
        response.file_len = content->file_len;
        strncpy(response.prefix, request->prefix, strlen(request->prefix));
        strncpy(response.username, request->username, strlen(request->username));
        strncpy(response.filename, request->filename, strlen(request->filename));

        /** Send the metadata first */
        send_msg_to_socket((char*)(&response), sizeof(response), fd);


        /** Send the file content now */
        // TODO: Send the file content in small chunks MAX_CHUNK_SIZE
        send_msg_to_socket(content->file_data, content->file_len, fd);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return ERR_FILE_DOESNT_EXIST;
    }
    
   return SUCCESS; 
}
#endif

#if 1
bool store_file(put_file_request* request, int fd)
{
    char* row = request->username;

    /** Concatenate the path and name */
    char full_path[1024 + 256];
    sprintf(full_path, "%s/%s", request->directory_path, request->filename);

    char* column = full_path;

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** Check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        /** Append to this file */
        file_content* content = (file_content*)row_itr->second.content;
        
        char* new_data = (char*)realloc(content->file_data, request->chunk_len + content->file_len);

        if (new_data == NULL)
        {
            if (verbose)
                printf("Malloc failed while reallocating memory for file data\n");
            return ERR_MALLOC_FAILED;
        }

        snprintf(new_data, content->file_len + request->chunk_len, "%s%s", content->file_data, request->data);
        content->file_data = new_data;
        content->file_len += request->chunk_len;
        
        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }
    else /** Create a column with the given email ID */
    {
        /** Add the new column to this row */
        tablet_column col;
        col.type = MAIL;

        /** Add the new file */
        file_content* content = (file_content*)malloc(sizeof(file_content) * sizeof(char));
        // TODO: Check NULL

        char* data = (char*)malloc(request->chunk_len * sizeof(char));
        // TODO: Check NULL
        
        /** Read the data */
        strncpy(data, request->data, request->chunk_len);

        content->file_len = request->chunk_len;
        content->file_data = data;

        col.content = content; 
        /** Add the entry to the map */
        itr->second.columns.insert(std::make_pair(std::string(column), col));

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }
    return SUCCESS;
}
#else
bool store_file(put_file_request* request, int fd)
{
    char* row = request->username;

    /** Concatenate the path and name */
    char full_path[1024 + 256];
    snprintf(full_path, "%s/%s", request->directory_path, request->filename);

    char* column = full_path;

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return ERR_USER_DOESNT_EXIST;
    }

    /** Check if this column exists */
    map_tablet_row:: iterator row_itr; 
    if ((row_itr = itr->second.columns.find(std::string(column))) != itr->second.columns.end())
    {
        // TODO: Append to this file to change existing file later
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
        
        /** Read the data */
        unsigned int len_read = 0;
        while (len_read != request->file_len)
        {
            len_read += read(fd, data + len_read, request->file_len - len_read);
        }

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
#endif

bool change_password(char* username, char* old_password, char* new_password)
{
    // TODO:    
}

int create_folder(create_folder_request* request)
{

}

int get_folder_content(get_folder_content_request* request, int fd)
{

}

int delete_folder(delete_folder_content_request* request)
{

}

bool process_command(char* command, int len, int fd)
{
    char message[64] = {0};

    //printf("command: %s len: %d\n", command, len);

    //char* command = strtok(buffer, " ");

    //if (command == NULL)
    //{
    //    if (verbose)
    //        printf("command is NULL\n");
    //    return SUCCESS;
    //}

    /** add user command */
    if (strncmp(command, "add", strlen("add")) == 0 || strncmp(command, "ADD", strlen("ADD")) == 0)
    {
        char* username = strtok(command + strlen("add"), " ");
        char* password = strtok(NULL, " ");

        /** Add the user */
        int res = add_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user added", strlen("+OK user added"));
        else if (res == ERR_EXCEEDED_MAX_TABLET_USER_LIMIT)
            strncpy(message, "-ERR max user limit exceeded", strlen("-ERR max user limit exceeded"));
        else if (res == ERR_USER_ALREADY_EXISTS)
            strncpy(message, "-ERR user already exists", strlen("-ERR user already exists"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message) + 1, fd);

        return SUCCESS;
    }
    /** delete user command */
    else if (strncmp(command, "delete", strlen("delete")) == 0 || strncmp(command, "DELETE", strlen("DELETE")) == 0)
    {
        char* username = strtok(command + strlen("delete"), " ");
        char* password = strtok(NULL, " ");

        /** Delete the user */
        int res = delete_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user deleted", strlen("+OK user deleted"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;
    }
    /** change pw command */
    else if (strncmp(command, "changepw", strlen("changepw")) == 0 || strncmp(command, "CHANGEPW", strlen("CHANGEPW")) == 0)
    {
        char* username = strtok(command + strlen("changepw"), " ");
        char* old_password = strtok(NULL, " ");
        char* new_password = strtok(NULL, " ");

        /** Change the password */
        int res = change_password(username, old_password, new_password);

        if (res == SUCCESS)
            strncpy(message, "+OK password changed", strlen("+OK password changed"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        else if (res == ERR_INCORRECT_OLD_PASSWORD)
            strncpy(message, "-ERR old password is incorrect", strlen("-ERR old password is incorrect"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;
    }
    /** authenticate user command */
    else if (strncmp(command, "auth", strlen("auth")) == 0 || strncmp(command, "AUTH", strlen("AUTH")) == 0)
    {
        char* username = strtok(command + strlen("auth"), " ");
        char* password = strtok(NULL, " ");

        printf("received username: %s password: %s\n", username, password);
        int res = auth_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user authenticated", strlen("+OK user authenticated"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        else if (res == ERR_WRONG_PASSWORD)
            strncpy(message, "-ERR incorrect password", strlen("-ERR incorrect password"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;
    }
    /** put email command */
    else if (strncmp(command, "putmail", 7) == 0 || strncmp(command, "PUTMAIL", 7) == 0)
    {
        if (verbose)
            printf("putmail request\n");
        put_mail_request* mail_request = (put_mail_request*)command;

        /** Store the new mail */
        int res = store_email(mail_request);

        if (res == SUCCESS)
            strncpy(message, "+OK email stored", strlen("+OK email stored"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get email list command */
    else if (strncmp(command, "getmail", 7) == 0 || strncmp(command, "GETMAIL", 7) == 0)
    {
        if (verbose)
            printf("get email list request\n");
        get_mail_request* mail_request = (get_mail_request*)command;
        get_mail_response response;

        /** Get mails */
        int res = get_email_list(mail_request, &response);

        if (res == SUCCESS)
        {
            send_msg_to_socket((char*)(&response), sizeof(get_mail_response), fd);
        }
        else
        {
            if (res == ERR_USER_DOESNT_EXIST)
            {
                strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
                message[strlen(message)] = '\0';
                send_msg_to_socket(message, strlen(message), fd);
            }
        }

        return SUCCESS;
    }
    /** get mail body command */
    else if (strncmp(command, "mailbody", 8) == 0 || strncmp(command, "MAILBODY", 8) == 0)
    {
        if (verbose)
            printf("get email body request\n");
        get_mail_body_request* req = (get_mail_body_request*)command;
        get_mail_body_response response;

        /** Get the mail body */
        int res = get_mail_body(req, &response);
        
        if (res == SUCCESS)
        {
            send_msg_to_socket((char*)(&response), sizeof(get_mail_body_response), fd);
        }
        else
        {
            if (res == ERR_INVALID_EMAILID)
                strncpy(message, "-ERR invalid email ID", strlen("-ERR invalid email ID"));
            else if (res == ERR_USER_DOESNT_EXIST)
                strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
            
            message[strlen(message)] = '\0';
            send_msg_to_socket(message, strlen(message), fd);
        }

        return SUCCESS;

    }
    /** delete mail command */
    else if (strncmp(command, "delmail", 8) == 0 || strncmp(command, "DELMAIL", 8) == 0)
    {
        if (verbose)
            printf("delete email request\n");
        delete_mail_request* req = (delete_mail_request*)command;

        /** Delete mail */
        int res = delete_mail(req);
        
        if (res == SUCCESS)
            strncpy(message, "+Ok deleted email", strlen("+Ok deleted email"));
        else if (res == ERR_INVALID_EMAILID)
            strncpy(message, "-ERR invalid email ID", strlen("-ERR invalid email ID"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        
        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;

    }
    /** put file command */
    else if (strncmp(command, "putfile", 7) == 0 || strncmp(command, "PUTFILE", 7) == 0)
    {
        put_file_request* file_request = (put_file_request*)command;

        /** Store the new file */
        int res = store_file(file_request, fd);
        
        if (res == SUCCESS)
            strncpy(message, "+OK file stored", strlen("+OK file stored"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get file command */
    else if (strncmp(command, "getfile", 7) == 0 || strncmp(command, "GETFILE", 7) == 0)
    {
        get_file_request* file_request = (get_file_request*)command;

        /** Get file data */
        int res = get_file_data(file_request, fd);

        if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR file doesn't exist", strlen("-ERR file doesn't exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** delete file command */
    else if (strncmp(command, "delfile", 7) == 0 || strncmp(command, "DELFILE", 7) == 0)
    {
        delete_file_request* file_request = (delete_file_request*)command;

        /** Store the new file */
        int res = delete_file(file_request);
        
        if (res == SUCCESS)
            strncpy(message, "+OK file deleted", strlen("+OK file deleted"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        else if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR file doesn't exist", strlen("-ERR file doesn't exist"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get folder command */
    else if (strncmp(command, "getfolder", 9) == 0 || strncmp(command, "GETFOLDER", 9) == 0)
    {
        get_folder_content_request* folder_request = (get_folder_content_request*)command;

        /** Get file data */
        int res = get_folder_content(folder_request, fd);

        if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR folder doesn't exist", strlen("-ERR folder doesn't exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** create folder command */
    else if (strncmp(command, "mkfolder", 8) == 0 || strncmp(command, "MKFOLDER", 8) == 0)
    {
        create_folder_request* folder_request = (create_folder_request*)command;

        /** Get file data */
        int res = create_folder(folder_request);

        if (res == ERR_FILE_ALREADY_EXISTS)
            strncpy(message, "-ERR folder already exist", strlen("-ERR folder already exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** delete folder command */
    else if (strncmp(command, "delfolder", 9) == 0 || strncmp(command, "DELFOLDER", 9) == 0)
    {
        delete_folder_content_request* del_request = (delete_folder_content_request*)command;

        /** Get file data */
        int res = delete_folder(del_request);

        if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR folder doesn't exist", strlen("-ERR folder doesn't exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
#if 0
    /** put file command */
    else if (strncmp(command, "putfile", 7) == 0 || strncmp(command, "PUTFILE", 7) == 0)
    {
        put_file_request* file_request = (put_file_request*)command;

        /** Store the new file */
        int res = store_file(file_request, fd);
        
        if (res == SUCCESS)
            strncpy(message, "+OK file stored", strlen("+OK file stored"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get file command */
    else if (strncmp(command, "getfile", 7) == 0 || strncmp(command, "GETFILE", 7) == 0)
    {
        get_file_metadata* file_request = (get_file_metadata*)command;

        /** Get file data */
        int res = get_file_data(file_request, fd);

        if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR file doesn't exist", strlen("-ERR file doesn't exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
#endif
    else
    {
        if (verbose)
            printf("command: %s\n", command);
        strncpy(message, "-ERR unknown command", strlen("-ERR unknown command"));

        message[strlen(message)] = '\0';

        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }

   return FAILURE;
}

#ifdef SERIALIZE
void serialize_tablet_to_file(char* filepath)
{
   std::ofstream ofs(filepath);
   boost::archive::text_oarchive oa(ofs);
   oa << tablet;
   ofs.close();

}

//void serialize_tablet_row_to_file(char* filepath)
//{
//    map_tablet::iterator it;
//    boost::archive::text_oarchive oa(ofs);
//    for (it = tablet.begin(); it != tablet.end(); it++)
//    {
//        oa << it;
//    }
//    ofs.close();
//}

map_tablet deserialize_tablet_from_file(char* filepath)
{
   map_tablet new_tablet;
   std::ifstream ifs(filepath);
   boost::archive::text_iarchive ia(ifs);
   ia >> new_tablet;
   ifs.close();

   return new_tablet;
}

//map_tablet deserialize_tablet_row_from_file(char* filepath)
//{
//   map_tablet new_tablet;
//   std::ifstream ifs(filepath);
//   boost::archive::text_iarchive ia(ifs);
//   ia >> new_tablet;
//   ifs.close();
//
//   return new_tablet;
//}

#if 0
int main()
{
    /** Populate map with data */
    tablet_row row;
    row.email_id = std::string("ritika");
    row.password = std::string("pass");
    row.num_emails = 0;
    row.num_files = 1;
    
    tablet_column col;
    col.type = MAIL;

    file_content f_content;
    f_content.file_len = 6;
    f_content.file_data = (char*)malloc(6);
    strcpy(f_content.file_data, "abcde");
    
    col.content = &f_content;
    row.columns.insert(std::make_pair(std::string("col1"), col));

    tablet.insert(std::make_pair("row1", row));

    serialize_tablet_to_file("map.serial");
    
    map_tablet new_tablet;
    new_tablet = deserialize_tablet_from_file("map.serial");

    map_tablet::iterator it;
    for(it = new_tablet.begin(); it != new_tablet.end(); it++)
    {
        printf("row key : %s\n", it->first.c_str());
        printf("email id : %s\n", it->second.email_id.c_str());
        printf("password: %s\n", it->second.password.c_str());
        printf("num emails: %lu\n", it->second.num_emails);
        printf("num files: %lu\n", it->second.num_files);

        map_tablet_row::iterator it_col;

        for (it_col = it->second.columns.begin(); it_col != it->second.columns.end(); it_col++)
        {
            printf("col key: %s\n", it_col->first.c_str());
            printf("type val : %u\n", it_col->second.type);
            file_content* content = (file_content*)it_col->second.content;;
            printf("col content: file len : %lu\n", content->file_len);
            printf("col content: file data : %s\n", content->file_data);
        }
    }
}
#endif
#endif
