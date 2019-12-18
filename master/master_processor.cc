#include "master_processor.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdlib>
#include <ctime>  // For time()
#include <vector>
#include "constants.h"
#include "master_thread_handler.h"
#include "md5.h"
#include "server_config_store.h"
using namespace std;
//process username
void process_command(string& command, int fd) {
  cout << "Master received frontend username: >>>" << command << "<<<" << endl;
  unsigned char* hash_buffer = (unsigned char*)malloc(MD5_DIGEST_LENGTH);
  char* tmp = new char[command.size() + 1];
  strcpy(tmp, command.c_str());
  computeDigest(tmp, command.size(), hash_buffer);
  cout << "Hash Buffer from username: ";
  for (int i = 0; i < MD5_DIGEST_LENGTH ; i++) {
    cout << (int) hash_buffer[i];
  }
  cout << endl;
  delete[] tmp;
  int sum_hash = 0;
  for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
    sum_hash += (int)hash_buffer[i];
  }
  cout << "sum hash: " << sum_hash << endl; 
  sum_hash = sum_hash % (int)groups.size() + 1;
  vector<server_netconfig> servers = groups[sum_hash];

  //Filter dead servers
  vector<server_netconfig> filtered_servers;
  for (auto& server: servers)
	  if (server.status == Dead)
		  filtered_servers.push_back(server);

  srand(time(0));
  int random_pick = rand() % (int)filtered_servers.size();
  write_sock(fd, &filtered_servers[(vector<server_netconfig>::size_type)random_pick]);
}

int h(int x, int range) {
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = ((x >> 16) ^ x) * 0x45d9f3b;
  x = (x >> 16) ^ x;
  return x % range;
}
void process_primary(int group_id, int fd) {
	vector<server_netconfig> candidates = groups[group_id];

	//Filter dead servers
	vector<server_netconfig> filtered_servers;
	for (auto& server: candidates)
		if (server.status == Dead)
			filtered_servers.push_back(server);

	cout << filtered_servers.size() << " alive server" << endl;

	std::vector<server_netconfig>::size_type pick =
			(std::vector<server_netconfig>::size_type) h(group_id, filtered_servers.size());
	int primary_id = filtered_servers[pick].key;

	cout << "primary id: "<< primary_id << endl;
	write_primary(fd, primary_id);
}
