#ifndef SINGLECONNSERVERCONTROL_H
#define SINGLECONNSERVERCONTROL_H

#include <string>
#include <set>

using namespace std;

/*
 * Handles admin control with one client.
 */
class SingleConnServerControl {
public:
	SingleConnServerControl(int sock);
	~SingleConnServerControl();
	void backbone();
private:
	int sock;
	set<pthread_t> *webThreads;
	int sendMsg(string msg);
};

#endif //SINGLECONNSERVERCONTROL_H
