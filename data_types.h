
#define SUCCESS         0
#define FAILURE         1

typedef struct
{
    char* prefix[5]
    char username[32];
    char password[16];

}login_request;

typedef struct 
{
    char from[64];
    char to[64];
    char subject[256];
    char date[64];
}email_header;

typedef struct
{
    char prefix[5];
    char username[32];
    char email_id[64];
    email_header header;
    uint64_t email_len;
}mail_request;

typedef struct
{
    char prefix[5];
    char username[32];
    char filename[256];
    uint64_t file_len;
}drive_request;

