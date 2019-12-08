#ifndef FRONTENDSERVER_H
#define FRONTENDSERVER_H

#include <string>
#include "cookierelay.h"
#include "backendrelay.h"

void cleanup();
//void die(string msg, bool isThread = true);
void readConfig_fes(char *configFile, int configID, string *webIP, int *webPort, string *controlIP, int *controlPort);
string sendCommand(string command);
void *webThreadFunc(void *args);
void *controlThreadFunc(void *args);

const int MAX_WEB_CLNT = 100;
const int MAX_CTRL_CLNT = 5;
const int BACKEND_PORT = 5000;
const int LOADBALANCER_PORT = 4000;

/*
 * Struct for passing multiple parameters to webThreadFunc()
 */
struct web_thread_struct {
    int clntSock;
    int backendSock;
    string webroot;
    CookieRelay *CR;
    BackendRelay *BR;
};

/*
 * Struct for passing multiple parameters to controlThreadFunc()
 */
struct control_thread_struct {
    int clntSock;
};

#endif //FRONTENDSERVER_H
