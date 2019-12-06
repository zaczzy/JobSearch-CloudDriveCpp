#ifndef COOKIERELAY_H
#define COOKIERELAY_H

/*
 * Tracks cookies (single front-end server scenario).
 * Later the public methods will connect to load balancer or backend
 * to fetch and return the data instead of storing local state.
 * SingleConnServerHTML won't know the difference.
 */
class CookieRelay {
public:
	CookieRelay();
	~CookieRelay();
	int genCookie(string browser);
	string fetchBrowser(int cookie);
private:
	int latestCookie;
	map<int, string> cookie2Browser;
};


#endif //COOKIERELAY_H
