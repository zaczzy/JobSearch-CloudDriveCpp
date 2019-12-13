#ifndef BACKENDRELAY_H
#define BACKENDRELAY_H

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
	int getSock(); //dumb

	pthread_mutex_t mutex_sock;
private:
	int masterSock;
	int loadbalancerSock;
};


#endif //BACKENDRELAY_H
