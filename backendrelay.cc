#include "servercommon.h"
#include "backendrelay.h"

/*
 * Constructor
 */
BackendRelay::BackendRelay(int sock): masterSock(sock) {
	loadbalancerSock = -1;
	mutex_sock = PTHREAD_MUTEX_INITIALIZER;
}

/*
 * Destructor
 */
BackendRelay::~BackendRelay() {

}

/*
 * Send command to backend (ritika)
 */
string BackendRelay::sendCommand(string command) {
	char buff[BUFF_SIZE];
	write(masterSock, (char *)command.c_str(), command.length());
	read(masterSock, buff, sizeof(buff));
//	if (VERBOSE)
//		fprintf(stderr, "%s\n", buff);
	string result = buff;
	return result;
}

