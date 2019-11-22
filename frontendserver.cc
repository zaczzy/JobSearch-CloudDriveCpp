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
#include <fcntl.h>			//fcntl()
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
int BACKEND_PORT = 5000;
bool VERBOSE = false;
bool shutdownFlag = false;
int backendSock = -1;
pthread_mutex_t mutex_backendSock = PTHREAD_MUTEX_INITIALIZER;
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
 * Handler for SIGINT
 */
static void sigintHandler(int signum)
{
	shutdownFlag = true;
}

/*
 * Function for exiting cleanly
 */
void die(string msg, int sock, bool isThread=true){
	if (sock > 0)
		close(sock);
	cerr << msg;
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

/*
 * Create client socket. Make it reusable.
 */
int createClientSocket(unsigned short port) {
	int sockfd, connfd;
	struct sockaddr_in servaddr, cli;

	// socket create and varification
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		printf("socket creation failed...\n");
		exit(0);
	}
	else
		printf("Socket successfully created..\n");
	bzero(&servaddr, sizeof(servaddr));

	// assign IP, PORT
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	servaddr.sin_port = htons(PORT);

	// connect the client socket to server socket
	if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
		printf("connection with the server failed...\n");
		exit(0);
	}
	else
		printf("connected to the server..\n");


//	int clntSock;
//	struct sockaddr_in servAddr;
//
//	if ((clntSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
//		die("socket() failed", -1);
//
//	//Reusable
//	int enable = 1;
//	if (setsockopt(clntSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
//		die("setsockopt(SO_REUSEADDR) failed", clntSock);
//	if (setsockopt(clntSock, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(int)) < 0)
//		die("setsockopt(SO_REUSEPORT) failed", clntSock);
//
//	memset(&servAddr, 0, sizeof(servAddr));
//	servAddr.sin_family= AF_INET;
//	servAddr.sin_addr.s_addr= inet_addr("127.0.0.1");
//	servAddr.sin_port= htons(port);
//
//	if (connect(clntSock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0)
//		die("connect failed", clntSock);

//  ADD username password
//	AUTH username password
}

/*
 * Handles HTML service with one client.
 */
class SingleConnServerHTML {
public:
	SingleConnServerHTML(int sock, string webroot);
	~SingleConnServerHTML();
	void backbone();
private:
	string readHTMLFromFile(string fname);
	int sendMsg(string msg);
	void sendStatus(int statusCode, int length);
	void handleGET();
	void handlePOST(char *body);

	int sock;
	string webroot;
	string requestURI;
	bool loggedIn;
	string redirectTo; //the page a user attempts to visit before redirected to login
	set<string> commands;
	map<int, string> status2reason;
};

/*
 * Constructor
 */
SingleConnServerHTML::SingleConnServerHTML(int sock, string webroot):
	sock(sock), webroot(webroot) {
	requestURI = "";
	loggedIn = false;
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

SingleConnServerHTML::~SingleConnServerHTML() {

}

/*
 * Send a message to the client over provided socket.
 */
int SingleConnServerHTML::sendMsg(string msg) {
	char *m = (char *)msg.c_str();
	int i = write(sock, m, strlen(m));
	if (shutdownFlag || i < 0)
		die("Shutdown flag or write fail", sock);
	if (VERBOSE)
		fprintf(stderr, "[%d] S: %s", sock, m);
	return 1;
}

/*
 * Sends a status code with the description.
 * Non-200 codes are wrapped in HTML to display.
 * Note: HTTP version used here affects msg version received from client!
 */
void SingleConnServerHTML::sendStatus(int statusCode, int length = 0) {
	//get corresponding reason in words
	string reason = status2reason[statusCode];
	//status line (
	string statusLine = "HTTP/1.1 " + to_string(statusCode) + " " + reason + "\r\n";
	if (length > 0) {
		statusLine += "Content-Length: " + to_string(length) + "\r\n";
		statusLine += "Content-Type: text/html\r\n";
	}
	//format in HTML if not 200
	if (statusCode != 200) {
		string body =
				"<html><body><h1> " +
				to_string(statusCode) + " " + reason +
				" </h1></body></html>";
		statusLine += "Content-Length: " + to_string(body.length()) + "\r\n";
		statusLine += "Content-Type: text/html\r\n";
		statusLine += "\r\n";
		statusLine += body;
	}
	//no headers
	else {
		statusLine += "\r\n";
	}

	sendMsg(statusLine);
}

/*
 * Read in an html file from disk.
 */
string SingleConnServerHTML::readHTMLFromFile(string fname) {
	string HTML = "";

	//read config file
	ifstream infile(fname);
	if (!infile) {
		string msg = "Can't open " + fname + "\n";
		die(msg, -1);
	}
	string line;
	while (getline(infile, line)) {
		HTML += line;
	}
	HTML += "\r\n\r\n";

	return HTML;
}

/*
 * Handle GET request
 */
void SingleConnServerHTML::handleGET() {
	if (!loggedIn) {
		redirectTo = requestURI;
		string HTML = readHTMLFromFile("templates/login.html");
		sendStatus(200, HTML.length()); //possibly -4 length
		sendMsg(HTML);
		return;
	}

	if (requestURI.compare("/index.html") == 0 || requestURI.compare("/") == 0) {
		//read from templates/index.html
		string HTML = readHTMLFromFile("templates/index.html");
		//send OK and headers
		sendStatus(200, HTML.length());
		//serve landing page
		sendMsg(HTML);
	}
	else if (requestURI.compare("/mail.html") == 0) {
		string HTML = readHTMLFromFile("templates/mail.html");
		sendStatus(200, HTML.length());
		sendMsg(HTML);
	}
	else if (requestURI.compare("/storage.html") == 0) {
		string HTML = readHTMLFromFile("templates/storage.html");
		sendStatus(200, HTML.length());
		sendMsg(HTML);
	}
	else {
		//Resource not found
		sendStatus(404);
	}
}

/*
 * Handle POST request
 */
void SingleConnServerHTML::handlePOST(char *body) {
	char buff[BUFF_SIZE];
	if (requestURI.compare("/handle_login") == 0) {
		//parse login data e.g. user=michal&pass=p
		char *delim = "&\n";
		char *user_str = strtok(body, delim);
		char *pass_str = strtok(NULL, delim);
		char *remember_str = strtok(NULL, delim);
		string user = user_str + strlen("user=");
		string pass = pass_str + strlen("pass=");
		string remember = remember_str + strlen("adduser=");

		if (remember.compare("") != 0) {
			char buff[BUFF_SIZE];
			string s_addCmd = "ADD " + user + " " + pass;
			pthread_mutex_lock(&mutex_backendSock);
			write(backendSock, s_addCmd.c_str(), s_addCmd.length());
			read(backendSock, buff, sizeof(buff));
			if (VERBOSE)
				fprintf(stderr, "%s", buff);
			pthread_mutex_unlock(&mutex_backendSock);
			//redirect to login page
			//TODO: add message stating account add successful
			string HTML = readHTMLFromFile("templates/login.html");
			sendStatus(200, HTML.length());
			sendMsg(HTML);
			return;
		}

		//hardcoded user/pass:
		if (user.compare("michal") != 0 || pass.compare("p") != 0) {
			//redirect to login page
			//TODO: add message stating invalid credentials
			string HTML = readHTMLFromFile("templates/login.html");
			sendStatus(200, HTML.length());
			sendMsg(HTML);
			return;
		}

		loggedIn = true;
		requestURI = redirectTo;
		redirectTo = "";
		handleGET();
	}
}

/*
 * Main
 */
void SingleConnServerHTML::backbone() {
	if (VERBOSE)
		fprintf(stderr, "[%d] S: +OK server ready [localhost]\r\n", sock);

	while (true) {
		//Can't use fgets because POST body is binary data
		//Can't use fread because it blocks (size of POST body variable)
		//Can't use fgets for GET and read for POST because buffered read interferes with standalone read
		//And I don't want to use nonblocking sockets
		//Can't use strtok because of multichar delimiters \r\n and \r\n\r\n

		char c_requestLine[BUFF_SIZE];

	//	memset(c_requestLine, 0, sizeof(c_requestLine));

		if (shutdownFlag)
			die("shutdown", sock);

		if (read(sock, c_requestLine, sizeof(c_requestLine)) < -1)
			sendStatus(400);

		if (VERBOSE)
			fprintf(stderr, "[%d] C: %s\n", sock, c_requestLine);

		//from strtok single character delimiter, modify in-place, char * hell to string paradise
		string requestLine = c_requestLine;
		int i_endline = requestLine.find("\r\n");
		string firstLine = requestLine.substr(0, i_endline);
		string notFirstLine = requestLine.substr(i_endline+strlen("\r\n"));

		//I exaggerated strtok can be better for multitoken split
		char *c_firstLine = (char *)firstLine.c_str();
		char *c_req = strtok(c_firstLine, " ");
		char *reqURI = strtok(NULL, " ");
		char *c_httpVersion = strtok(NULL, " ");

		//check valid request
		string req = c_req;
		if (commands.find(req) == commands.end()) {
			sendStatus(500);
//			return;
			continue;
		}

		string httpVersion = c_httpVersion;
		if (httpVersion.compare("HTTP/1.1") != 0) {
			if (VERBOSE)
				fprintf(stderr, "[%d] S: ERR: wrong HTTP version\n", sock);
			sendStatus(500);
			continue;
		}

		//check first character in URI is "/"
		if (reqURI[0] != '/') {
			if (VERBOSE)
				fprintf(stderr, "[%d] S: ERR: no preceding slash\n", sock);
			sendStatus(400);
//			return;
			continue;
		}
		requestURI = reqURI;

		if (req.compare("GET") == 0) {
			handleGET();
		}
		else if (req.compare("POST") == 0) {
			//get body
			int i_endheaders = notFirstLine.find("\r\n\r\n");
			string body = notFirstLine.substr(i_endheaders+strlen("\r\n\r\n"));
			handlePOST((char *)body.c_str());
		}
		else
			sendStatus(501);
	}
	close(sock);
}

/*
 * Callback from main thread upon initialization of worker thread.
 * Initializes a SingleConnServerHTML
 */
void *threadFunc(void *args){
	struct thread_struct *a = (struct thread_struct *)args;
	int clntSock = (intptr_t)(a->clntSock);
	string webroot = (string)(a->webroot);

	SingleConnServerHTML S(clntSock, webroot);
	S.backbone();
}

int main(int argc, char *argv[])
{
	int c;
	int N = 0;
	int servPort = 8000;
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
			servPort = stoi(optarg);
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

	if (VERBOSE)
		fprintf(stderr, "Webroot: %s\nPort: %d\n\n", webroot.c_str(), servPort);

	//Start serving
	int servSock = createServerSocket(servPort);
	backendSock = createClientSocket(BACKEND_PORT);
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
