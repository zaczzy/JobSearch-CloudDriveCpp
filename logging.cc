#include "logging.h"
#include "key_value.h"
#include "data_types.h"

#include <stdlib.h>
#include <errno.h>
#include <fstream>
#include <vector>

#define LOG_FILE            "key_value.log"

typedef struct 
{
    char* username;
    char* password;
}user_login;

extern bool verbose;
std::ofstream ofs(LOG_FILE,std::fstream::trunc);

unsigned long long get_log_sequence_no()
{
    FILE* file = fopen(LOG_SEQ_NO_FILE, "r");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open sequence no file. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    char* seq_no_str;
    size_t len = 0;
    int ret = getline(&seq_no_str, &len, file);

    fclose(file);

    printf("ret: %d, str: %s\n", ret, seq_no_str);
    if (ret != -1)
    {
        return atoi(seq_no_str);
    }
    else
    {
        return -1;
    }
}

unsigned long long get_checkpoint_version_no()
{
    FILE* file = fopen(CHECKPOINT_FILE, "r");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open sequence no file. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    char* seq_no_str;
    size_t len = 0;
    int ret = getline(&seq_no_str, &len, file);

    fclose(file);

    printf("ret: %d, str: %s\n", ret, seq_no_str);
    if (ret != -1)
    {
        return atoi(seq_no_str);
    }
    else
    {
        return -1;
    }

}

void update_log_file(char* data, unsigned long size)
{
    /** Open the file in append mode and add the data */
    FILE* file = fopen(LOG_FILE, "a");

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

int add_log_entry(op_type type, void* data)
{
#ifdef SERIALIZE

    if (verbose)
        printf("log type: %d\n", type);
    boost::archive::text_oarchive oa(ofs);
    oa << type;

    switch(type)
    {
        case ADD_FILE:
            {
                put_file_request* req = (put_file_request*)data;
                oa << *req;
            }
            break;
        case ADD_FOLDER:
            {
                create_folder_request* req = (create_folder_request*)data;
                oa << *req;
            }
            break;
        case ADD_EMAIL:
            {
                put_mail_request* req = (put_mail_request*)data;
                oa << *req;
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
                oa << *req;
            }
            break;
        case DELETE_FOLDER:
            {
                delete_folder_content_request* req = (delete_folder_content_request*)data;
                oa << *req;
            }
            break;
        case DELETE_EMAIL:
            {
                delete_mail_request* req = (delete_mail_request*)data;
                oa << *req;
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
   
    ofs.flush();
    /** Update the sequence number */
    FILE* file = fopen(LOG_SEQ_NO_FILE, "r+");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open sequence no file. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    char* seq_no_str;
    size_t len = 0;
    int ret = getline(&seq_no_str, &len, file);

    printf("ret: %d, str: %s\n", ret, seq_no_str);
    if (ret != -1)
    {
        unsigned long long sequence_no = atoi(seq_no_str);

        if (verbose)
            printf("current seq no: %llu\n", sequence_no);
        sequence_no++;

        char new_seq_no_str[32];
        sprintf(new_seq_no_str, "%llu", sequence_no);

        /** Write the updated sequence no to the file */
        fseek(file, 0, SEEK_SET);
        fwrite(new_seq_no_str, 1, strlen(new_seq_no_str), file);
        fclose(file);
    }
    else
    {
        if (verbose)
            printf("Couldn't read sequence number. Error : %s\n", strerror(errno));
    }

#endif
}

int replay_log()
{
#ifdef SERIALIZE
    /** Deserialize data from file */
    int type;
    std::ifstream ifs(LOG_FILE);
    
    if (verbose)
        printf("Replaying log\n");
    
    FILE* file = fopen(LOG_SEQ_NO_FILE, "r");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open sequence no file. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    char* seq_no_str;
    size_t len = 0;
    int ret = getline(&seq_no_str, &len, file);

    unsigned long long sequence_no;
    if (ret != -1)
    {
        sequence_no = atoi(seq_no_str);

        if (verbose)
            printf("current seq no: %llu\n", sequence_no);
    }

    unsigned long long ctr = 0;
    while (ctr++ < sequence_no)
    {
        boost::archive::text_iarchive ia(ifs);
        ia >> type;
        printf("type: %d\n", type);
        
        switch(type)
        {
            case ADD_FILE:
                {
                    put_file_request req;
                    ia >> req;

                    /** Add the file */
                    store_file(&req);
                }
                break;
            case ADD_FOLDER:
                {
                    create_folder_request req;
                    ia >> req;

                    /** Add the file */
                    create_folder(&req);
                }
                break;
            case ADD_EMAIL:
                {
                   put_mail_request req;
                   ia >> req;

                    /** Add the email */
                    store_email(&req);
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
                    delete_folder_content_request req;
                    ia >> req;

                    /** Add the file */
                    delete_folder(&req);
                }
                break;
            case DELETE_EMAIL:
                {
                    delete_mail_request req;
                    ia >> req;

                    /** Delete the file */
                    delete_mail(&req);
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
                printf("default log replay case\n");
                break;
        }

    };

    ifs.close();
#endif
}
