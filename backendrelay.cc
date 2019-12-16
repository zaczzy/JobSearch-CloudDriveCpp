#include <cstring>
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
	char *c_command = (char *)command.c_str();
	char buff[BUFF_SIZE];
	write(masterSock, c_command, strlen(c_command));
	if (VERBOSE)
		fprintf(stderr, "[%d][BACK] S: %s\n", masterSock, c_command);
	read(masterSock, buff, sizeof(buff));
	if (VERBOSE)
		fprintf(stderr, "[%d][BACK] C: %s\n", masterSock, buff);
	string result = buff;
	return result;
}

int BackendRelay::getSock() {
	return masterSock;
}

