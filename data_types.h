#ifndef A72BB918_8F17_44D2_84FD_B8711EA9776E
#define A72BB918_8F17_44D2_84FD_B8711EA9776E

#include <cstdint>
#include <cstring>
#include <vector>
#define SUCCESS 0
#define FAILURE 1
#define MAX_LEN_EMAIL_BODY 256  // Will change this later
#define CHUNK_SIZE 8192
#define MAX_EMAILS 10           // WIll change this later

#pragma pack(1)
typedef struct {
  char prefix[5];
  char username[32];
  char password[16];

} login_request;

#pragma pack(1)
struct email_header {
  char from[64];
  char to[64];
  char subject[256];
  char date[64];

  bool operator==(const struct email_header& header) const {
    if (strncmp(subject, header.subject, strlen(subject)) == 0) {
      if (strncmp(date, header.date, strlen(date)) == 0) return true;
    }

    return false;
  }
};

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "putmail"
    char username[32];
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
    //unsigned long email_id;
    uint64_t num_emails;
    email_header email_headers[MAX_EMAILS];
}get_mail_response;

#pragma pack(1)
typedef struct
{
    char prefix[9];     // Should be "mailbody"
    char username[32];
    unsigned long email_id;
}get_mail_body_request;


#pragma pack(1)
typedef struct
{
    char prefix[9];     // Should be "mailbody"
    char username[32];
    unsigned long email_id;
    uint64_t mail_body_len;
    char mail_body[MAX_LEN_EMAIL_BODY];
}get_mail_body_response;

#pragma pack(1)
typedef struct
{
    char prefix[8];     // Should be "delmail"
    char username[32];
    unsigned long email_id;
}delete_mail_request;

// DELETE file from directory
#pragma pack(1)
typedef struct {
  char prefix[8];  // Should be "delfile"
  char username[32];
  char directory_path[1024];
  char filename[256];
} delete_file_request;

// PUT file to directory
#pragma pack(1)
typedef struct {
  char prefix[8];  // Should be "putfile"
  char username[32];
  char directory_path[1024];
  char filename[256];
  uint64_t chunk_len;
  char data[CHUNK_SIZE];
  bool has_more; // TODO: inform ritika
} put_file_request;

// GET file from directory
#pragma pack(1)
typedef struct {
  char prefix[8];  // Should be "getfile"
  char username[32];
  char directory_path[1024]; // "r00t"
  char filename[256];
} get_file_request;

// GET RESPONSE
#pragma pack(1)
typedef struct {
  char chunk[CHUNK_SIZE];
  bool has_more;
  size_t f_len;
} get_file_response;

// CREATE folder
#pragma pack(1)
typedef struct {
  char prefix[9];  // Should be "mkfolder"
  char username[32];
  char directory_path[1024]; // "r00t"
  char folder_name[256]; // "test"
} create_folder_request; //TODO: ask ritika what's the return value

// GET folder contents
#pragma pack(1)
typedef struct {
  char prefix[10];  // Should be "getfolder"
  char username[32];
  char directory_path[1024]; // "r00t"
  char folder_name[256]; // "test"
} get_folder_content_request;

// RESPONSE be like "Ffile1~Ddir1~Ffile2~~"

// DELETE folder request
#pragma pack(1)
typedef struct {
  char prefix[10];  // Should be "delfolder"
  char username[32];
  char directory_path[1024]; // "r00t"
  char folder_name[256]; // "test"
} delete_folder_content_request;
#endif /* A72BB918_8F17_44D2_84FD_B8711EA9776E */
