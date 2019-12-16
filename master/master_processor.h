#ifndef master_processor_H_
#define master_processor_H_
#define NEW_CONN "[%d] New connection\r\n"
#define CLI_READ "[%d] C: %s\r\n"
#define SER_WRIT "[%d] S: %s"
#define CLS_CONN "[%d] Connection closed\r\n"
#include <string>
void process_command(std::string& command, int fd);
void process_primary(int group_id, int fd);
#endif