#ifndef SINGLECONNSERVERCONTROL_H
#define SINGLECONNSERVERCONTROL_H

using namespace std;

/*
 * Handles admin control with one client.
 */
class SingleConnServerControl {
public:
	SingleConnServerControl(int sock, function<void(string, bool)> die, bool VERBOSE);
	~SingleConnServerControl();
	void backbone();
private:
	bool VERBOSE;
	int sock;
	set<pthread_t> *webThreads;
	int sendMsg(string msg);
	function<void(string, bool)> die();
};

#endif //SINGLECONNSERVERCONTROL_H
