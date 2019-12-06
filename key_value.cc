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

#ifdef SERIALIZE
class SerializeCStringHelper {
public:
  SerializeCStringHelper(char*& s) : s_(s) {}
  SerializeCStringHelper(const char*& s) : s_(const_cast<char*&>(s)) {}

private:

  friend class boost::serialization::access;

  template<class Archive>
  void save(Archive& ar, const unsigned version) const {
    bool isNull = (s_ == 0);
    ar & isNull;
    if (!isNull) {
      std::string s(s_);
      ar & s;
    }
  }

  template<class Archive>
  void load(Archive& ar, const unsigned version) {
    bool isNull;
    ar & isNull;
    if (!isNull) {
      std::string s;
      ar & s;
      s_ = strdup(s.c_str());
    } else {
      s_ = 0;
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

private:
  char*& s_;
};
#endif

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
    uint64_t file_len;
    char* file_data;
    
#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & file_len;
       SerializeCStringHelper helper(file_data);
       ar & helper; 
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
    row.num_emails = 0;
    row.num_files = 0;

    if (verbose)
        printf("Added user %s\n", username);
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

bool auth_user(char* username, char* password)
{
    map_tablet::iterator itr;
    
    /** Check if this username exists */
    if ((itr = tablet.find(std::string(username))) == tablet.end())
    {
        if (verbose)
            printf("user %s doesn't exist\n", username);

        return FAILURE;
    }

    /** verify password */
    if (strncmp(itr->second.password.c_str(), password, strlen(itr->second.password.c_str())) == 0)
    {
        if (verbose)
            printf("user authenticated\n");

        return SUCCESS;
    }

    return FAILURE;
}

#if 1
bool store_email(put_mail_request* request)
{
    char* row = request->username;

    map_tablet::iterator itr;
    
    /** Check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
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
bool get_email_list(get_mail_request* request, get_mail_response* response)
{
    char* row = request->username;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
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
bool delete_mail(delete_mail_request* request)
{
    char* row = request->username;
    unsigned long email_id = request->email_id;

    map_tablet::iterator itr;

    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
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
        return FAILURE;
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
bool get_mail_body(get_mail_body_request* request, get_mail_body_response* response)
{
    char* row = request->username;
    unsigned long email_id = request->email_id;

    map_tablet::iterator itr;
    
    /** check if the row exists */
    if ((itr = tablet.find(std::string(row))) == tablet.end())
    {
        if (verbose)
            printf("no row with username %s exists\n", row);

        return FAILURE;
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
        return FAILURE;
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

bool get_file_data(get_file_metadata* request, int fd)
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

        return FAILURE;
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
        send_msg_to_socket(content->file_data, content->file_len, fd);
    }
    else /** column doesn't exist */
    {
        if (verbose)
            printf("no column with %s exists\n", column);
        return FAILURE;
    }
    
   return SUCCESS; 
}

bool store_file(put_file_metadata* request, int fd)
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
        
        /** Read the data */
        unsigned int len_read = 0;
        while (len_read != request->file_len)
        {
            len_read += read(fd, data, request->file_len - len_read);
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
    else if (strncmp(command, "delete", strlen("delete")) == 0 || strncmp(command, "DELETE", strlen("DELETE")) == 0)
    {
        char* username = strtok(command + strlen("delete"), " ");
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
    /** authenticate user command */
    else if (strncmp(command, "auth", strlen("auth")) == 0 || strncmp(command, "AUTH", strlen("AUTH")) == 0)
    {
        char* username = strtok(command + strlen("auth"), " ");
        char* password = strtok(NULL, " ");

        bool res = auth_user(username, password);

        if (res == SUCCESS)
            strncpy(message, "+OK user authenticated", strlen("+OK user authenticated"));
        else
            strncpy(message, "-ERR user not authenticated", strlen("-ERR user not authenticated"));

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
        if (verbose)
            printf("get email list request\n");
        get_mail_request* mail_request = (get_mail_request*)command;
        get_mail_response response;

        /** Get mails */
        bool res = get_email_list(mail_request, &response);

        if (res == SUCCESS)
        {
            send_msg_to_socket((char*)(&response), sizeof(get_mail_response), fd);
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
        put_file_metadata* file_request = (put_file_metadata*)command;

        /** Store the new file */
        bool res = store_file(file_request, fd);
        
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
        get_file_metadata* file_request = (get_file_metadata*)command;

        /** Get file data */
        bool res = get_file_data(file_request, fd);

        if (res != SUCCESS)
        {
            strncpy(message, "-ERR can't get file", strlen("-ERR can't get file"));
            message[strlen(message)] = '\0';
            send_msg_to_socket(message, strlen(message), fd);
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
        bool res = get_mail_body(req, &response);
        
        if (res == SUCCESS)
        {
            send_msg_to_socket((char*)(&response), sizeof(get_mail_body_response), fd);
        }
        else
        {
            strncpy(message, "-ERR can't get mail body", strlen("-ERR can't get mail body"));
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
        bool res = delete_mail(req);
        
        if (res == SUCCESS)
        {
            strncpy(message, "+Ok deleted email", strlen("+Ok deleted email"));
        }
        else
        {
            strncpy(message, "-ERR can't delete email", strlen("-ERR can't delete email"));
        }
        
        message[strlen(message)] = '\0';
        send_msg_to_socket(message, strlen(message), fd);

        return SUCCESS;

    }
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

map_tablet deserialize_tablet_from_file(char* filepath)
{
   map_tablet new_tablet;
   std::ifstream ifs(filepath);
   boost::archive::text_iarchive ia(ifs);
   ia >> new_tablet;
   ifs.close();

   return new_tablet;
}

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
