
#include <vector>

#define SUCCESS         0
#define FAILURE         1
#define MAX_LEN_EMAIL_BODY  256 // Will change this later
#define MAX_SIZE_FILE       2048    // WIll change this later
#define MAX_EMAILS          10      // WIll change this later

#pragma pack(1)
typedef struct
{
    char prefix[5];
    char username[32];
    char password[16];

}login_request;

#pragma pack(1)
struct email_header 
{
    char from[64];
    char to[64];
    char subject[256];
    char date[64];
    
    bool operator==(const struct email_header& header) const
    {
        if (strncmp(subject, header.subject, strlen(subject)) == 0)
        {
            if (strncmp(date, header.date, strlen(date)) == 0)
                return true;
        }

        return false;
    }
};

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "putmail"
    char username[32];
    //char email_id[64];
    email_header header;
    uint64_t email_len;
    char email_body[MAX_LEN_EMAIL_BODY];
}put_mail_request;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "getmail""
    char username[32];
    //char email_id[64];
}get_mail_request;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "getmail""
    char username[32];
    //char email_id[64];
    uint64_t num_emails;
    email_header email_headers[MAX_EMAILS];
}get_mail_response;

#pragma pack(1)
//typedef struct
//{
//    char prefix[8];     // Should be "putfile"
//    char username[32];
//    char filename[256];
//    uint64_t file_len;
//    char file_content[MAX_SIZE_FILE];
//}put_file_request;

typedef struct
{
    char prefix[8];     // Should be "putfile"
    char username[32];
    char filename[256];
    uint64_t file_len;
}put_file_metadata;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "getfile"
    char username[32];
    char filename[256];
}get_file_request;

#pragma pack(1)
//typedef struct
//{
//    char prefix[8];     // Should be "getfile"
//    char username[32];
//    char filename[256];
//    uint64_t file_len;
//    char file_content[MAX_SIZE_FILE];
//}get_file_response;

typedef struct
{
    char prefix[8];     // Should be "getfile"
    char username[32];
    char filename[256];
    uint64_t file_len;
}get_file_metadata;

#pragma pack(1)
typedef struct
{
    char prefix[9];     // Should be "mailbody"
    char username[32];
    unsigned long email_id;
    //char email_id[64];
    //uint64_t index;
}get_mail_body_request;


#pragma pack(1)
typedef struct
{
    char prefix[9];     // Should be "mailbody"
    char username[32];
    unsigned long email_id;
    //char email_id[64];
    uint64_t mail_body_len;
    char mail_body[MAX_LEN_EMAIL_BODY];
}get_mail_body_response;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "delmail"
    char username[32];
    unsigned long email_id;
    //char email_id[64];
    //uint64_t index;
}delete_mail_request;

