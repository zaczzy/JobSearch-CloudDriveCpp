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
#include <signal.h>			//pthread_kill()

#include <string>
#include <set>

using namespace std;

//static void sigintHandler(int signum);
void die(string msg, bool isThread=true);
//void cleanup();
int createServerSocket(unsigned short port);
int createClientSocket(unsigned short port);

extern const int BUFF_SIZE;
extern bool VERBOSE;
extern char *EMPTYSTR;
extern bool shutdownFlag;


#endif //SERVERCOMMON_H
