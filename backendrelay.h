#ifndef BACKENDRELAY_H
#define BACKENDRELAY_H
#include "data_types.h"

#include <pthread.h>
#include <map>
#include <string>

using namespace std;

/*
 * Gets socket and port from backend and handles connection
 * Sock from constructor is direct to backend master; other socket points to
 * real backend
 */
class BackendRelay {
 public:
  BackendRelay(int sock);
  ~BackendRelay();
  int getSock();  // dumb
  void setNewBackendSock(string username);
  string sendCommand(string &command, const string &username);
  string sendFolderRequest(const get_folder_content_request *req,
                           size_t max_resp_len, const string &username);
  void sendFileRequest(const get_file_request *req);
  bool sendChunk(const string &username, const string &directory_path,
                 const string &filename, const string &data,
                 const size_t chunk_len);
  void recvChunk(get_file_response* resp);
	bool createFolderRequest(const create_folder_request* req);
	bool removeFolderRequest(const delete_folder_content_request* req);
  bool removeFileRequest(const delete_file_request* req);
  pthread_mutex_t mutex_sock;

 private:
  int masterSock;
  int backendSock;
};

#endif  // BACKENDRELAY_H
