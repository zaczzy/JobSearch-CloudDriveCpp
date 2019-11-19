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
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>			//close()
#include <arpa/inet.h>		//htons()
#include <poll.h>
#include <unistd.h>			//getopt()
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

using namespace std;

int BUFF_SIZE = 2048;
int MAX_CLNT = 100;
bool VERBOSE = false;
bool shutdownFlag = false;
map<string, pthread_mutex_t> locks;
set<pthread_t> threads;

/*
 * Struct for passing multiple parameters to threadFunc()
 */
struct thread_struct {
    int clntSock;
    string webroot;
};

/*
 * Struct for passing request params
 */
struct request_struct {
    int clntSock;
    string webroot;
    string requestURI;
};

/*
 * Function for exiting cleanly
 */
void die(const char *msg, int sock, bool isThread){
	if (sock > 0)
		close(sock);
	fprintf(stderr, "%s", msg);
	if (isThread) {
		threads.erase(pthread_self());
		pthread_exit(NULL);
	}
	exit(1);
}

/*
 * Function for exiting main thread cleanly after interrupt
 */
void cleanup(int sock, int sock2){
	if (sock > 0)
		close(sock);
	if (sock2 > 0)
		close(sock2);
	for (pthread_t t: threads)
		if (pthread_kill(t, SIGUSR1) < 0)
			die("pthread_kill() failed", -1);
	std::map<string,pthread_mutex_t>::iterator i;
	for (i = locks.begin(); i!=locks.end(); ++i)
		pthread_mutex_destroy(&(i->second));
	exit(1);
}

/*
 * Create a server socket. Make it reusable and nonblocking.
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

	//Nonblocking
	int flags = fcntl(servSock, F_GETFL, 0);
	if (flags < 0)
		die("ERR fcntl() failed\n", servSock);
	if (fcntl(servSock, F_SETFL, flags | O_NONBLOCK))
		die("ERR fcntl() 2 failed\n", servSock);

	return servSock;
}

class SingleConnServerHTML {
public:
	SingleConnServerHTML();
	~SingleConnServerHTML();
	void mainloop();
private:
	int sendMsg(int sock, string msg);
	void sendStatus(int statusCode);
	int handleGET(struct request_struct* r);
	int handlePOST(struct request_struct* r);
	int handleRequest(struct request_struct* r, string req);

	int sock;
	string webroot;
	string requestURI;
	bool loggedIn;
	set<string> commands;
	map<int, string> status2reason;
};

SingleConnServerHTML::SingleConnServerHTML(int sock, string webroot):
	sock(sock), webroot(webroot) {
	requestURI = "";
	loggedIn = False;
	commands = {
		"GET",
		"POST"
	};
	status2reason = {
			{200, "OK"},
			{201, "Created"},
			{202, "Accepted"},
			{204, "No Content"},
			{205, "Reset Content"},
			{206, "Partial Content"},
			{300, "Multiple Choices"},
			{301, "Moved Permanently"},
			{304, "Not Modified"},
			{307, "Temporary Redirect"},
			{308, "Permanent Redirect"},
			{400, "Bad Request"},
			{401, "Unauthorized"},
			{402, "Payment Required lol"},
			{403, "Forbidden"},
			{404, "Not Found"},
			{429, "Too Many Requests"},
			{451, "Unavailable For Legal Reasons"},
			{500, "Internal Server Error"},
			{501, "Not Implemented"},
			{502, "Bad Gateway"},
			{503, "Service Unavailable"},
			{504, "Gateway Timeout"},
			{505, "HTTP Version Not Supported"}
	};
}

SingleConnServerHTML()::~SingleConnServerHTML() {

}

/*
 * Send a message to the client over provided socket.
 */
int SingleConnServerHTML::sendMsg(string msg) {
	char *m = (char *)msg.c_str();
	int i = write(sock, m, strlen(m));
	if (shutdownFlag || i < 0)
		die("Shutdown flag", sock, true);
	if (VERBOSE)
		fprintf(stderr, "[%d] S: %s", sock, m);
	return 1;
}

void SingleConnServerHTML::sendStatus(int statusCode) {
	//get corresponding reason in words
	string reason = status2reason[statusCode];
	//status line
	string statusLine = "HTTP/1.0" + toString(statusCode) + reason + "\r\n";
	//no headers
	statusLine += "\r\n";
	//format in HTML if not 200
	if (statusCode != 200) {
		string body =
				"<html><body><h1>" +
				statusCode + " " + reason +
				"</h1></body></html>";
		statusLine += body;
	}

	sendMsg(statusLine);
}

/*
 * Read in an html file.
 */
string SingleConnServerHTML::readHTMLFromFile(string fname) {
	string HTML = "";

	//read config file
	ifstream infile(fname);
	if (!infile)
		die("Can't open config file", -1);
	string line;
	while (getline(infile, line)) {
		HTML += line;
	}

	return HTML;
}

/*
 * Handle GET request
 */
void SingleConnServerHTML::handleGET() {
	if (!loggedIn) {
		sendStatus(200);
		//read from templates/login.html
		string HTML = readHTMLFromFile("login.html");
		//serve login page
		sendMsg(HTML);
		return;
	}

	if (requestURI.compare("index.html") == 0 || requestURI.compare("") == 0) {
		sendStatus(200);
		if (requestURI.compare("") == 0)
			requestURI = "/";
		//read from templates/index.html
		string HTML = readHTMLFromFile("index.html");
		//serve landing page
		sendMsg(HTML);
	}
	else if (requestURI.compare("mail.html") == 0) {
		sendStatus(200);
		//read from templates/mail.html
		string HTML = readHTMLFromFile("mail.html");
		//serve mail page
		sendMsg(HTML);
	}
//	else if (requestURI.compare("storage.html") == 0) {
//		sendStatus(200);
//		//read from templates/storage.html
//		string HTML = readHTMLFromFile("storage.html");
//		//serve storage page
//		sendMsg(HTML);
//	}
	else {
		sendStatus(400);
	}
}

void SingleConnServerHTML::handlePOST() {
	//YOU WERE HERE
	//parse login data e.g. user=michal&pass=porubcin
	//for login POST, check against hardcoded user/pass
}

void SingleConnServerHTML::mainloop() {
	FILE *clntFp = fdopen(sock, "r");
	if (clntFp == NULL)
		die("fdopen error", sock, true);

	sendMsg("+OK server ready [localhost]\r\n");
	char requestLine[BUFF_SIZE];
	while (true) {
		memset(requestLine, 0, sizeof(requestLine));

		struct pollfd fds[1];
		fds[0].fd = sock;
		fds[0].events = POLLIN;
		int ret = poll(fds, 1, 500);
		//poll error
		if (ret == -1)
			die("poll error", clntSock, true);
		//retry on timeout and empty buffer (no prev. commands)
		if (!ret)
			continue;
		//clntSock is readable
		if (fds[0].revents & POLLIN)
			i = read(clntSock, requestLine, sizeof(requestLine)-1);

		if (shutdownFlag)
			die("shutdown", clntSock, true);

		if (fgets(requestLine, sizeof(requestLine, clntFp) == NULL)) {
			sendStatus(400);
		}

		char *delim = " ";
		char *c_req = strtok(requestLine, delim);
		char *reqURI = strtok(NULL, delim);
		char *httpVersion = strtok(NULL, delim);
		char *extraGarbage = strtok(NULL, delim);

		requestURI = reqURI;
		string req = c_req;

		//check valid request
		if (commands.find(req) == commands.end()){
			sendStatus(500);
			continue;
		}

		//check first character in URI is "/"
		if (requestURI[0] != "/") {
			sendStatus(400);
			continue;
		}

		//skip remaining headers
		while (true) {
			if (fgets(requestLine, sizeof(requestLine, clntFp) == NULL))
				sendStatus(400);
			if (strcmp(line, "\r\n") == 0)
				break;
		}
		memset(requestLine, 0, sizeof(requestLine));

		if (VERBOSE)
			fprintf(stderr, "[%d] C: %s %s\n", clntSock, (char *)req.c_str());

		if (req.compare("GET") == 0) {
			handleGET();
		}
		else if (req.compare("POST") == 0) {
			if (fgets(requestLine, sizeof(requestLine, clntFp) == NULL))
				sendStatus(400);
			string body = requestLine;
			handlePOST(body);
		}
		else {
			sendStatus(501);
		}

	}
}

/*
 * Callback from main thread upon initialization of worker thread.
 * Reads HTTP request from the client into a buffer and processes it with handleRequest()
 * if a valid request is found.
 */
void *threadFunc(void *args){
	struct thread_struct *a = (struct thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);
	string webroot = (string)(a->webroot);

	SingleConnServerHTML S(clntSock, webroot);
	S.mainloop();
}

int main(int argc, char *argv[])
{
	int c;
	int N = 0;
	int port = 11000;
	string webroot;
	string buff;

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	sigemptyset(&action.sa_mask);
	action.sa_handler = sigintHandler;
	if (sigaction(SIGINT, &action, NULL) != 0)
		die("sigaction() failed", -1, false);

	//Handle options
	while ((c = getopt (argc, argv, "avp:")) != -1) {
		if (c == 'v') {
			VERBOSE = true;
		}
		else if (c == 'p') {
			port = stoi(optarg);
		}
		else if (c == 'a'){
			fprintf(stderr, "*** Author: Michal Porubcin (michal19)\n");
			return 0;
		}
	}

	//non option arguments
	if (optind >= argc)
		die("*** Author: Michal Porubcin (michal19)\n", -1, false);
	webroot = argv[optind];

	//Start serving
	int servSock = createServerSocket(port);
	while (true) {
		if (threads.size() > MAX_CLNT)
			continue;
		struct sockaddr_in clientaddr;
		socklen_t clientaddrlen = sizeof(clientaddr);
		int clntSock = accept(servSock, (struct sockaddr*)&clientaddr, &clientaddrlen);
		if (shutdownFlag)
			cleanup(servSock, clntSock);
		if (clntSock < 0)
			continue;

		struct thread_struct args;
		args.clntSock = clntSock;
		args.webroot = webroot;

		pthread_t ntid;
		if (pthread_create(&ntid, NULL, threadFunc, (void *)&args) != 0)
			cleanup(servSock, clntSock);
		pthread_detach(ntid);
		threads.insert(ntid);
	}

	return 0;
}
