#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include <vector>
#include <string>

void readConfig_lb(char *configFile, vector<int> *fe_ctrlSocks, vector<string> *fe_fwdSocks);
string sendCommand(string command);
string getFirstFreeServer(vector<int> loads);
void redirect(int clntSock, string fwdAddr);
void *cookieThreadFunc(void *args);

const int MAX_WEB_CLNT = 100;
const int WEB_PORT = 7000;
const int COOKIE_PORT = 9000;

#endif //LOADBALANCER_H
