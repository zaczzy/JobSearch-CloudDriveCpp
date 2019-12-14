#ifndef BACKENDRELAY_H
#define BACKENDRELAY_H
#include "data_types.h"

#include <string>
#include <map>
#include <pthread.h>

using namespace std;

/*
 * Gets socket and port from backend and handles connection
 * Sock from constructor is direct to backend master; other socket points to real backend
 */
class BackendRelay {
public:
	BackendRelay(int sock);
	~BackendRelay();
	string sendCommand(string command);
	string sendFolderRequest(const get_folder_content_request* req, size_t max_resp_len);
	bool sendChunk(const string& username, const string& directory_path, const string& filename, const string& data, const size_t chunk_len);
	pthread_mutex_t mutex_sock;
private:
	int masterSock;
	int loadbalancerSock;
};


#endif //BACKENDRELAY_H
