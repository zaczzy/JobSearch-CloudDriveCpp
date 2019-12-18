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
#include "backendrelay.h"

using namespace std;

set<pthread_t> webThreads;
set<pthread_t> controlThreads;
set<int> socks;

/*
 * Struct for passing multiple parameters to webThreadFunc()
 */
struct web_thread_struct {
    int clntSock;
//    int backendSock;
    string webroot;
    CookieRelay *CR;
    BackendRelay *BR;
};

/*
 * Struct for passing multiple parameters to controlThreadFunc()
 */
struct control_thread_struct {
    int clntSock;
};

/*
 * Handler for SIGINT
 */
static void sigintHandler(int signum)
{
	shutdownFlag = true;
}

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
	for (int sock: socks)
		close(sock);
	for (pthread_t t: webThreads)
		if (pthread_kill(t, SIGUSR1) < 0)
			die("pthread_kill() failed", false);
	for (pthread_t t: controlThreads)
		if (pthread_kill(t, SIGUSR1) < 0)
			die("pthread_kill() failed", false);
	std::map<string,pthread_mutex_t>::iterator i;
//	for (i = locks.begin(); i!=locks.end(); ++i)
//		pthread_mutex_destroy(&(i->second));
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

		//split web server and control server address
		unsigned int pos1 = line.find(",");
		if (pos1 == std::string::npos)
			die("Invalid config file");
		webAddr = line.substr(0,pos1);
		controlAddr = line.substr(pos1+1);

		//parse web server IP and port
		unsigned int pos2 = webAddr.find(":");
		if (pos2 == std::string::npos)
			die("Invalid address in config file");
		string wIP = webAddr.substr(0,pos2);
		int wPort = stoi(webAddr.substr(pos2+1));

		//parse control server IP and port
		unsigned int pos3 = controlAddr.find(":");
		if (pos3 == std::string::npos)
			die("Invalid address in config file");
		string cIP = controlAddr.substr(0,pos3);
		int cPort = stoi(controlAddr.substr(pos3+1));

		*webIP = wIP;
		*webPort = wPort;
		*controlIP = cIP;
		*controlPort = cPort;

//		struct sockaddr_in servAddr;
//		bzero(&servAddr, sizeof(servAddr));
//		servAddr.sin_family=AF_INET;
//		servAddr.sin_port=htons(forwardPort);
//		inet_pton(AF_INET, forwardIP, &(servAddr.sin_addr));
//		fAddrs->push_back(servAddr);
	}
}

/*
 * Asks master node for new backend server.
 */
int fetchNewBackendSock(int masterSock, string username) {
}


/*
 * Callback from main thread upon initialization of worker thread.
 * Initializes a SingleConnServerHTML
 */
void *webThreadFunc(void *args){
	struct web_thread_struct *a = (struct web_thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);
	string webroot = (string)(a->webroot);
	CookieRelay *CR = (CookieRelay *)(a->CR);
	BackendRelay *BR = (BackendRelay *)(a->BR);

	SingleConnServerHTML S(clntSock, webroot, CR, BR);
	S.backbone();
//	close(clntSock); //socket closed at bottom of backbone()
	socks.erase(clntSock);
	webThreads.erase(pthread_self());
}

/*
 * Callback from main thread upon initialization of worker thread.
 * Initializes a control thread
 */
void *controlThreadFunc(void *args){
	struct control_thread_struct *a = (struct control_thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);

	SingleConnServerControl S(clntSock, &webThreads);
	S.backbone();
	close(clntSock);
	socks.erase(clntSock);
	controlThreads.erase(pthread_self());
}

/*
 * Run command:
 * $ ./fes 1 -v
 */
int main(int argc, char *argv[])
{

	int c;

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
	char *configFile = (char *)"config_fes.txt";
	int configID = atoi(argv[optind]); //deleted = sno = ?

	string webIP;
	int webPort;
	string controlIP;
	int controlPort;
	readConfig_fes(configFile, configID, &webIP, &webPort, &controlIP, &controlPort);

	if (VERBOSE)
		fprintf(stderr, "Webroot: %s\nWebport: %d\nCtrlport: %d\n\n", webIP.c_str(), webPort, controlPort);

	//Server socket for webclients
	int webSock = createServerSocket(webPort);
	//Control socket for load balancer
	int controlSock = createServerSocket(controlPort);
	//Client socket for master
	int masterSock = createClientSocket(MASTER_PORT);
	if (VERBOSE)
		fprintf(stderr, "S: connected to [%d]! (M)\n", masterSock);
	//Manually signal when load balancer is ready
	pauseBeforeConnect();
	//Client socket for load balancer (for cookies??)
	int loadbalancerSock = createClientSocket(COOKIE_PORT);
	if (VERBOSE)
		fprintf(stderr, "S: connected to [%d]! (LB)\n", loadbalancerSock);

	socks.insert(webSock);
	socks.insert(controlSock);
//	socks.insert(backendSock);
	socks.insert(loadbalancerSock);

	//Relay messages through backend
	BackendRelay BR(masterSock);
	//Relay cookies with load balancer
	CookieRelay CR(loadbalancerSock);
	while (true) {
		struct pollfd fds[2];
		fds[0].fd = webSock;
		fds[0].events = POLLIN;
		fds[1].fd = controlSock;
		fds[1].events = POLLIN;
		int ret = poll(fds, 2, 500);
		//poll error
		if (ret == -1)
			die("Poll error", false);
		//web socket
		else if (fds[0].revents & POLLIN) { //Possibly change else if to if
			if (webThreads.size() > MAX_WEB_CLNT)
				continue;
			int clntSock = acceptClient(webSock);
			if (clntSock < 0)
				continue;

			struct web_thread_struct args;
			args.clntSock = clntSock;
			args.webroot = webIP;
			args.CR = &CR;
			args.BR = &BR; //&BR

			pthread_t ntid;
			if (pthread_create(&ntid, NULL, webThreadFunc, (void *)&args) != 0)
				cleanup();
			pthread_detach(ntid);
			webThreads.insert(ntid);
			// cout << "\n~~~~~\n" << webThreads.size() << "\n~~~~~\n" << endl;
		}
		//control socket
		else if (fds[1].revents & POLLIN) {
			if (controlThreads.size() > MAX_CTRL_CLNT)
				continue;
			int clntSock = acceptClient(controlSock);
			if (clntSock < 0)
				continue;

			struct control_thread_struct args;
			args.clntSock = clntSock;

			pthread_t ntid;
			if (pthread_create(&ntid, NULL, controlThreadFunc, (void *)&args) != 0)
				cleanup();
			pthread_detach(ntid);
			controlThreads.insert(ntid);
		}
	}

	return 0;
}
