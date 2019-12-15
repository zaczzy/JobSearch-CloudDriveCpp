#include "backendrelay.h"
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
  char buff[BUFF_SIZE];
  write(masterSock, (char*)command.c_str(), command.length());
  read(masterSock, buff, sizeof(buff));
  //	if (VERBOSE)
  //		fprintf(stderr, "%s\n", buff);
  string result = buff;
  return result;
}
// TODO: next version, implement repeated reading until "~~" encountered
// RESPONSE be like "Ffile1~Ddir1~Ffile2~~"
string BackendRelay::sendFolderRequest(const get_folder_content_request* req,
                                       size_t max_resp_len) {
  write(masterSock, req, sizeof(*req));
  char* buff = new char[max_resp_len];
  int buff_size = read(masterSock, buff, max_resp_len);
  string result;
  if ((size_t)buff_size < max_resp_len) {
    buff[buff_size] = 0;
    result = buff;
    if (buff[buff_size - 1] != '~' || buff[buff_size - 2] != '~') {
      fprintf(stderr, "sendFolderRequest response malformated\n");
      result = "";
    }
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
bool BackendRelay::sendChunk(const string& username, const string& directory_path,
               const string& filename, const string& data,
               const size_t chunk_len, bool has_more) {
  put_file_request req;
  memset(&req, 0, sizeof(req));
	strcpy(req.prefix, "putfile");
	strcpy(req.username, username.c_str());
	strcpy(req.directory_path, directory_path.c_str());
	strcpy(req.filename, filename.c_str());
	memcpy(req.data, data.c_str(), chunk_len);
	req.chunk_len = (uint64_t) chunk_len;
  req.has_more = has_more;
	write(masterSock, &req, sizeof(req));
  char buff[10];
  int resp_size = read(masterSock, &buff, 10);
  buff[resp_size] = 0;
  if (strcmp(buff, "+OK") == 0) {
    return true;
  }
  fprintf(stderr, "something wrong with sendChunk\n");
	return false;
}
// TODO: ask ritika about confirm
bool BackendRelay::createFolderRequest(const create_folder_request* req) {
  write(masterSock, req, sizeof(*req));
  char * confirm = new char[10];
  int rlen = read(masterSock, confirm, 10);
  confirm[rlen]=0;
  bool retval = strncmp(confirm, "+OK", 3);
  delete[] confirm;
  return retval;
}
// TODO: ask ritika about confirm
bool BackendRelay::removeFolderRequest(const delete_folder_content_request* req) {
  write(masterSock, req, sizeof(*req));
  char * confirm = new char[10];
  int rlen = read(masterSock, confirm, 10);
  confirm[rlen]=0;
  bool retval = strncmp(confirm, "+OK", 3);
  delete[] confirm;
  return retval;
}
void BackendRelay::recvChunk(get_file_response* resp) {
  read(masterSock, resp, sizeof(*resp));
};