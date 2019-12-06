#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sys/time.h>
#include <time.h>

#include "servercommon.h"
#include "frontendserver.h"
#include "singleconnserverhtml.h"
#include "singleconnservercontrol.h"
#include "cookierelay.h"
#include "mailservice.h"

using namespace std;

int backendSock = -1;
int loadbalancerSock = -1;
pthread_mutex_t mutex_backendSock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cookieRelay = PTHREAD_MUTEX_INITIALIZER;
set<pthread_t> webThreads;
set<pthread_t> controlThreads;

///*
// * Struct for passing request params
// */
//struct request_struct {
//    int clntSock;
//    string webroot;
//    string requestURI;
//};

/*
 * Function for exiting main thread cleanly after interrupt
 */
void cleanup(){
	for (int t: socks)
		close(socks);
	for (pthread_t t: webThreads)
		if (pthread_kill(t, SIGUSR1) < 0)
			die("pthread_kill() failed", false);
	for (pthread_t t: controlThreads)
		if (pthread_kill(t, SIGUSR1) < 0)
			die("pthread_kill() failed", false);
	std::map<string,pthread_mutex_t>::iterator i;
	for (i = locks.begin(); i!=locks.end(); ++i)
		pthread_mutex_destroy(&(i->second));
	exit(1);
}

/*
 * Function for exiting cleanly from either child or main thread
 */
void die(string msg, bool isThread=true){
	if (isThread)
		pthread_exit(NULL);
	else
		exit(1);
}

/*
 * Read from server config file
 * Format of fes config file: webserverIP:webserverPort,configserverIP:configserverPort
 */
void readConfig_fes(char *configFile, int configID, string *webIP, int *webPort, string *controlIP, int *controlPort) {
	//read config file
	ifstream infile(configFile);
	if (!infile)
		die("Can't open config file");
	int lineno = 1;
	string line;
	while (getline(infile, line)) {
		if (lineno++ != configID)
			continue;

		string webAddr;
		string controlAddr;

		//split web server and config server address
		int pos1 = line.find(",");
		if (pos2 == std::string::npos)
			die("Invalid config file");
		webAddr = line.substr(0,pos1);
		controlAddr = line.substr(pos1+1);

		//parse web server IP and port
		int pos2 = webAddr.find(":");
		if (pos2 == std::string::npos)
			die("Invalid address in config file");
		string wIP = webAddr.substr(0,pos2);
		int wPort = stoi(webAddr.substr(pos2+1));

		//parse control server IP and port
		int pos2 = controlAddr.find(":");
		if (pos2 == std::string::npos)
			die("Invalid address in config file");
		string cIP = controlAddr.substr(0,pos2);
		int cPort = stoi(controlAddr.substr(pos2+1));

		webIP = &wIP;
		webPort = &wPort;
		controlIP = &cIP;
		controlPort = &cPort;

//		struct sockaddr_in servAddr;
//		bzero(&servAddr, sizeof(servAddr));
//		servAddr.sin_family=AF_INET;
//		servAddr.sin_port=htons(forwardPort);
//		inet_pton(AF_INET, forwardIP, &(servAddr.sin_addr));
//		fAddrs->push_back(servAddr);
	}
}

/*
 * Send command to backend (ritika)
 */
string sendCommand(string command) {
	char buff[BUFF_SIZE];
	pthread_mutex_lock(&mutex_backendSock);
	write(backendSock, (char *)command.c_str(), command.length());
	read(backendSock, buff, sizeof(buff));
	pthread_mutex_unlock(&mutex_backendSock);
	if (VERBOSE)
		fprintf(stderr, "%s\n", buff);
	string result = buff;
	return result;
}

/*
 * Callback from main thread upon initialization of worker thread.
 * Initializes a SingleConnServerHTML
 */
void *webThreadFunc(void *args){
	struct thread_struct *a = (struct thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);
	string webroot = (string)(a->webroot);
	CookieRelay *CR = (CookieRelay *)(a->CR);

	SingleConnServerHTML S(clntSock, die, VERBOSE, webroot, CR);
	S.backbone();
	close(clntSock);
}

/*
 * Callback from main thread upon initialization of worker thread.
 * Initializes a control thread
 */
void *controlThreadFunc(void *args){
	struct thread_struct *a = (struct thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);

	SingleConnServerHTML S(clntSock, die, VERBOSE, &webThreads);
	S.backbone();
	close(clntSock);
}

int main(int argc, char *argv[])
{

	int c;
	int N = 0;
//	int webPort = 8000;
//	string webroot = "localhost";

	//signal handling
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = sigintHandler;
	if (sigaction(SIGINT, &action, NULL) != 0)
		die("sigaction() failed", false);

	//Handle options
	while ((c = getopt (argc, argv, "avp:")) != -1) {
		if (c == 'v') {
			VERBOSE = true;
		}
//		else if (c == 'p') {
//			servPort = stoi(optarg);
//		}
		else if (c == 'a'){
			fprintf(stderr, "*** Author: Michal Porubcin (michal19)\n");
			return 0;
		}
	}

//	//non option arguments
	if (optind >= argc)
		die("*** Author: Michal Porubcin (michal19)\n", false);
//	char *configFile = argv[optind++];
	int configID = atoi(argv[optind]); //deleted = sno = ?

	string webIP;
	int webPort;
	string controlIP;
	int controlPort;
	readConfig_fes(configFile, configID, &webIP, &webPort, &controlIP, &controlPort);

	if (VERBOSE)
		fprintf(stderr, "Webroot: %s\nPort: %d\n\n", webroot.c_str(), controlPort);

	//Server socket for webclients
	int webSock = createServerSocket(webPort);
	//Control socket for load balancer
	int controlSock = createServerSocket(controlPort);
	//Client socket for backend
	backendSock = createClientSocket(BACKEND_PORT);
	//Client socket for load balancer (for cookies??)
	loadbalancerSock = createClientSocket(LOADBALANCER_PORT);

	socks.insert(webSock);
	socks.insert(controlSock);
	socks.insert(backendSock);
	socks.insert(loadbalancerSock);

	//Relay cookies with load balancer
	CookieRelay CR;
	while (true) {
		struct pollfd fds[2];
		fds[0].fd = webSock;
		fds[0].events = POLLIN;
		fds[1].fd = controlSock;
		fds[1].events = POLLIN;
		int ret = poll(fds, 1, 500);
		//poll error
		if (ret == -1)
			die("Poll error");
		//web socket
		else if (fds[0].revents & POLLIN) { //Possibly change else if to if
			if (webThreads.size() > MAX_WEB_CLNT)
				continue;
			struct sockaddr_in clientaddr;
			socklen_t clientaddrlen = sizeof(clientaddr);
			int clntSock = accept(webSock, (struct sockaddr*)&clientaddr, &clientaddrlen);
			socks.insert(clntSock);
			if (shutdownFlag)
				cleanup();
			if (clntSock < 0)
				continue;

			struct web_thread_struct args;
			args.clntSock = clntSock;
			args.webroot = webroot;
			args.CR = &CR;

			pthread_t ntid;
			if (pthread_create(&ntid, NULL, webthreadFunc, (void *)&args) != 0)
				cleanup();
			pthread_detach(ntid);
			webThreads.insert(ntid);
		}
		//control socket
		else if (fds[1].revents & POLLIN) {
			if (controlThreads.size() > MAX_CTRL_CLNT)
				continue;
			struct sockaddr_in clientaddr;
			socklen_t clientaddrlen = sizeof(clientaddr);
			int clntSock = accept(controlSock, (struct sockaddr*)&clientaddr, &clientaddrlen);
			socks.insert(clntSock);
			if (shutdownFlag)
				cleanup();
			if (clntSock < 0)
				continue;

			struct control_thread_struct args;
			args.clntSock = clntSock;

			pthread_t ntid;
			if (pthread_create(&ntid, NULL, controlthreadFunc, (void *)&args) != 0)
				cleanup();
			pthread_detach(ntid);
			controlThreads.insert(ntid);
		}
	}

	return 0;
}
