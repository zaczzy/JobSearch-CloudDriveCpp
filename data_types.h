
#include <vector>
#include <string.h>
#include <stdint.h>

#define SERIALIZE

#ifdef SERIALIZE
#include <fstream>
#include  <iostream>
#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/binary_oarchive.hpp> 
#include <boost/archive/binary_iarchive.hpp> 
#include <boost/serialization/unordered_map.hpp> 
#include <boost/serialization/string.hpp> 
#include <boost/serialization/list.hpp> 
#endif

#define SUCCESS         0
#define FAILURE         1
#define MAX_LEN_EMAIL_BODY  256 // Will change this later
#define MAX_SIZE_FILE       2048    // WIll change this later
#define MAX_EMAILS          10      // WIll change this later

#ifdef SERIALIZE
class SerializeCStringHelper {
public:
  SerializeCStringHelper(char*& s) : s_(s) {}
  SerializeCStringHelper(const char*& s) : s_(const_cast<char*&>(s)) {}

private:

  friend class boost::serialization::access;

  template<class Archive>
  void save(Archive& ar, const unsigned version) const {
    bool isNull = (s_ == 0);
    ar & isNull;
    if (!isNull) {
      std::string s(s_);
      ar & s;
    }
  }

  template<class Archive>
  void load(Archive& ar, const unsigned version) {
    bool isNull;
    ar & isNull;
    if (!isNull) {
      std::string s;
      ar & s;
      s_ = strdup(s.c_str());
    } else {
      s_ = 0;
    }
  }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

private:
  char*& s_;
};
#endif

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
    unsigned long email_id;
    
    bool operator==(const struct email_header& header) const
    {
        if (strncmp(subject, header.subject, strlen(subject)) == 0)
        {
            if (strncmp(date, header.date, strlen(date)) == 0)
                return true;
        }

        return false;
    }
    
#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & from;
       ar & to;
       ar & subject;
       ar & date;
       ar & email_id;
   }
#endif
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

//#pragma pack(1)
//typedef struct
//{
//    char prefix[8];     // Should be "putfile"
//    char username[32];
//    char filename[256];
//    uint64_t file_len;
//    char file_content[MAX_SIZE_FILE];
//}put_file_request;

#pragma pack(1)
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
typedef struct
{
    char prefix[9]; // Should be "filelist"
    char username[32];
}get_file_list;

//#pragma pack(1)
//typedef struct
//{
//    char prefix[8];     // Should be "getfile"
//    char username[32];
//    char filename[256];
//    uint64_t file_len;
//    char file_content[MAX_SIZE_FILE];
//}get_file_response;

#pragma pack(1)
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

