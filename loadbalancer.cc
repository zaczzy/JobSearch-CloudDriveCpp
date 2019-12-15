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
set<pthread_t> cookieThreads;
set<int> socks; //for cleanup
//Cookie storage here

int latestCookie = 0;
map<int, string> cookie2Browser;

/*
 * Struct for passing multiple parameters to cookieThreadFunc()
 */
struct cookie_thread_struct {
    int clntSock;
};

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

		int fe_ctrlSock = createClientSocket(cPort);
		if (fe_ctrlSock < 0)
			continue;
		fe_ctrlSocks.push_back(fe_ctrlSock);
		fe_fwdAddrs.push_back(webAddr);

		if (VERBOSE)
			fprintf(stderr, "LB: connected to [%d] (FES)!\n", fe_ctrlSock);
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

void *cookieThreadFunc(void *args) {
	struct cookie_thread_struct *a = (struct cookie_thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);

	if (VERBOSE)
		fprintf(stderr, "[%d][COOK] LB: +OK server ready [localhost]\r\n", clntSock);

	while (true) {
		//read shit
		bool b_break = false;
		string requestLine = readClient(clntSock, &b_break);
		if (b_break)
			break;

		if (VERBOSE)
			fprintf(stderr, "[%d][COOK] FES: {%s}\n", clntSock, (char *)requestLine.c_str());

		//parse command
		int i_sep = requestLine.find(" ");
		string command = requestLine.substr(0,i_sep);
		string payload = requestLine.substr(i_sep+1);
		if (command.compare("GEN") == 0) {
			//payload == browser
			cookie2Browser[++latestCookie] = payload;
			string msg = to_string(latestCookie);
			char *m = (char *)msg.c_str();
			int i = write(clntSock, m, strlen(m));
			if (VERBOSE)
				fprintf(stderr, "[%d][COOK] LB: {%s}\n", clntSock, m);
			//check i == 0
		}
		else if (command.compare("FETCH") == 0) {
			//payload == cookie
			int cookie = stoi(payload);
			string msg = "name=";
			if (cookie2Browser.find(cookie) != cookie2Browser.end())
				msg += cookie2Browser[cookie];
			char *m = (char *)msg.c_str();
			int i = write(clntSock, m, strlen(m));
			if (VERBOSE)
				fprintf(stderr, "[%d][COOK] LB: {%s}\n", clntSock, m);
		}
		else if (command.compare("DEL") == 0) {
			//payload == cookie
			int cookie = stoi(payload);
			cookie2Browser.erase(cookie);
			string msg = "ok";
			char *m = (char *)msg.c_str();
			int i = write(clntSock, m, strlen(m));
			if (VERBOSE)
				fprintf(stderr, "[%d][COOK] LB: {%s}\n", clntSock, m);
		}
		else {
			die("bad command");
		}
	}
//	SingleConnServerCookie S(clntSock);
//	S.backbone();
	socks.erase(clntSock);
	close(clntSock);
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

	if (VERBOSE)
		fprintf(stderr, "Webroot: 127.0.0.1\nPort: %d\n\n", WEB_PORT);

	char *configFile = (char*)"config_fes.txt";

	//Server socket for webclients (to redirect)
	int webSock = createServerSocket(WEB_PORT);
	//Server socket for fes to connect to (for cookies)
	int cookieSock = createServerSocket(COOKIE_PORT);
//	//Manually signal when load balancer is ready
//	pauseBeforeConnect();
	//Client sockets for fes control
	readConfig_lb(configFile);

	while (true) {
		//eventually need to listen on 2 fds because of cookie server
		struct pollfd fds[2];
		fds[0].fd = webSock;
		fds[0].events = POLLIN;
		fds[1].fd = cookieSock;
		fds[1].events = POLLIN;
		int ret = poll(fds, 2, 500);
		//poll error
		if (ret == -1)
			die("Poll error", false);
		//web socket
		else if (fds[0].revents & POLLIN) {
			cout << "web" << endl;
			//accept shit
			int clntSock = acceptClient(webSock);
			if (clntSock < 0)
				continue;
			socks.insert(clntSock);
			//read shit
			bool b_break = false;
			string requestLine = readClient(clntSock, &b_break);
			if (b_break) {
				close(clntSock);
				socks.erase(clntSock);
				continue;
			}

			if (VERBOSE)
				fprintf(stderr, "[%d][WEB] C: {%s}\n", clntSock, (char *)requestLine.c_str());

			//get LOAD for each fes
			//account for dead servers later
			vector<int> loads;
			vector<int> updatedSocks;
			vector<string> updatedAddrs;
			int z = 0;
			for (int sock: fe_ctrlSocks) {
				string result = sendCommand(sock, "GETLOAD");
				if (result.empty()) {
					close(sock);
					socks.erase(sock);
				}
				else {
					int load = stoi(result.substr(strlen("threads=")));
					loads.push_back(load);
					updatedSocks.push_back(sock);
					updatedAddrs.push_back(fe_fwdAddrs[z]);
				}
				z++;
			}
			fe_ctrlSocks = updatedSocks;
			fe_fwdAddrs = updatedAddrs;

			string fwdAddr = getFirstFreeServer(loads);

			redirect(clntSock, fwdAddr);
			//*socket closed*
		}
		//cookie socket
		else if (fds[1].revents & POLLIN) {
			cout << "cookie" << endl;
//			if (controlThreads.size() > MAX_CTRL_CLNT)
//				continue;
			//accept shit
			int clntSock = acceptClient(cookieSock);
			if (clntSock < 0)
				continue;
			socks.insert(clntSock);

//			int clntSock = acceptClient(controlSock);
//			if (clntSock < 0)
//				continue;

			struct cookie_thread_struct args;
			args.clntSock = clntSock;

			pthread_t ntid;
			if (pthread_create(&ntid, NULL, cookieThreadFunc, (void *)&args) != 0)
				cleanup();
			pthread_detach(ntid);
			cookieThreads.insert(ntid);
		}
	}
}
