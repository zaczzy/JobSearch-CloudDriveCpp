#include "logging.h"
#include "key_value.h"
#include "data_types.h"

#include <stdlib.h>
#include <errno.h>
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>

#define LOG_FILE            "key_value.log"
extern char log_file[256];

typedef struct 
{
    char* username;
    char* password;
}user_login;

extern bool verbose;
std::ofstream ofs;
extern char seq_no_file[256];
extern char checkpoint_ver_file[256];
extern pthread_mutex_t log_file_mutx;

long long get_log_sequence_no()
{
    pthread_mutex_lock(&log_file_mutx);
    FILE* file = fopen(seq_no_file, "r");

    if (file == NULL)
    {
        if (verbose)
            printf("unable to open sequence no file. Error : %s\n", strerror(errno));
        
        exit(EXIT_FAILURE);
    }

    char* seq_no_str;
    size_t len = 0;
    int ret = getline(&seq_no_str, &len, file);
    perror("Failed at getline");

    fclose(file);
    pthread_mutex_unlock(&log_file_mutx);

    printf("getlint ret: %d, current log sequence: %s\n", ret, seq_no_str);
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
    FILE* file = fopen(checkpoint_ver_file, "r");

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
                if(verbose)
                    printf("adding log==\n");
                put_file_request* req = (put_file_request*)data;
                oa << *req;
                 if(verbose)
                    printf("added log==\n");
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

    if (verbose)
        printf("updating sequence no \n");
    /** Update the sequence number */
    pthread_mutex_lock(&log_file_mutx);
    FILE* file = fopen(seq_no_file, "r+");

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
        pthread_mutex_unlock(&log_file_mutx);

#endif
    return 0; // zac added
}

int replay_log()
{
#ifdef SERIALIZE
    /** Deserialize data from file */
    int type;
    std::ifstream ifs(log_file);
    
    if (verbose)
        printf("Replaying log\n");
    
    pthread_mutex_lock(&log_file_mutx);
    FILE* file = fopen(seq_no_file, "r");

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
    fclose(file);
    
    pthread_mutex_unlock(&log_file_mutx);

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
    return 0; // zac added
}

int transfer_log(int cfd, int req_sequence_no, int requestor_id)
{
#ifdef SERIALIZE
    /** Deserialize data from file */
    int type;
    std::ifstream ifs(log_file);
    
    if (verbose)
        printf("Replaying log\n");
    
    pthread_mutex_lock(&log_file_mutx);
    FILE* file = fopen(seq_no_file, "r");

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
            printf("primary current seq no: %llu\n", sequence_no);
    }
    fclose(file);
    
    pthread_mutex_unlock(&log_file_mutx);
       
    unsigned long long ctr = req_sequence_no;
    unsigned long long i = 0;
    logging_consensus transfer_struct;

    while (i++ < sequence_no)
    {
        boost::archive::text_iarchive ia(ifs);
        ia >> type;
        printf("type: %d\n", type);
        
        switch(type)
        {
            case ADD_FILE:
                {
                    put_file_request req;
                    int req_len = sizeof(put_file_request);
                    ia >> req;

                    /** Add the file */
                    if(i > ctr) {
                      
                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, &req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }

                      //store_file(&req);

                    }
                }
                break;
            case ADD_FOLDER:
                {
                    create_folder_request req;
                    int req_len = sizeof(create_folder_request);
                    ia >> req;

                    /** Add the file */
                    if(i > ctr) {

                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, &req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }

                      //create_folder(&req);

                    }
                }
                break;
            case ADD_EMAIL:
                {
                   put_mail_request req;
                   int req_len = sizeof(put_mail_request);
                   ia >> req;

                    /** Add the email */
                    if(i > ctr) {
                      //store_email(&req);
                     
                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, &req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }
                     
                    }
                }
                break;
            case ADD_USER:
                {
                    char* command;
                    SerializeCStringHelper helper(command);
                    ia >> helper;
                    
                    if(i > ctr) {
                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, command, strlen(command)+ 1); 
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }
                     
                    }
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

                    if(i > ctr) {
                      //change_password(username, old_password, new_password);
                    /*
                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 0;
                      memcpy(transfer_struct.data, req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }
                    */
                    }
                }
                break;
            case DELETE_FILE:
                {
                    delete_file_request* req;
                    int req_len = sizeof(delete_file_request);
                    ia >> req;

                    /** Add the file */
                    if(i > ctr) {
                      //delete_file(req);

                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, &req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }
                    }
                }
                break;
            case DELETE_FOLDER:
                {
                    delete_folder_content_request req;
                    int req_len = sizeof(delete_folder_content_request);
                    ia >> req;

                    /** Add the file */
                    if(i > ctr) {
                      //delete_folder(&req);

                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, &req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }

                    }
                }
                break;
            case DELETE_EMAIL:
                {
                    delete_mail_request req;
                    int req_len = sizeof(delete_mail_request);
                    ia >> req;

                    /** Delete the file */
                    if(i > ctr) {
                      //delete_mail(&req);

                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = requestor_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 5555;
                      memcpy(transfer_struct.data, &req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }

                    }
                }
                break;
            case DELETE_USER:
                {
                    char* command;
                    SerializeCStringHelper helper(command);
                    ia >> helper;

                    char* username = strtok(command + strlen("add"), " ");
                    char* password = strtok(NULL, " ");
                    
                    if(i > ctr) {
                      //delete_user(username, password);
                   /* 
                      transfer_struct.primaryId = -1;
                      transfer_struct.glbl_seq_num = -1;
                      transfer_struct.requestor_id = myserver_id;
                      transfer_struct.seq_num = -1;
                      transfer_struct.is_commit = 0;
                      memcpy(transfer_struct.data, req, req_len);
                      int send_bytes = send(cfd, &transfer_struct, sizeof(logging_consensus), 0);
                      if(send_bytes < sizeof(logging_consensus)) 
                      {
                        perror("Couldn't send message to log transfer requestor!\n");
                      }
                    */
                    }
                }
                break;
            default:
                printf("default log replay case\n");
                break;
        }

    };

    ifs.close();
#endif
    return 0; // zac added
}
