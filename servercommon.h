#ifndef SERVERCOMMON_H
#define SERVERCOMMON_H

static void sigintHandler(int signum);
void die(string msg, int sock, bool isThread=true);
void cleanup(int sock, int sock2);
int createServerSocket(unsigned short port);
int createClientSocket(unsigned short port);

const int BUFF_SIZE = 2048;

#endif //SERVERCOMMON_H
