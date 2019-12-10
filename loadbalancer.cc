#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <sys/time.h>
#include <time.h>

#include "servercommon.h"

using namespace std;

vector<int> loads;

//Cookie storage here

int main(int argc, char *argv[])
{
	char *configFile;
	int configID;

	//Handle options
	while ((c = getopt (argc, argv, "avp:")) != -1) {
		if (c == 'v') {
			VERBOSE = true;
		}
		else if (c == 'p') {
			servPort = stoi(optarg);
		}
		else if (c == 'a'){
			fprintf(stderr, "*** Author: Michal Porubcin (michal19)\n");
			return 0;
		}
	}

//	//non option arguments
	if (optind >= argc)
		die("*** Author: Michal Porubcin (michal19)\n", -1);
	configID = atoi(argv[optind]);

	//need to connect to each config server
	configFile = (char*)"config_fes.txt";

	string webIP;
	int webPort;
	string configIP;
	int configPort;
	readConfig_fes(configFile, configID, &webIP, &webPort, &controlIP, &controlPort);
	struct sockaddr_in bAddr;
	createSocket(bindaddress, &sock, &bAddr);

	handleMsgs(&fAddrs, order);
}
