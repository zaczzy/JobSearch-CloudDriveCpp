#ifndef SINGLECONNSERVERHTML_H
#define SINGLECONNSERVERHTML_H

/*
 * Handles HTML service with one client.
 */
class SingleConnServerHTML {
public:
	SingleConnServerHTML(int sock, function<void(string, bool)> die, bool VERBOSE, string webroot, CookieRelay *CR);
	~SingleConnServerHTML();
	void backbone();
private:
	string readHTMLFromFile(string fname);
	int sendMsg(string msg);
	void sendStatus(int statusCode);
	void sendHeaders(int length);
	void handleGET(bool HEAD);
	void handlePOST(char *body);
	void splitHeaderBody(string input, vector<string> *header_list, string *body);
	function<void(string, bool)> die();

	bool VERBOSE;
	int sock;
	string webroot;
	string requestURI;
	string username;
	CookieRelay *CR;
	bool sendCookie;
	int cookieValue;
	string redirectTo; //the page a user attempts to visit before redirected to login
	set<string> commands;
	map<int, string> status2reason;
};

#endif SINGLECONNSERVERHTML_H
