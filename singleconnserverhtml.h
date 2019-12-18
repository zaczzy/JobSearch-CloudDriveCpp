#ifndef SINGLECONNSERVERHTML_H
#define SINGLECONNSERVERHTML_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <pthread.h>

#include "cookierelay.h"
#include "backendrelay.h"
#include "mailservice.h"

using namespace std;

/*
 * Handles HTML service with one client.
 */
class SingleConnServerHTML {
public:
	SingleConnServerHTML(int sock, string webroot, CookieRelay *CR, BackendRelay *BR);
	~SingleConnServerHTML();
	void backbone();
private:
	string readHTMLFromFile(string fname);
	string readFavicon(string favicon);
	int sendMsg(string msg);
	void sendStatus(int statusCode);
	void sendHeaders(int length);
	void sendFileHeaders(const string& fname, int length);
	void handleGET(bool HEAD);
	void handlePOST(char *body, bool is_multipart_form, string& boundary, int content_length, int sock);
	void splitHeaderBody(string input, vector<string> *header_list, string *body);
	string generateLogin(string msg = "");
	string generateInbox(get_mail_response *resp);

	int sock;
	string webroot;
	string requestURI;
	string username;
	CookieRelay *CR;
	BackendRelay *BR;
	bool sendCookie;
	int cookieValue;
	string redirectTo; //the page a user attempts to visit before redirected to login
	set<string> commands;
	map<int, string> status2reason;
};

#endif //SINGLECONNSERVERHTML_H
