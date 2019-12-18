//
// Created by Zachary Zhao on 10/8/19.
//
#include <iostream>
//#include <stdio.h>
#include <string.h>
#include "master_thread_handler.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include "constants.h"
#include "master_processor.h"
#include "server_config_store.h"
#include "thread_pool.h"
#include "util.h"
#define BUFFER_SIZE 2048
using namespace std;
void write_sock(int fd, const struct server_netconfig *server_config) {
	cout << "Writing to fes" << endl;
  unsigned int wlen = sizeof(server_config->serv_addr), bytes_w = 0;
  do {
    unsigned int bytes_wt =
        write(fd, &(server_config->serv_addr), wlen - bytes_w);
    if (bytes_wt <= 0) {
      perror("write failed");
    } else {
      bytes_w += bytes_wt;
    }
  } while (bytes_w < wlen);
//	write(fd, &(server_config->serv_addr), sizeof(server_config->serv_addr));
//	cout << "Done" << endl;
}
void write_primary(int fd, int primary_id) {
	write(fd, &primary_id, sizeof(int));
}
static unsigned long find_crlf_begindex(char *str, unsigned long str_size) {
  unsigned long index;
  for (index = 0; index < str_size - 1; index++) {
    if (str[index] == '\r' && str[index + 1] == '\n') {
      break;
    }
  }
  return index;
}
static bool endsWith(const string &mainStr, const string &toMatch) {
  if (mainStr.size() >= toMatch.size() &&
      mainStr.compare(mainStr.size() - toMatch.size(), toMatch.size(),
                      toMatch) == 0)
    return true;
  else
    return false;
}

static bool is_request_for_primary(string &req) {
  return endsWith(req, string("primary"));
}
int my_handler(int fd) {
  unsigned short id_stuff[2];
  memset(id_stuff, 218, sizeof(id_stuff));
  read(fd, &id_stuff, sizeof(id_stuff));

  cout << "Received [" << id_stuff[0] << " " << id_stuff[1] << "]" << endl;

  if (id_stuff[0] != 0 || id_stuff[1] != 0) {
	  // check to register backends
	  pair<int, size_t> tmp;
	  tmp.first = (int) id_stuff[0];
	  tmp.second = (size_t) id_stuff[1];
	  sock2servid[fd] = tmp;
	  // mark backend as alive
	  groups[sock2servid[fd].first][sock2servid[fd].second].status = Alive;
  }
  char *buffer = new char[BUFFER_SIZE + 1];
  int bytes_read;



  //  main loop process command
  while (!should_terminate()) {
    bytes_read = read(fd, buffer, BUFFER_SIZE);
    if (bytes_read <= 0) {
      // ending connection
      if (bytes_read == 0) {
        cout << "client closed connection" << endl;
        groups[sock2servid[fd].first][sock2servid[fd].second].status = Dead;
      } else {
        perror("read failed");
      }
      delete[] buffer;
      close(fd);
      return 0;
    }
    // null terminal and create string instead
    buffer[bytes_read] = '\0';
    string buff_str = buffer;
    // process buffer
    if (is_request_for_primary(buff_str)) {
      int group_id = stoi(buff_str.substr(0, 2));
      process_primary(group_id, fd);
    } else {
      process_command(buff_str, fd);
    }
  }

  delete[] buffer;
  return 1;
}
