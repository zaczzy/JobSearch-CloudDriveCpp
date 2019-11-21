
//#include <vector>

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
typedef struct 
{
    char from[64];
    char to[64];
    char subject[256];
    char date[64];
}email_header;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "putmail"
    char username[32];
    char email_id[64];
    email_header header;
    uint64_t email_len;
    char email_body[MAX_LEN_EMAIL_BODY];
}put_mail_request;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "getmail""
    char username[32];
    char email_id[64];
    uint64_t num_emails;
    email_header email_headers[MAX_EMAILS];
}get_mail_request;

#pragma pack(1)
typedef struct
{
    char prefix[6];     // Should be "drive"
    char username[32];
    char filename[256];
    uint64_t file_len;
    char file_content[MAX_SIZE_FILE];
}get_file_request;

