#include "logging.h"
#include "key_value.h"
#include "data_types.h"

#include <stdlib.h>
#include <errno.h>
#include <fstream>
#include <vector>

#define LOG_FILE            "key_value.log"
#define LOG_SEQ_NO_FILE     "log_seq_no.txt"

typedef struct 
{
    char* username;
    char* password;
}user_login;

//std::vector<log_entry> log_entry_table;
extern bool verbose;
std::ofstream ofs(LOG_FILE,std::fstream::app);
boost::archive::text_oarchive oa(ofs);

int add_log_entry(op_type type, void* data)
{
#ifdef SERIALIZE
    //std::ofstream ofs(LOG_FILE,std::fstream::app);
    //boost::archive::text_oarchive oa(ofs);

    // TODO: remove
    printf("log type: %d\n", type);
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

                // TODO: rmeove
                printf("tets req->username: %s\n", req->username);
                oa << *req;
            }
            break;
        case ADD_USER:
            {
                printf("initial data: %s\n", (char*)data);
                char* str = (char*)data;

                // TODO: Remove
                printf("str: %s\n", str);
                SerializeCStringHelper helper(str);
                oa << helper;

                // TODO: remove
                printf("str from helper: %s\n", helper.getstr());

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
    boost::archive::text_iarchive ia(ifs);
    
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
        ia >> type;
        printf("type: %d\n", type);
        
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
                    // TODO remove
                    printf("put email log\n");

                   put_mail_request req;
                   ia >> req;

                   printf("testing req->username: %s\n", req.username);
                    /** Add the email */
                    store_email(&req);
                }
                break;
            case ADD_USER:
                {
                    char* command;
                    SerializeCStringHelper helper(command);
                    ia >> helper;

                    // TODO: Remove
                    printf("str read from helper: %s\n", helper.getstr());
                    printf("command: %s\n", command);

                    char* username = strtok(command + strlen("add"), " ");
                    char* password = strtok(NULL, " ");
                    
                    // TODO: Remove
                    printf("username : %s passwod: %s\n", username, password);
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
                printf("default log replay case\n");
                break;
        }

    };

    ifs.close();
#endif
}
