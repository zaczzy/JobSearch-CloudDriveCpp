#ifndef _LOGGING_H
#define _LOGGING_H

#include "data_types.h"

class LogVoidPtrHelper;

typedef enum
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

#if 0
typedef struct
{
    op_type type;
    void* request;
}log_entry;
#else
typedef struct 
{ 
    char username[32];
    op_type type;
    void* metadata;
    char* payload;

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & username;
       LogVoidPtrHelper v_helper(type, metadata);
       ar & v_helper; 
       SerializeCStringHelper p_helper(payload);
       ar & p_helper;
   }
#endif
}log_entry;
#endif

typedef struct
{
  char filename[256]; 
  uint64_t file_len;

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & filename;
       ar & file_len;
   }
#endif
}log_addfile_mdata;

typedef struct
{
    unsigned long email_id;
    email_header header;
    uint64_t email_len;

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & email_id;
       ar & header;
       ar & email_len;
   }
#endif
}log_addemail_mdata;

typedef struct
{
    char password[16];

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & password;
   }
#endif
}log_adduser_mdata;

typedef struct
{
    char password[16];

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & password;
   }
#endif
}log_deluser_mdata;

typedef struct
{
    char old_password[16];
    char new_password[16];

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & old_password;
       ar & old_password;
   }
#endif
}log_chngpw_mdata;

typedef struct
{
  char filename[256]; 

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & filename;
   }
#endif
}log_delfile_mdata;

typedef struct
{
    unsigned long email_id;

#ifdef SERIALIZE
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & email_id;
   }
#endif
}log_delemail_mdata;


#ifdef SERIALIZE
class LogVoidPtrHelper {
public:
  LogVoidPtrHelper(op_type& _type, void*& _metadata) : type(_type), metadata(_metadata) {}

private:

  friend class boost::serialization::access;

  template<class Archive>
  void save(Archive& ar, const unsigned version) const {
    
      ar & type;
      switch(type)
      {
        case ADD_FILE:
            {
                log_addfile_mdata* mdata = (log_addfile_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        case ADD_EMAIL:
            {
                log_addemail_mdata* mdata = (log_addemail_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        case ADD_USER:
            {
                log_adduser_mdata* mdata = (log_adduser_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        case DELETE_FILE:
            {
                log_delfile_mdata* mdata = (log_delfile_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        case DELETE_EMAIL:
            {
                log_delemail_mdata* mdata = (log_delemail_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        case DELETE_USER:
            {
                log_deluser_mdata* mdata = (log_deluser_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        case CHNG_PW:
            {
                log_chngpw_mdata* mdata = (log_chngpw_mdata*)metadata;
                metadata = mdata;

                ar & mdata;
            }
            break;
        default:
            break;

      }
  }

  template<class Archive>
      void load(Archive& ar, const unsigned version) {
      op_type type;
      ar & type;
      switch(type)
      {
        case ADD_FILE:
            {
                log_adduser_mdata* mdata;
                ar & mdata;
                metadata = mdata;
            }

            break;
        case ADD_EMAIL:
            {
                log_addemail_mdata* mdata;
                ar & mdata;
                metadata = mdata;
            }

            break;
        case ADD_USER:
            {
                log_adduser_mdata* mdata;
                ar & mdata;
                metadata = mdata;
            }

            break;
        case DELETE_FILE:
            {
                log_delfile_mdata* mdata;
                ar & mdata;
                metadata = mdata;

            }

            break;
        case DELETE_EMAIL:
            {
                log_delemail_mdata* mdata;
                ar & mdata;
                metadata = mdata;
            }

            break;
        case DELETE_USER:
            {
                log_deluser_mdata* mdata;
                ar & mdata;
                metadata = mdata;
            }

            break;
        case CHNG_PW:
            {
                log_chngpw_mdata* mdata;
                ar & mdata;
                metadata = mdata;
            }

            break;
        default:
            break;

      }
      }

  BOOST_SERIALIZATION_SPLIT_MEMBER();

private:
  op_type& type;
  void*& metadata;
};
#endif

int log_entry_to_file(log_entry entry);
int replay_log();

#endif
