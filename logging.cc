#include "logging.h"

#include <stdlib.h>
#include <fstream>

#define LOG_FILE    "key_value.log"

log_entry* prepare_log_entry(char* username, op_type type, void* metadata, char* data)
{
    log_entry* entry = (log_entry*)malloc(sizeof(log_entry) * sizeof(char));

    strncpy(entry->username, username, strlen(username));
    entry->type = type;

    switch(type)
    {
        case ADD_FILE:
            {
                log_addfile_mdata* mdata = (log_addfile_mdata*)metadata;
                entry->metadata = mdata;
                if (data != NULL)
                {
                    uint64_t file_len = mdata->file_len;
                    entry->payload = (char*)malloc(file_len * sizeof(char));
                    strncpy(entry->payload, data, file_len);
                }
                else
                {
                    // TODO: Report error
                }
            }
            break;
        case ADD_EMAIL:
            {
                log_addemail_mdata* mdata = (log_addemail_mdata*)metadata;
                entry->metadata = mdata;
                if (data != NULL)
                {
                    uint64_t email_len = mdata->email_len;
                    entry->payload = (char*)malloc(email_len * sizeof(char));
                    strncpy(entry->payload, data, email_len);
                }
                else
                {
                    // TODO: Report error]
                }
            }
            break;
        case ADD_USER:
            {
                log_adduser_mdata* mdata = (log_adduser_mdata*)metadata;
                entry->metadata = mdata;
                entry->payload = NULL;
            }
            break;
        case DELETE_FILE:
            {
                log_delfile_mdata* mdata = (log_delfile_mdata*)metadata;
                entry->metadata = mdata;
                entry->payload = NULL;
            }
            break;
        case DELETE_EMAIL:
            {
                log_delemail_mdata* mdata = (log_delemail_mdata*)metadata;
                entry->metadata = mdata;
                entry->payload = NULL;
            }
            break;
        case DELETE_USER:
            {
                log_deluser_mdata* mdata = (log_deluser_mdata*)metadata;
                entry->metadata = mdata;
                entry->payload = NULL;
            }
            break;
        case CHNG_PW:
            {
                log_chngpw_mdata* mdata = (log_chngpw_mdata*)metadata;
                entry->metadata = mdata;
                entry->payload = NULL;
            }
            break;
        default:
            break;
    }

    return entry;
}

#ifdef SERIALIZE
int log_entry_to_file(log_entry entry)
{
    /** Serialize entry and write to file */
    std::ofstream ofs(LOG_FILE);
    boost::archive::text_oarchive oa(ofs);
    oa << entry;
    ofs.close();
}
#endif

int replay_log()
{

}
