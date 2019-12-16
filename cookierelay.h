#ifndef COOKIERELAY_H
#define COOKIERELAY_H

#include <string>
#include <map>
#include <pthread.h>

using namespace std;

/*
 * Tracks cookies (single front-end server scenario).
 * Later the public methods will connect to load balancer or backend
 * to fetch and return the data instead of storing local state.
 * SingleConnServerHTML won't know the difference.
 */
class CookieRelay {
public:
	CookieRelay(int sock);
	~CookieRelay();
	//change genCookie and fetchBrowser to use sockets!!
	int genCookie(string browser);
	void delCookie(int cookie);
	string fetchBrowser(int cookie);

	pthread_mutex_t mutex_sock;
private:
	string sendCommand(string command);

	int sock;
	int latestCookie;
	map<int, string> cookie2Browser;
};


#endif //COOKIERELAY_H
