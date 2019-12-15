#ifndef _LOGGING_H
#define _LOGGING_H

#include "data_types.h"

class LogVoidPtrHelper;

typedef enum : int
{
    ADD_FILE,
    ADD_FOLDER,
    ADD_EMAIL,
    ADD_USER,
    CHNG_PW,
    DELETE_FILE,
    DELETE_EMAIL,
    DELETE_USER,
    DELETE_FOLDER
}op_type;

unsigned long long get_log_sequence_no();
unsigned long long get_checkpoint_version_no();
void update_log_file(char* data, unsigned long size);
int add_log_entry(op_type type, void* data);
int replay_log();

#endif
