#include <cstring>

#include "servercommon.h"
#include "singleconnserverhtml.h"
#include "mailservice.h"

using namespace std;

//const int BUFF_SIZE = 2048;
//bool VERBOSE = false;
//char *EMPTYSTR = (char *)"";
//bool shutdownFlag = false;

/*
 * Constructor
 */
SingleConnServerHTML::SingleConnServerHTML(int sock, string webroot, CookieRelay *CR, BackendRelay *BR):
	sock(sock), webroot(webroot), CR(CR), BR(BR) {
	requestURI = "";
	sendCookie = false;
	cookieValue = -1;
	redirectTo = "/index.html";
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

/*
 * Destructor
 */
SingleConnServerHTML::~SingleConnServerHTML() {

}

/*
 * Send a message to the client over provided socket.
 */
int SingleConnServerHTML::sendMsg(string msg) {
	char *m = (char *)msg.c_str();
	int i = write(sock, m, strlen(m));
	if (shutdownFlag || i < 0)
		die("Shutdown flag or write fail");
	if (VERBOSE)
		fprintf(stderr, "[%d][WEB] S: %s", sock, m);
	return 1;
}

/*
 * Sends a status code with the description.
 * Non-200 codes are wrapped in HTML to display.
 * Note: HTTP version used here affects msg version received from client!
 */
void SingleConnServerHTML::sendStatus(int statusCode) {
	//get corresponding reason in words
	string reason = status2reason[statusCode];
	//status line
	string statusLine = "HTTP/1.1 " + to_string(statusCode) + " " + reason + "\r\n";
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
	else {
		statusLine += "\r\n";
	}

	sendMsg(statusLine);
}

/*
 * Sends just headers.
 */
void SingleConnServerHTML::sendHeaders(int length = 0) {
	int statusCode = 200;
	//get corresponding reason in words
	string reason = status2reason[statusCode];
	//status line
	string statusLine = "HTTP/1.1 " + to_string(statusCode) + " " + reason + "\r\n";
	//if body exists
	if (length > 0) {
		statusLine += "Content-Length: " + to_string(length) + "\r\n";
		statusLine += "Content-Type: text/html\r\n";
	}
	//if cookie needs to be sent
	if (sendCookie) {
		statusLine += "Set-Cookie: name=" + to_string(cookieValue) + "\r\n";
		sendCookie = false;
	}
	statusLine += "\r\n";

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
		die(msg);
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
void SingleConnServerHTML::handleGET(bool HEAD = false) {
	//ignore favicons
	if (requestURI.compare("/favicon.ico") == 0) {
		sendStatus(200);
		return;
	}

	//if not logged in
	if (cookieValue == -1) {
		cerr << "UH OH " << requestURI << endl;
		redirectTo = requestURI;
		string HTML = readHTMLFromFile("templates/login.html");
		sendHeaders(HTML.length()); //possibly -4 length
		sendMsg(HTML);
		return;
	}

	//logged in
	string HTML;
	//(redirect login.html to index.html, because logged in)
	if (requestURI.compare("/index.html") == 0 ||
			requestURI.compare("/") == 0 ||
			requestURI.compare("/login.html") == 0)
		HTML = readHTMLFromFile("templates/index.html");
	else if (requestURI.compare("/mail/inbox.html") == 0) {
		//SHOW_MAIL
		char c_HTML[BUFF_SIZE];
		webserver_core(0, (char*)username.c_str(), -1, EMPTYSTR, EMPTYSTR, c_HTML, backendSock);
		HTML = c_HTML;
		HTML = readHTMLFromFile("templates/mail.html");
	}
	else if (requestURI.find("/mail/m") == 0) {
		//READ_MAIL
		//URL format: /mail/m[email_id]
		string email_id = requestURI.substr(strlen("/mail/m"));
		char c_HTML[BUFF_SIZE];
		webserver_core(1, (char*)username.c_str(), stoi(email_id), EMPTYSTR, EMPTYSTR, c_HTML, backendSock);
		HTML = c_HTML;
	}
	else if (requestURI.compare("/storage.html") == 0) {
		//storage dummy page
		HTML = readHTMLFromFile("templates/storage.html");
	}
	else {
		//Resource not found
		sendStatus(404);
		return;
	}

	sendHeaders(HTML.length());
	if (!HEAD)
		sendMsg(HTML);
}

/*
 * Handle POST request
 */
void SingleConnServerHTML::handlePOST(char *body) {
	char buff[BUFF_SIZE];
	memset(buff, 0, sizeof(buff));
	if (requestURI.compare("/handle_login") == 0) {
		//parse login data e.g. user=michal&pass=p
		const char *delim = "&\n";
		char *c_user = strtok(body, delim);
		char *c_pass = strtok(NULL, delim);
		char *c_adduser = strtok(NULL, delim);
		string user = c_user + strlen("user=");
		string pass = c_pass + strlen("pass=");
//		cout << "\n===" << user << "," << pass << "===\n" << endl;

		//Add new user
		if (c_adduser != NULL && strlen(c_adduser) > 0) {
			cout << "BYE" << endl;
//			string remember = adduser_str + strlen("adduser=");
			string s_addCmd = "ADD " + user + " " + pass;
			pthread_mutex_lock(&(BR->mutex_sock));
			BR->sendCommand(s_addCmd);
			pthread_mutex_unlock(&(BR->mutex_sock));

			//redirect to login page
			//TODO: add message stating account add successful
			string HTML = readHTMLFromFile("templates/login.html");
			sendHeaders(HTML.length());
			sendMsg(HTML);
			return;
		}

		//Authenticate
		string s_authCmd = "AUTH " + user + " " + pass;
		string authResult = BR->sendCommand(s_authCmd);
		string okerr = authResult.substr(0,3);
		//if invalid credentials
		if (authResult.substr(0,3).compare("+OK") != 0) {
			cout << "HI" << endl;

//		//hardcoded user/pass:
//		if (user.compare("michal") != 0 || pass.compare("p") != 0) {
			//redirect to login page
			//TODO: add message stating invalid credentials
			string HTML = readHTMLFromFile("templates/login.html");
			sendHeaders(HTML.length());
			sendMsg(HTML);
			return;
		}

		sendCookie = true;
		username = user;
		pthread_mutex_lock(&(CR->mutex_sock));
		cookieValue = CR->genCookie(username);
		pthread_mutex_unlock(&(CR->mutex_sock));
		requestURI = redirectTo;
		redirectTo = "";
		handleGET();
	}
	//from some kind of compose_email.html ??
	if (requestURI.compare("/send_mail") == 0) {
		//SEND_MAIL
		//parse data e.g. msg=NoobDown&rcpt=Me
		const char *delim = "&\n";
		char *c_rcpt = strtok(body, delim);
		char *c_msg = strtok(NULL, delim);
		string rcpt = c_rcpt + strlen("rcpt=");
		string msg = c_msg + strlen("msg=");

		//throwaway buffer
		char b[BUFF_SIZE];
		webserver_core(2, (char*)username.c_str(), -1, (char *)msg.c_str(), (char *)rcpt.c_str(), b, backendSock);
		//handle differently on failure?
	}
	if (requestURI.compare("/delete_mail") == 0) {
		//DELETE_MAIL
		//parse data e.g. emailid=777
		string email_id = body + strlen("emailid=");
		char b[BUFF_SIZE];
		webserver_core(3, (char*)username.c_str(), stoi(email_id), EMPTYSTR, EMPTYSTR, b, backendSock);
		//handle differently on failure?
	}
}

/*
 * Get headers and body from an HTTP request.
 */
void SingleConnServerHTML::splitHeaderBody(string input, vector<string> *header_list, string *body) {
	cout << "hi1" << endl;
	//deal with body
	unsigned int i_endheaders = input.find("\r\n\r\n");
	*body = input.substr(i_endheaders+strlen("\r\n\r\n"));

	cout << "hi2" << endl;
	//deal with headers (leave an \r\n for easier iteration below)
	string headers = input.substr(0,i_endheaders+2);
	unsigned int i_endline;
	string remainingHeaders = headers;
	while((i_endline = remainingHeaders.find("\r\n")) != std::string::npos) {
		cerr << i_endline;
		string line = remainingHeaders.substr(0,i_endline);
//		cout << "hi3" << i++ << line << endl;
		remainingHeaders = remainingHeaders.substr(i_endline+strlen("\r\n"));
		if (remainingHeaders.length() == 0)
			break;
		cout << "{" << remainingHeaders << "}" << endl;
		header_list->push_back(line);
	}
	cout << "phuk";
}

/*
 * Main
 */
void SingleConnServerHTML::backbone() {
	if (VERBOSE)
		fprintf(stderr, "[%d][WEB] S: +OK server ready [localhost]\r\n", sock);

	while (true) {
		//Can't use fgets because POST body is binary data
		//Can't use fread because it blocks (size of POST body variable)
		//Can't use fgets for GET and read for POST because buffered read interferes with standalone read
		//And I don't want to use nonblocking sockets
		//Can't use strtok because of multichar delimiters \r\n and \r\n\r\n

		char c_requestLine[BUFF_SIZE];
		memset(c_requestLine, 0, sizeof(c_requestLine));

		if (shutdownFlag)
			die("shutdown");

		int i = read(sock, c_requestLine, sizeof(c_requestLine));
		//read() error
		if (i < -1)
			sendStatus(400);
		//client closed connection
		if (i == 0)
			break;
		if (VERBOSE)
			fprintf(stderr, "[%d][WEB] C: {%s}\n", sock, c_requestLine);


		//from strtok single character delimiter, modify in-place, char * hell to string paradise
		string requestLine = c_requestLine;

		vector<string> headers;
		string body;
		splitHeaderBody(requestLine, &headers, &body);
		string firstLine = headers[0];

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
				fprintf(stderr, "[%d][WEB] S: ERR: wrong HTTP version\n", sock);
			sendStatus(500);
			continue;
		}

		//check first character in URI is "/"
		if (reqURI[0] != '/') {
			if (VERBOSE)
				fprintf(stderr, "[%d][WEB] S: ERR: no preceding slash\n", sock);
			sendStatus(400);
//			return;
			continue;
		}
		requestURI = reqURI;

		//check cookie
		int receivedCookie = -1;
		for (string header: headers) {
			if (header.find("Cookie: ") == 0) {
				receivedCookie = stoi(header.substr(strlen("Cookie: name=")));
				break;
			}
		}
		//existing cookie, existing connection
		if (receivedCookie != -1 && cookieValue != -1) {
			//check matching cookies
			if (receivedCookie != cookieValue) {
				sendStatus(401);
				continue;
			}
		}
		//existing cookie, new connection
		else if (receivedCookie != -1 && cookieValue == -1) {
			//login automatically
			pthread_mutex_lock(&(CR->mutex_sock));
			username = CR->fetchBrowser(cookieValue);
			cookieValue = CR->genCookie(username);
			pthread_mutex_unlock(&(CR->mutex_sock));
		}
		//no cookie, existing connection
		else if (receivedCookie == -1 && cookieValue != -1) {
			//logout scenario
			//possibly unnecessary to handle here,
			//if logout is handled properly elsewhere
		}
		//no cookie, new connection
		else if (receivedCookie == -1 && cookieValue == -1) {
			//standard case; just proceed
		}


		if (req.compare("GET") == 0)
			handleGET();
		else if (req.compare("POST") == 0)
			handlePOST((char *)body.c_str());
		else if (req.compare("HEAD") == 0)
			handleGET(true);
		else
			sendStatus(501);
	}
	close(sock);
}
