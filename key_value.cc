#include "key_value.h"

#include <algorithm>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include "socket.h"

#include "error.h"
#include "logging.h"
#include "seq_consistency.h"

#define MAX_TABLET_USERS    100     // TODO: Check with team 

int master_sockfd; 
extern int server_no, group_no;

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

typedef enum
{
    FILE_TYPE,
    DIRECTORY_TYPE
}file_type;

typedef struct fd_entry fd_entry;
#ifdef SERIALIZE
class VectorHelper {
public:
  VectorHelper(std::vector<fd_entry>*& _vect) : vect(_vect) {}

private:

  friend class boost::serialization::access;

  template<class Archive>
  void save(Archive& ar, const unsigned version) const 
  {  
      ar & vect->size();
      for(int i = 0 ; i < vect->size(); i++)
      {
          ar & vect[i];
      }
  }

  template<class Archive>
  void load(Archive& ar, const unsigned version) 
  {
       int vect_size;
       ar & vect_size;

       for (int i = 0; i < vect_size; i++)
       {
           fd_entry entry;
           ar & entry;
           vect->push_back(entry);
       }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

private:
  std::vector<fd_entry>*& vect;
};
#endif


typedef struct fd_entry 
{
    file_type type;
    char name[1024];
     
    int res;
    bool operator ==( const fd_entry& m ) const
    {
        return (m.type == type && ((strncmp(m.name, name, strlen(name))) == 0));
    }

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & type;
       ar & name; 
   }
#endif
}fd_entry;

typedef struct
{
    file_type type;  // 0 - Directory, 1 - File
    
    /** If it is a file */
    uint64_t file_len;
    char* file_data;

    /** If it is a directory */
    uint64_t num_files;
    std::vector<fd_entry>* entry;
    
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
       VectorHelper vect_helper(entry);
       ar & entry;
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
      //column_type type;
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

void clear_tablet()
{
    tablet.clear();
}

void add_root_folder(map_tablet::iterator itr)
{
    tablet_column col;
    col.type = DRIVE;

    /** Add the new folder */
    file_content* content = (file_content*)malloc(sizeof(file_content) * sizeof(char));
    // TODO: Check NULL

    if (verbose)
        printf("num cols: %lu\n", itr->second.columns.size());
    content->type = DIRECTORY_TYPE;
    content->num_files = 0;
    content->entry = new std::vector<fd_entry>();

    col.content = content; 
    /** Add the entry to the map */
    itr->second.columns.insert(std::make_pair(std::string("/r00t"), col));

    if (verbose)
        printf("num cols: %lu\n", itr->second.columns.size());

    if (verbose)
        printf("added root folder for the user\n");
}

int add_user(char* username, char* password)
{
    printf("username: %s password: %s\n", username, password);
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

    tablet.insert(std::make_pair(username, row));

    if (verbose)
        printf("Added user %s\n", username);
    
    itr = tablet.find(std::string(username));
        
    /** Add the root folder for this user */
    add_root_folder(itr);

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

    if (verbose)
        printf("Deleted user %s\n", username);

    return SUCCESS;
}

int auth_user(char* username, char* password)
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
    else
    {
        if (verbose)
            printf("user not authenticated\n");
    }
    

    return ERR_WRONG_PASSWORD;
}

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
    content->header->email_id = email_id;
    content->email_id = email_id;
    content->body_len = request->email_len;

    if (verbose)
        printf("email body : %s \n subject: %s\n email len : %lu\n", request->email_body, request->header.subject, request->email_len);
    
    col.content = content; 
    /** Add the entry to the map */
    itr->second.columns.insert(std::make_pair(std::string(email_id_str), col));

    if (verbose)
        printf("Added email id %lu to row %s\n", email_id, row);
    
    return SUCCESS;
}

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

int delete_mail(delete_mail_request* request)
{
    char* row = request->username;
    unsigned long email_id = request->email_id;
    // TODO: Remove
    printf("delete email id %lu\n", email_id);

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

        if (verbose)
            printf("email %lu deleted\n", request->email_id);
    }
    else /** email id doesn't exist */
    {
        if (verbose)
            printf("no email with id %lu exists\n", email_id);
        return ERR_INVALID_EMAILID;
    }

   return SUCCESS;
}

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
        strncpy(response->mail_body, content->email_body, content->body_len);
        response->mail_body_len = content->body_len;

        if (verbose)
            printf("email body : %s \n email len : %lu\n", response->mail_body, response->mail_body_len);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no email with id %lu  exists\n", email_id);
        return ERR_INVALID_EMAILID;
    }
    
   return SUCCESS; 
}

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
        
        /** Delete this file to its parent's list */
        map_tablet_row:: iterator parent_itr; 
        if ((parent_itr = itr->second.columns.find(std::string(request->directory_path))) != itr->second.columns.end())
        {
            file_content* content = (file_content*)(parent_itr->second.content);
            content->num_files--;

            fd_entry entry;
            entry.type = FILE_TYPE;
            strncpy(entry.name, request->filename, strlen(request->filename));
            std::vector<fd_entry>::iterator a = std::find(content->entry->begin(), content->entry->end(), entry);
            content->entry->erase(a);

            if (verbose)
                printf("file %s deleted\n", request->filename);
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
            response.f_len = content->file_len;
            send_msg_to_socket((char*)(&response), sizeof(response), fd);

            if (verbose)
                printf("sent file : %s\n", full_path);
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
bool store_file(put_file_request* request)
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
    }
    else /** Create a column with the given email ID */
    {
        /** Add the new column to this row */
        tablet_column col;
        col.type = DRIVE;

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
            printf("added file %s\n", column);

        /** Add this file to its parent's list */
        map_tablet_row:: iterator parent_itr; 
        if ((parent_itr = itr->second.columns.find(std::string(request->directory_path))) != itr->second.columns.end())
        {
            file_content* content = (file_content*)(parent_itr->second.content);
            content->num_files++;
            fd_entry entry;
            entry.type = FILE_TYPE;
            strncpy(entry.name, request->filename, strlen(request->filename));
            content->entry->push_back(entry);
        }


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
    map_tablet::iterator itr;
    
    /** Check if this username exists */
    if ((itr = tablet.find(std::string(username))) == tablet.end())
    {
        if (verbose)
            printf("user %s doesn't exist\n", username);

        return ERR_USER_DOESNT_EXIST;
    }

    /** verify old password */
    if (strncmp(itr->second.password.c_str(), old_password, strlen(itr->second.password.c_str())) == 0)
    {
        /** Change password */
        itr->second.password = std::string(new_password); 
        if (verbose)
            printf("password successfully changed\n");

        return SUCCESS;
    }

    return ERR_WRONG_PASSWORD;
}

int create_folder(create_folder_request* request)
{
    char* row = request->username;

    /** Concatenate the path and name */
    char full_path[1024 + 256];
    sprintf(full_path, "%s/%s", request->directory_path, request->folder_name);

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
        return ERR_FILE_ALREADY_EXISTS;
    }
    else /** Create a column with the given folder name */
    {
        /** Add the new column to this row */
        tablet_column col;
        col.type = DRIVE;

        file_content* content = (file_content*)malloc(sizeof(file_content) * sizeof(char));
     // TODO: Check NULL

     if (verbose)
        printf("num cols: %lu\n", itr->second.columns.size());
     content->type = DIRECTORY_TYPE;
        content->num_files = 0;
     content->entry = new std::vector<fd_entry>();

        col.content = content; 
    /** Add the entry to the map */
    itr->second.columns.insert(std::make_pair(std::string(column), col));

#if 0
        

        /** Add the new folder */
        file_content* content = (file_content*)malloc(sizeof(file_content) * sizeof(char));
        // TODO: Check NULL

        content->num_files = 0;

        col.content = content; 
        /** Add the entry to the map */
        itr->second.columns.insert(std::make_pair(std::string(column), col));
#endif
        /** Add this folder to its parent's list */
        map_tablet_row:: iterator parent_itr; 
        if ((parent_itr = itr->second.columns.find(std::string(request->directory_path))) != itr->second.columns.end())
        {
            file_content* content = (file_content*)(parent_itr->second.content);
            content->num_files++;
            fd_entry entry;
            memset(&entry, 0, sizeof(entry));
            entry.type = DIRECTORY_TYPE;
            strncpy(entry.name, request->folder_name, strlen(request->folder_name));
            if (verbose)
                printf("Added folder %s to its parent folder %s\n", entry.name, parent_itr->first.c_str());
            content->entry->push_back(entry);
        }

        if (verbose)
            printf("Added column %s to row %s\n", column, row);
    }
    return SUCCESS;
}

int get_folder_content(get_folder_content_request* request, int fd, char** response_str)
{
    char* row = request->username;
    *response_str = NULL;
    if (verbose)
    printf("Getting folder contents for dir path  : %s and folder name : %s\n", request->directory_path, request->folder_name);
    /** Concatenate the path and name */
    char full_path[1024 + 256];
    sprintf(full_path, "%s/%s", request->directory_path, request->folder_name);

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
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("folder %s doesn't exist\n", column);
        return ERR_FILE_DOESNT_EXIST;
    }
    else 
    {
        tablet_column* col = &(row_itr->second);
        file_content* content = (file_content*)col->content;
        //file_content* content = (file_content*)(row_itr->content);

        printf("type : %d num_files: %lu\n", content->type, content->num_files);
        std::string content_list;

        if (verbose)
            printf("content entry vector size : %lu\n", content->entry->size());
            
        if (verbose)
            printf("contents of folder : %s with directory path : %s\n", request->folder_name, request->directory_path);
        for(auto it = content->entry->begin(); it != content->entry->end(); it++)
        {
            if (verbose)
                printf("%s\n", it->name);
            content_list += ((it->type == FILE_TYPE) ? 'F' : 'D') + std::string(it->name) + '~';
        }
        char* response;
        if (content_list.length() == 0)
        {
            response = (char*)malloc(2 * sizeof(char));
            strcpy(response, "~");

            if (verbose)
                printf("response if len 1: %s\n", response);
        }
        else
        {
            response = (char*)malloc((content_list.length()+1) * sizeof(char));
            strncpy(response, content_list.c_str(), content_list.length());
            response[content_list.length()] = '\0';


            if (verbose)
                printf("response if len > 1: %s\n", response);
        }
       
        *response_str = response;
    }
    return SUCCESS;
}

int delete_folder(delete_folder_content_request* request)
{
    char* row = request->username;

    /** Concatenate the path and name */
    char full_path[1024 + 256];
    sprintf(full_path, "%s/%s", request->directory_path, request->folder_name);

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
    if ((row_itr = itr->second.columns.find(std::string(column))) == itr->second.columns.end())
    {
        if (verbose)
            printf("folder %s doesn't exist\n", column);
        return ERR_FILE_DOESNT_EXIST;
    }
    else /** Delete the folder */
    {
        /** Iterate through all the columns of the map and delete the ones which are DRIVE type and
            their name contains the folder's name */
        map_tablet_row:: iterator row_col;
        int res;
        for(row_col = itr->second.columns.begin(); row_col != itr->second.columns.end(); row_col++)
        {
            if ((res = strncmp(row_col->first.c_str(), column, strlen(column))) == 0)
            {
                /** Delete this column */
                itr->second.columns.erase(row_col);

                if (verbose)
                    printf("Deleted folder %s\n", column);
            }
        }

        /** Delete this folder to its parent's list */
        map_tablet_row:: iterator parent_itr; 
        if ((parent_itr = itr->second.columns.find(std::string(request->directory_path))) != itr->second.columns.end())
        {
            file_content* content = (file_content*)(parent_itr->second.content);
            content->num_files--;

            fd_entry entry;
            entry.type = DIRECTORY_TYPE;
            strncpy(entry.name, request->folder_name, strlen(request->folder_name));
            std::vector<fd_entry>::iterator a = std::find(content->entry->begin(), content->entry->end(), entry);
            content->entry->erase(a);
        }
        if (verbose)
            printf("Deleted folder %s recursively\n", column);
    }
    return SUCCESS;
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
        /** Remove the newline character */
        //command[strlen(command) - 1] = '\0';
        /** LOg this entry into the log file */
        add_log_entry(ADD_USER, command);

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

        if(fd != -1)
        send_msg_to_socket(message, strlen(message) + 1, fd);

        return SUCCESS;
    }
    /** delete user command */
    else if (strncmp(command, "delete", strlen("delete")) == 0 || strncmp(command, "DELETE", strlen("DELETE")) == 0)
    {
        /** LOg this entry into the log file */
        add_log_entry(DELETE_USER, command);

        char* username = strtok(command + strlen("delete"), " ");
        char* password = strtok(NULL, " ");

        /** Delete the user */
        int res = delete_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user deleted", strlen("+OK user deleted"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;
    }
    /** change pw command */
    else if (strncmp(command, "changepw", strlen("changepw")) == 0 || strncmp(command, "CHANGEPW", strlen("CHANGEPW")) == 0)
    {
        /** LOg this entry into the log file */
        add_log_entry(CHNG_PW, command);

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

        if(fd != -1)
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

        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;
    }
    /** put email command */
    else if (strncmp(command, "putmail", 7) == 0 || strncmp(command, "PUTMAIL", 7) == 0)
    {
        if (verbose)
            printf("putmail request\n");
        put_mail_request* mail_request = (put_mail_request*)command;

        /** LOg this entry into the log file */
        add_log_entry(ADD_EMAIL, mail_request);

        /** Store the new mail */
        int res = store_email(mail_request);

        if (res == SUCCESS)
            strncpy(message, "+OK email stored", strlen("+OK email stored"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        if(fd != -1)
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
           if(fd != -1)
            send_msg_to_socket((char*)(&response), sizeof(get_mail_response), fd);
        }
        else
        {
            if (res == ERR_USER_DOESNT_EXIST)
            {
                strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
                message[strlen(message)] = '\0';
                if(fd != -1)
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
          if(fd != -1)
            send_msg_to_socket((char*)(&response), sizeof(get_mail_body_response), fd);
        }
        else
        {
            if (res == ERR_INVALID_EMAILID)
                strncpy(message, "-ERR invalid email ID", strlen("-ERR invalid email ID"));
            else if (res == ERR_USER_DOESNT_EXIST)
                strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
            
            message[strlen(message)] = '\0';
            if(fd != -1)
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

        /** LOg this entry into the log file */
        add_log_entry(DELETE_EMAIL, req);

        /** Delete mail */
        int res = delete_mail(req);
        
        if (res == SUCCESS)
            strncpy(message, "+Ok deleted email", strlen("+Ok deleted email"));
        else if (res == ERR_INVALID_EMAILID)
            strncpy(message, "-ERR invalid email ID", strlen("-ERR invalid email ID"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        
        message[strlen(message)] = '\0';
        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;

    }
    /** put file command */
    else if (strncmp(command, "putfile", 7) == 0 || strncmp(command, "PUTFILE", 7) == 0)
    {
        put_file_request* file_request = (put_file_request*)command;

        if (verbose)
        {
            printf("putfile req:\n");
            printf("username: %s\n", file_request->username);
            printf("filename: %s\n", file_request->filename);
            printf("directory path: %s\n", file_request->directory_path);
            printf("data: %s\n", file_request->data);
            printf("len: %lu\n", file_request->chunk_len);
        }
        if (verbose)
            printf("adding log entry for putfile\n");

        /** LOg this entry into the log file */
        add_log_entry(ADD_FILE, file_request);

        if (verbose)
            printf("added log entry for putfile\n");

        /** Store the new file */
        int res = store_file(file_request);
        
        if (res == SUCCESS)
            strncpy(message, "+OK file stored", strlen("+OK file stored"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        if(fd != -1)
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
        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** delete file command */
    else if (strncmp(command, "delfile", 7) == 0 || strncmp(command, "DELFILE", 7) == 0)
    {
        delete_file_request* file_request = (delete_file_request*)command;

        /** LOg this entry into the log file */
        add_log_entry(DELETE_FILE, file_request);

        /** Store the new file */
        int res = delete_file(file_request);
        
        if (res == SUCCESS)
            strncpy(message, "+OK file deleted", strlen("+OK file deleted"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        else if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR file doesn't exist", strlen("-ERR file doesn't exist"));

        message[strlen(message)] = '\0';

        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);
            
        return SUCCESS;
    }
    /** get folder command */
    else if (strncmp(command, "getfolder", 9) == 0 || strncmp(command, "GETFOLDER", 9) == 0)
    {
        get_folder_content_request* folder_request = (get_folder_content_request*)command;

        /** Get file data */
        char* response;
        int res = get_folder_content(folder_request, fd, &response);

        if (verbose && response != NULL)
            printf("sending response to get folder content : %s\n", response);
        else
            printf("sending response to get folder content NULL\n");
        if (res == SUCCESS)
        {
            send_msg_to_socket(response, strlen(response), fd);
            return SUCCESS;
        }
        if (res == ERR_FILE_DOESNT_EXIST)
            strncpy(message, "-ERR folder doesn't exist", strlen("-ERR folder doesn't exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));


        message[strlen(message)] = '\0';
        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** create folder command */
    else if (strncmp(command, "mkfolder", 8) == 0 || strncmp(command, "MKFOLDER", 8) == 0)
    {
        create_folder_request* folder_request = (create_folder_request*)command;

        /** LOg this entry into the log file */
        add_log_entry(ADD_FOLDER, folder_request);

        /** Get file data */
        int res = create_folder(folder_request);

        if (res == SUCCESS)
            strncpy(message, "+OK added folder", strlen("+OK added folder"));
        if (res == ERR_FILE_ALREADY_EXISTS)
            strncpy(message, "-ERR folder already exist", strlen("-ERR folder already exist"));
        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));
        
        message[strlen(message)] = '\0';
        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** create folder command */
    else if (strncmp(command, "mkfolder", 8) == 0 || strncmp(command, "MKFOLDER", 8) == 0)
    {
        create_folder_request* folder_request = (create_folder_request*)command;

        /** LOg this entry into the log file */
        add_log_entry(ADD_FOLDER, folder_request);

        /** Get file data */
        int res = create_folder(folder_request);

        if (res == ERR_FILE_ALREADY_EXISTS)
            strncpy(message, "-ERR folder already exist", strlen("-ERR folder already exist"));

        else if (res == ERR_USER_DOESNT_EXIST)
            strncpy(message, "-ERR user doesn't exist", strlen("-ERR user doesn't exist"));

        message[strlen(message)] = '\0';

        if(fd != -1)
        send_msg_to_socket(message, strlen(message), fd);
        
        return SUCCESS;
    }
    /** delete folder command */
    else if (strncmp(command, "delfolder", 9) == 0 || strncmp(command, "DELFOLDER", 9) == 0)
    {
        delete_folder_content_request* del_request = (delete_folder_content_request*)command;

        /** LOg this entry into the log file */
        add_log_entry(DELETE_FOLDER, del_request);

        /** Get file data */
        int res = delete_folder(del_request);

        if (res == SUCCESS)
            strncpy(message, "+OK folder deleted", strlen("+OK folder deleted"));
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
void serialize_tablet_to_file(const char* filepath)
{
   std::ofstream ofs(filepath);
   boost::archive::text_oarchive oa(ofs);
   oa << tablet;
   ofs.close();

}

map_tablet deserialize_tablet_from_file(char* filepath)
{
   map_tablet new_tablet;
   std::ifstream ifs(filepath);
   boost::archive::text_iarchive ia(ifs);
   ia >> new_tablet;
   ifs.close();

   return new_tablet;
}

void take_checkpoint()
{
    if (verbose)
        printf("====Taking checkpoint=====\n");

    serialize_tablet_to_file(CHECKPOINT_FILE);
    
    if (verbose)
        printf("====Taken checkpoint successfully====");

    /** Read and update checkpoint version no */
    FILE* file = fopen(CHECKPOINT_VERSION_FILE, "r+");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open version no file. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    char* ver_no_str;
    size_t len = 0;
    int ret = getline(&ver_no_str, &len, file);

    printf("ret: %d, str: %s\n", ret, ver_no_str);
    if (ret != -1)
    {
        unsigned long long version_no = atoi(ver_no_str);

        if (verbose)
            printf("current seq no: %llu\n", version_no);
        version_no++;

        char new_ver_no_str[32];
        sprintf(new_ver_no_str, "%llu", version_no);

        /** Write the updated version no to the file */
        fseek(file, 0, SEEK_SET);
        fwrite(new_ver_no_str, 1, strlen(new_ver_no_str), file);
        fclose(file);
    
        if (verbose)
            printf("====Updated checkpint version no successfully====");

    }
    else
    {
        if (verbose)
            printf("Couldn't read version number. Error : %s\n", strerror(errno));
    }
}

void write_checkpoint_to_file(char* data, unsigned long size)
{
    /** Open the checkpoint file in truncate mode */
    FILE* file = fopen(CHECKPOINT_FILE, "w");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open log file for appending. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    unsigned long size_written = 0;

    while (size_written != size)
    {
        size_written += fwrite(data + size_written, 1, size - size_written, file);
    }

    fclose(file);

}

void respond_with_seq_no(recovery_req req)
{
    recovery_resp resp;
    strncpy(resp.prefix, "recovery_res", strlen("recovery_res"));
    
    if (req.log_sequence_no == get_log_sequence_no())
    {
        resp.seq_no_match = true;
    }
    else
    {
        resp.seq_no_match = false;
        // TODO: Send the log diff
    //resp.log_size = ;
    }
    
    if (req.checkpoint_version_no == get_checkpoint_version_no())
    {
        resp.ver_no_match = true;
    }
    else
    {
        resp.ver_no_match = false;
        // TODO: Send the checkpoint
    //resp.checkpoint_size = ;
    }
}

void ask_primary(unsigned short group_no)
{
    char request[64];
    
    sprintf(request, "%02huprimary", group_no);

    if (verbose)
        printf("primary no request len : %lu data : %s\n", strlen(request), request);

    int bytes = write(master_sockfd, request, strlen(request));
    printf("bytes: %d", bytes);
    char buff[64];
    bytes = read(master_sockfd, buff, sizeof(buff));

    // TODO: Update primary IP and port
    //primary_ip 
}

// void send_new_log(unsigned long long seq_no)
// {
    
// }

// void send_checkpoint()
// {

// }

// void recover()
// {
//     /** Ask primary from master */
//     // TODO: get primary result ?
//     ask_primary(group_no);

//     /** Send the log sequence no and checkpoint version no to primary */
//     recovery_req s_no;
   
//     strncpy(s_no.prefix, "seq_no", strlen("seq_no"));
//     s_no.log_sequence_no = get_log_sequence_no();
//     s_no.checkpoint_version_no = get_checkpoint_version_no();
//     // TODO: Send this structure to primary

//     /** Read the response of primary */
//     recovery_resp* res;
//     // TODO: Read the response

//     if (res->ver_no_match && res->seq_no_match)
//     {
//         /** Just replay the existing log */
//         replay_log();
//     }
//     else if (res->ver_no_match && !res->seq_no_match)
//     {
//         /** Read the new log from primary */
//         char* data;
//         unsigned long size;
//         // TODO: read new log from primary

//         /** Append the new log to log file */
//         update_log_file(data, size);

//         /** Replay the updated log file */
//         replay_log();
//     }
//     else if (!res->ver_no_match && !res->seq_no_match)
//     {
//         /** Read both the log and checkpoint from primary */
//         // TODO: Read the checkpoint, write as it is to the file
//         char* checkpoint;
//         unsigned long size;
//         write_checkpoint_to_file(checkpoint, size);
    
//         /** Populate tablet with checkpoint */
//         deserialize_tablet_from_file(CHECKPOINT_FILE);

//         // TODO: Read the log
//         char* data;
//         unsigned long log_size;
//         update_log_file(data, log_size);

//         /** Replay the log */
//         replay_log();
//     }

//     /** Tell the master that I am ready to receive requests normally */
//     int bytes = write(master_sockfd, "ready", strlen("ready"));
// }


#endif
