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
#include "loadbalancer.h"

using namespace std;

vector<int> fe_ctrlSocks;
vector<string> fe_fwdAddrs;
set<int> socks; //for cleanup
//Cookie storage here

/*
 * Function for exiting main thread cleanly after interrupt
 */
void cleanup(){
	for (int sock: socks)
		close(sock);
	std::map<string,pthread_mutex_t>::iterator i;
//	for (i = locks.begin(); i!=locks.end(); ++i)
//		pthread_mutex_destroy(&(i->second));
	exit(1);
}

/*
 * Read config file as a load balancer. Fills vector of control sockets and vector of redirect URLs
 */
void readConfig_lb(char *configFile) {
	ifstream infile(configFile);
	if (!infile)
		die("Can't open config file");
	int lineno = 0;
	string line;
	while (getline(infile, line)) {
		string webAddr;
		string controlAddr;

		//split web server and control server address
		unsigned int pos1 = line.find(",");
		if (pos1 == std::string::npos)
			die("Invalid config file");
		webAddr = line.substr(0,pos1);
		controlAddr = line.substr(pos1+1);

		//parse control server IP and port
		unsigned int pos3 = controlAddr.find(":");
		if (pos3 == std::string::npos)
			die("Invalid address in config file");
		string cIP = controlAddr.substr(0,pos3);
		int cPort = stoi(controlAddr.substr(pos3+1));

		fe_ctrlSocks.push_back(createClientSocket(cPort));
		fe_fwdAddrs.push_back(webAddr);
	}
}

/*
 * Send a message to the client over provided socket.
 */
string sendCommand(int sock, string command) {
	char buff[BUFF_SIZE];
	write(sock, (char *)command.c_str(), command.length());
	read(sock, buff, sizeof(buff));
//	if (VERBOSE)
//		fprintf(stderr, "%s\n", buff);
	string result = buff;
	return result;
}

string getFirstFreeServer(vector<int> *loads) {
	for (int i=0; i<loads->size(); i++)
		if ((*loads)[i] != MAX_WEB_CLNT)
			return fe_fwdAddrs[i];
	//if here, this is bad (all servers full)
}

void redirect(int clntSock, string fwdAddr){
	string msg = "HTTP/1.1 303 See Other\r\nLocation: " + fwdAddr;
	char *m = (char *)msg.c_str();
	int i = write(clntSock, m, strlen(m));
	if (shutdownFlag || i < 0)
		die("Shutdown flag or write fail");
	if (VERBOSE)
		fprintf(stderr, "[%d][WEB] LB: %s", clntSock, m);
}

int main(int argc, char *argv[])
{
	char *configFile;
	int configID;

	//Handle options
	int c;
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

	//non option arguments
	if (optind >= argc)
		die("*** Author: Michal Porubcin (michal19)\n", -1);
	configID = atoi(argv[optind]);

	//need to connect to each config server
	configFile = (char*)"config_fes.txt";

	//Create socket for fes control
	readConfig_lb(configFile);

	//Server socket for webclients (to redirect)
	int webSock = createServerSocket(LOADBALANCER_PORT);

	while (true) {
		//eventually need to listen on 2 fds because of cookie server
		struct pollfd fds[1];
		fds[0].fd = webSock;
		fds[0].events = POLLIN;
		int ret = poll(fds, 1, 500);
		//poll error
		if (ret == -1)
			die("Poll error", false);
		//web socket
		else if (fds[0].revents & POLLIN) {
			struct sockaddr_in clientaddr;
			socklen_t clientaddrlen = sizeof(clientaddr);
			int clntSock = accept(webSock, (struct sockaddr*)&clientaddr, &clientaddrlen);
			socks.insert(clntSock);
			if (shutdownFlag)
				cleanup();
			if (clntSock < 0)
				continue;

			//account for dead servers later
			vector<int> loads;
			for (int sock: fe_ctrlSocks) {
				int load = stoi(sendCommand(sock, "GETLOAD"));
				loads.push_back(load);
			}

			string fwdAddr = getFirstFreeServer(&loads);

			redirect(clntSock, fwdAddr);
		}
	}
}
