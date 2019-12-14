#include "logging.h"
#include "key_value.h"
#include "data_types.h"

#include <stdlib.h>
#include <fstream>
#include <vector>

#define LOG_FILE            "key_value.log"
#define LOG_SEQ_NO_FILE     "log_seq_no.txt"

typedef struct 
{
    char* username;
    char* password;
}user_login;

std::vector<log_entry> log_entry_table;
extern bool verbose;

int add_log_entry(op_type type, void* data)
{
#ifdef SERIALIZE
    std::ofstream ofs(LOG_FILE,std::fstream::app);
    boost::archive::text_oarchive oa(ofs);
    oa << type;

    switch(type)
    {
        case ADD_FILE:
            {
                put_file_request* req = (put_file_request*)data;
                oa << req;
            }
            break;
        case ADD_FOLDER:
            {
                create_folder_request* req = (create_folder_request*)data;
                oa << req;
            }
            break;
        case ADD_EMAIL:
            {
                put_mail_request* req = (put_mail_request*)data;
                oa << req;
            }
            break;
        case ADD_USER:
            {
                char* str = (char*)data;
                SerializeCStringHelper helper(str);
                oa << helper;
            }
            break;
        case CHNG_PW:
            {
                char* str = (char*)data;
                SerializeCStringHelper helper(str);
                oa << helper;
            }
            break;
        case DELETE_FILE:
            {
                delete_file_request* req = (delete_file_request*)data;
                oa << req;
            }
            break;
        case DELETE_FOLDER:
            {
                delete_folder_content_request* req = (delete_folder_content_request*)data;
                oa << req;
            }
            break;
        case DELETE_EMAIL:
            {
                delete_mail_request* req = (delete_mail_request*)data;
                oa << req;
            }
            break;
        case DELETE_USER:
            {
                char* str = (char*)data;
                SerializeCStringHelper helper(str);
                oa << helper;
            }
            break;
        default:
            break;
    }
   
    /** Update the sequence number */
    FILE* file = fopen(LOG_SEQ_NO_FILE, "w+");

    char* seq_no_str;
    size_t len;
    int ret = getline(&seq_no_str, &len, file);

    if (ret != -1)
    {
        unsigned long sequence_no = atoi(seq_no_str);
        sequence_no++;

        char new_seq_no_str[32];
        sprintf(new_seq_no_str, "%lu", sequence_no);

        /** Write the updated sequence no to the file */
        fseek(file, 0, SEEK_SET);
        fwrite(new_seq_no_str, 1, strlen(new_seq_no_str), file);
        fclose(file);
    }
    else
    {
        if (verbose)
            printf("Couldn't read sequence number\n");
    }


    ofs.close();
#endif
}

int replay_log()
{
#ifdef SERIALIZE
    /** Deserialize data from file */
    op_type type;
    std::ifstream ifs(LOG_FILE);
    boost::archive::text_iarchive ia(ifs);
    
    if (verbose)
        printf("Replaying log\n");
    
    //std::streampos streamEnd = ifs.seekg(0, std::ios_base::end).tellg();

    //while (ifs.tellp() < streamEnd)
    {
        ia >> type;
        
        switch(type)
        {
            case ADD_FILE:
                {
                    put_file_request* req;
                    ia >> req;

                    /** Add the file */
                    store_file(req);
                }
                break;
            case ADD_FOLDER:
                {
                    create_folder_request* req;
                    ia >> req;

                    /** Add the file */
                    create_folder(req);
                }
                break;
            case ADD_EMAIL:
                {
                   put_mail_request* req;
                    ia >> req;

                    /** Add the email */
                    store_email(req);
                }
                break;
            case ADD_USER:
                {
                    char* command;
                    SerializeCStringHelper helper(command);
                    ia >> helper;

                    char* username = strtok(command + strlen("add"), " ");
                    char* password = strtok(NULL, " ");
                    
                    add_user(username, password);
                }
                break;
            case CHNG_PW:
                {
                    char* command;
                    SerializeCStringHelper helper(command);
                    ia >> helper;

                    char* username = strtok(command + strlen("changepw"), " ");
                    char* old_password = strtok(NULL, " ");
                    char* new_password = strtok(NULL, " ");

                    change_password(username, old_password, new_password);
                }
                break;
            case DELETE_FILE:
                {
                    delete_file_request* req;
                    ia >> req;

                    /** Add the file */
                    delete_file(req);
                }
                break;
            case DELETE_FOLDER:
                {
                    delete_folder_content_request* req;
                    ia >> req;

                    /** Add the file */
                    delete_folder(req);
                }
                break;
            case DELETE_EMAIL:
                {
                    delete_mail_request* req;
                    ia >> req;

                    /** Add the file */
                    delete_mail(req);
                }
                break;
            case DELETE_USER:
                {
                    char* command;
                    SerializeCStringHelper helper(command);
                    ia >> helper;

                    char* username = strtok(command + strlen("add"), " ");
                    char* password = strtok(NULL, " ");
                    
                    delete_user(username, password);
                }
                break;
            default:
                break;
        }

    };

    ifs.close();
#endif
}
