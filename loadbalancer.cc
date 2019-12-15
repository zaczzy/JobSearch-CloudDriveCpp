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
string sendCommand(int sock, char *command) {
	char buff[BUFF_SIZE];
	memset(buff, 0, sizeof(buff));
	if (VERBOSE)
		fprintf(stderr, "[%d][WEB] LB: %s\n", sock, command);
	int i = write(sock, command, strlen(command));
	//Error (closed sock)
	if (i == 0)
		return "";
	read(sock, buff, sizeof(buff));
	string result = buff;
	return result;
}

string getFirstFreeServer(vector<int> loads) {
	for (int i=0; i<loads.size(); i++)
		if (loads[i] < MAX_WEB_CLNT)
			return fe_fwdAddrs[i];
	//if here, this is bad (all servers full)
}

void redirect(int clntSock, string fwdAddr){
	//http: required (absolute URI)
	string msg = "HTTP/1.1 303 See Other\r\nLocation: http://" + fwdAddr + "/\r\n\r\n";
	char *m = (char *)msg.c_str();
	int i = write(clntSock, m, strlen(m));
	//important to close! so browser can move on
	close(clntSock);
	if (shutdownFlag || i < 0)
		die("Shutdown flag or write fail");
	if (VERBOSE)
		fprintf(stderr, "[%d][WEB] LB: %s", clntSock, m);
}

int main(int argc, char *argv[])
{
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

	char *configFile = (char*)"config_fes.txt";

	//Server socket for webclients (to redirect)
	int webSock = createServerSocket(7000);
	//Server socket for fes to connect to (for cookies)
	int cookieSock = createServerSocket(9000);
//	//Manually signal when load balancer is ready
//	pause();
	//Client sockets for fes control
	readConfig_lb(configFile);

	if (VERBOSE)
		fprintf(stderr, "Webroot: 127.0.0.1\nPort: %d\n\n", 7000);

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
			//accept shit
			struct sockaddr_in clientaddr;
			socklen_t clientaddrlen = sizeof(clientaddr);
			int clntSock = accept(webSock, (struct sockaddr*)&clientaddr, &clientaddrlen);
			socks.insert(clntSock);
			if (shutdownFlag)
				cleanup();
			if (clntSock < 0)
				continue;

			char c_requestLine[BUFF_SIZE];
			memset(c_requestLine, 0, sizeof(c_requestLine));

			if (shutdownFlag)
				die("shutdown");

			//read shit
			int i = read(clntSock, c_requestLine, sizeof(c_requestLine));
			//read() error
			if (i < -1)
//				sendStatus(400);
				die("read in loadbalancer", false);
			//client closed connection
			if (i == 0)
				break;
			if (VERBOSE)
				fprintf(stderr, "[%d][WEB] C: {%s}\n", clntSock, c_requestLine);

			//get LOAD for each fes
			//account for dead servers later
			vector<int> loads;
			for (int sock: fe_ctrlSocks) {
				string result = sendCommand(sock, "GETLOAD");
				int load = stoi(result.substr(strlen("threads=")));
				loads.push_back(load);
			}

			string fwdAddr = getFirstFreeServer(loads);

			redirect(clntSock, fwdAddr);
		}
	}
}
