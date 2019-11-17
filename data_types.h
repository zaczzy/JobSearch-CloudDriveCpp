
#define SUCCESS         0
#define FAILURE         1

typedef struct
{
    char* prefix;
    char* username;
    char* password;

}login_request;

typedef struct
{
    char* prefix;
    char* username;
    char* email_header;
    char* email_len;
    char* email_body;

}mail_request;

typedef struct
{
    char* prefix;
    char* username;
    char* filename;
    uint64_t file_len;
    uint8_t* file_content;
}drive_request;

