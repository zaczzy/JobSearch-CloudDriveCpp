#include "backendrelay.h"
#include <cstring>
#include "servercommon.h"
/*
 * Constructor
 */
BackendRelay::BackendRelay(int sock) : masterSock(sock) {
  loadbalancerSock = -1;
  mutex_sock = PTHREAD_MUTEX_INITIALIZER;
}

/*
 * Destructor
 */
BackendRelay::~BackendRelay() {}

/*
 * Send command to backend (ritika)
 */
string BackendRelay::sendCommand(string command) {
  char* c_command = (char*)command.c_str();
  char buff[BUFF_SIZE];
  write(masterSock, c_command, strlen(c_command));
  if (VERBOSE) fprintf(stderr, "[%d][BACK] S: %s\n", masterSock, c_command);
  read(masterSock, buff, sizeof(buff));
  if (VERBOSE) fprintf(stderr, "[%d][BACK] C: %s\n", masterSock, buff);
  string result = buff;
  return result;
}

int BackendRelay::getSock() { return masterSock; }

// RESPONSE be like "Ffile1~Ddir1~Ffile2~" or "~" when empty
string BackendRelay::sendFolderRequest(const get_folder_content_request* req,
                                       size_t max_resp_len) {
  write(masterSock, req, sizeof(*req));
  char* buff = new char[max_resp_len];
  int buff_size = read(masterSock, buff, max_resp_len);
  string result;
  if ((size_t)buff_size < max_resp_len) {
    buff[buff_size] = '\0';
    result = buff;
    if (buff_size == 1 && buff[0] == '~') {
      return "";
    } else if (buff[buff_size - 1] != '~') {
      cerr<< "getFolderContentRequest response malformated" << endl;
      cerr << "|" << result << "|" << endl;
      result = "";
    }
    printf("folder content: %s\n", buff);
  } else {
    fprintf(stderr, "sendFolderRequest response overflow\n");
    result = "";
  }
  delete[] buff;
  return result;
}
void BackendRelay::sendFileRequest(const get_file_request* req) {
  write(masterSock, req, sizeof(*req));
}
bool BackendRelay::sendChunk(const string& username,
                             const string& directory_path,
                             const string& filename, const string& data,
                             const size_t chunk_len) {
  put_file_request req;
  memset(&req, 0, sizeof(req));
  strcpy(req.prefix, "putfile");
  strcpy(req.username, username.c_str());
  strcpy(req.directory_path, directory_path.c_str());
  strcpy(req.filename, filename.c_str());
  memcpy(req.data, data.c_str(), chunk_len);
  req.chunk_len = (uint64_t)chunk_len;
  cout << "UN: " << username << " DP: " << directory_path << " FN: " << filename
       << " DATA: <" << data << ">CLEN: " << (uint64_t)chunk_len << endl;
  printf("%lu\n", sizeof(put_file_request));
  printf("%lu\n", sizeof(req));
  write(masterSock, &req, sizeof(req));
  char buff[50];
  int resp_size = read(masterSock, &buff, 50);
  buff[resp_size] = '\0';
  if (strncmp(buff, "+OK", 3) == 0) {
    return true;
  }
  fprintf(stderr, "something wrong with sendChunk\n");
  return false;
}
// TODO: ask ritika about confirm
bool BackendRelay::createFolderRequest(const create_folder_request* req) {
  write(masterSock, req, sizeof(*req));
  char* confirm = new char[1024];
  int rlen = read(masterSock, confirm, 1024);
  confirm[rlen] = '\0';
  printf("mkfolder confirm: %s\n", confirm);
  // bool retval = strncmp(confirm, "+OK", 3);
  delete[] confirm;
  return true;
}
// TODO: ask ritika about confirm
bool BackendRelay::removeFolderRequest(
    const delete_folder_content_request* req) {
  write(masterSock, req, sizeof(*req));
  char* confirm = new char[1024];
  int rlen = read(masterSock, confirm, 1024);
  confirm[rlen] = '\0';
  printf("rm folder confirm: %s\n", confirm);
  // bool retval = strncmp(confirm, "+OK", 3) == 0;
  delete[] confirm;
  return true;
}
bool BackendRelay::removeFileRequest(const delete_file_request* req) {
  write(masterSock, req, sizeof(*req));
  char* confirm = new char[1024];
  int rlen = read(masterSock, confirm, 1024);
  confirm[rlen] = '\0';
  printf("rm file confirm: %s\n", confirm);
  // bool retval = strncmp(confirm, "+OK", 3) == 0;
  delete[] confirm;
  return true;
}
void BackendRelay::recvChunk(get_file_response* resp) {
  read(masterSock, resp, sizeof(*resp));
};
