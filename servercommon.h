#ifndef SERVERCOMMON_H
#define SERVERCOMMON_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>			//close()
#include <fcntl.h>			//fcntl()
#include <arpa/inet.h>		//htons()
#include <poll.h>
#include <unistd.h>			//getopt()
#include <pthread.h>
#include <signal.h>

#include <string>
#include <set>

using namespace std;

static void sigintHandler(int signum);
void die(string msg, bool isThread=true);
//void cleanup();
int createServerSocket(unsigned short port);
int createClientSocket(unsigned short port);

extern const int BUFF_SIZE = 2048;
extern bool VERBOSE = false;
extern char *EMPTYSTR = (char *)"";
extern bool shutdownFlag = false;
extern set<int> socks;


#endif //SERVERCOMMON_H
