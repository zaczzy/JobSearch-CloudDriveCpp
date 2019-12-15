#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
//#include <sys/time.h>
//#include <time.h>

#include "servercommon.h"

using namespace std;

const int BUFF_SIZE = 2048;
bool VERBOSE = false;
char *EMPTYSTR = (char *)"";
bool shutdownFlag = false;

/*
 * Function for exiting cleanly from either child or main thread
 */
void die(string msg, bool isThread){
	cerr << msg;
	if (isThread)
		pthread_exit(NULL);
	else
		exit(1);
}

/*
 * Used prior to a connect() to a server which hasn't been setup yet.
 */
void pauseBeforeConnect() {
	string beans;
	cout << "Pausing for setup of other servers..." << endl;
	cin >> beans;
}

/*
 * Accepts a connection from a client.
 */
int acceptClient(int webSock) {
	//accept shit
	struct sockaddr_in clientaddr;
	socklen_t clientaddrlen = sizeof(clientaddr);
	int clntSock = accept(webSock, (struct sockaddr*)&clientaddr, &clientaddrlen);
//	if (shutdownFlag)
//		cleanup();
	return clntSock;
}

/*
 * Read from the client
 */
string readClient(int clntSock, bool *b_break) {
	char c_requestLine[BUFF_SIZE];
	memset(c_requestLine, 0, sizeof(c_requestLine));

	if (shutdownFlag)
		die("shutdown");

	cout << "try read client" << endl;
	int i = read(clntSock, c_requestLine, sizeof(c_requestLine));
	//read() error
	if (i < -1)
//		sendStatus(400);
		die("read in loadbalancer", false);
	//client closed connection
	if (i == 0) {
		cout << c_requestLine << "====Other closed socket [" << clntSock << "]" << endl;
		*b_break = true;
		return "";
	}
	//from strtok single character delimiter, modify in-place, char * hell to string paradise
	string requestLine = c_requestLine;
	return requestLine;
}

/*
 * Create a server socket. Make it reusable and nonblocking.
 * IP eventually needed?
 */
int createServerSocket(unsigned short port){
	int servSock;
	struct sockaddr_in servAddr;

	if ((servSock = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		die("socket() failed", -1);

	//Reusable
	int enable = 1;
	if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEADDR) failed", servSock);
	if (setsockopt(servSock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEPORT) failed", servSock);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family= AF_INET;
	servAddr.sin_addr.s_addr= htons(INADDR_ANY);
	servAddr.sin_port= htons(port);
	if (bind(servSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
		die("bind() failed", servSock);
	if (listen(servSock, 10) < 0)
		die("listen() failed", servSock);

//	//Nonblocking
//	int flags = fcntl(servSock, F_GETFL, 0);
//	if (flags < 0)
//		die("ERR fcntl() failed\n", servSock);
//	if (fcntl(servSock, F_SETFL, flags | O_NONBLOCK))
//		die("ERR fcntl() 2 failed\n", servSock);

	return servSock;
}

/*
 * Create client socket. Make it reusable.
 */
int createClientSocket(unsigned short port) {
	int clntSock;
	struct sockaddr_in servAddr;

	if ((clntSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		die("socket() failed", -1);

	//Reusable
	int enable = 1;
	if (setsockopt(clntSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEADDR) failed", clntSock);
	if (setsockopt(clntSock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
		die("setsockopt(SO_REUSEPORT) failed", clntSock);

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family= AF_INET;
	servAddr.sin_addr.s_addr= inet_addr("127.0.0.1");
	servAddr.sin_port= htons(port);

	if (connect(clntSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
//		die("connect failed", clntSock);
		return -1;
	//removed 'clear welcome msg'
	return clntSock;
}
