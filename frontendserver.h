#ifndef FRONTENDSERVER_H
#define FRONTENDSERVER_H

#include <string>

void cleanup();
//void die(string msg, bool isThread = true);
void readConfig_fes(char *configFile, int configID, string *webIP, int *webPort, string *controlIP, int *controlPort);
string sendCommand(string command);
void *webThreadFunc(void *args);
void *controlThreadFunc(void *args);

const int MAX_WEB_CLNT = 100; //100
const int MAX_CTRL_CLNT = 5;
const int BACKEND_PORT = 5000;
const int COOKIE_PORT = 9000;

#endif //FRONTENDSERVER_H
