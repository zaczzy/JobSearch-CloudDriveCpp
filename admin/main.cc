#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <vector>
#include "server_config_store.h"
#define MAX 80
#define PORT 6007 // <-- go to this port
#define SA struct sockaddr

using namespace std;
atomic<bool> shouldTerminate{false};
// init workers
int num_workers = 0;
thread *workers;
condition_variable *worker_conds;
mutex stdout_mutex;
mutex *worker_mutex_;
bool *worker_wakeup;

void waitingForWork(int i) {
  while (!shouldTerminate.load()) {
    {
      unique_lock<mutex> stdout_lck(stdout_mutex);
      cout << i << " Waiting " << endl;
    }
    unique_lock<mutex> lck(worker_mutex_[i]);
    worker_conds[i].wait(lck, [i] { return worker_wakeup[i]; });
   {
      unique_lock<mutex> stdout_lck(stdout_mutex);
      cout << i << " Running " << endl;
    }
    // turn self off
    worker_wakeup[i] = false;
  }
}
static void shutdown_server() {
  shouldTerminate = true;
  // wake up
  // TODO: enable
  // for (int i = 0; i < num_workers; i++) {
  //   {
  //     lock_guard<mutex> lck(worker_mutex_[i]);
  //     worker_wakeup[i] = true;
  //   }
  //   worker_conds[i].notify_one();
  // }
}
static string readHTMLFromFile(string fname) {
  string HTML = "";
  // read config file
  ifstream infile(fname);
  if (!infile) {
    string msg = "Can't open " + fname + "\n";
    shutdown_server();
    return HTML;
  }
  string line;
  while (getline(infile, line)) {
    HTML += line;
  }
  HTML += "\r\n\r\n";

  return HTML;
}

static void render_admin_page(int conn_fd) {
  //  TODO: implement
}

void sig_handler(int signo) {
  if (signo == SIGINT) {
    shutdown_server();
  } else {
    printf("Unknown signal sent.\n");
  }
}
void process_request(int conn_fd) {
  bool disconnect = false;
  char *buffer = new char[100000];
  while(true) {
    int buffer_size = read(conn_fd, buffer, sizeof(buffer));
    buffer[buffer_size] = 0;
    string request = buffer;
    delete [] buffer;
    if (request.substr(0,3).compare("GET") == 0) {
      // send the dynamic view page
      string HTML = readHTMLFromFile("admin.html");
    } 
    if (disconnect) {
      break;
    }
  }
}
int main(int argc, char *argv[]) {
  // sigaction on the SIGINT and SIGUSER1
  struct sigaction actions;
  memset(&actions, 0, sizeof(actions));
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = sig_handler;
  if (sigaction(SIGINT, &actions, NULL)) {
    fprintf(stderr, "Error: Cannot catch SIGINT\n");
    return -1;
  }

  int opt;
  char *config_name;
  while ((opt = getopt(argc, argv, "t:")) != -1) {
    string opt_arg_str;
    switch (opt) {
      case 't':
        config_name = optarg;
        break;
      case '?':
        exit(EXIT_FAILURE);
      default:
        fprintf(stderr, "Error: default case for options is hit\n");
        return -1;
    }
  }
  // setup server config store
  if (argc != 3) {
    fprintf(stderr, "Need 3 argument: instead got %d\n", argc);
    return -1;
  } else {
    read_config_file(config_name);
  }
  // test contents
  for (map<int, vector<server_netconfig>>::iterator it = groups.begin();
       it != groups.end(); it++) {
    cout << it->first << ':' << endl;
    vector<server_netconfig> servers = it->second;
    for (server_netconfig &element : servers) {
      cout << "BServer: " << element.key << ":"
           << "IP " << inet_ntoa(element.serv_addr.sin_addr) << " PORT "
           << ntohs(element.serv_addr.sin_port) << endl;
    }
  }
  for (server_netconfig &element : frontends) {
    cout << endl
         << "FServer: " << element.key << ":"
         << "IP " << inet_ntoa(element.serv_addr.sin_addr) << " PORT "
         << ntohs(element.serv_addr.sin_port) << endl;
  }

  // calc num_workers
  for (map<int, vector<server_netconfig>>::iterator it = groups.begin();
       it != groups.end(); it++) {
    num_workers += (int) it->second.size();
  }
  num_workers += (int) frontends.size();

  // init global vars
  workers = new thread[num_workers];
  worker_conds = new condition_variable[num_workers];
  worker_wakeup = new bool[num_workers];
  worker_mutex_ = new mutex[num_workers];
  
  // TODO: enable
  // for (int i = 0; i < num_workers; i++) {
  //   workers[i] = thread(waitingForWork, i);
  // }

  // init frontend server
  int sockfd, connfd;
  socklen_t len;
  struct sockaddr_in servaddr, cli;
  // socket create and verification
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == -1) {
    printf("socket creation failed...\n");
    exit(0);
  } else
    printf("Socket successfully created..\n");
  bzero(&servaddr, sizeof(servaddr));
  // assign IP, PORT
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(PORT);

  // Binding newly created socket to given IP and verification
  if ((bind(sockfd, (SA *)&servaddr, sizeof(servaddr))) != 0) {
    printf("socket bind failed...\n");
    exit(0);
  } else
    printf("Socket successfully binded..\n");

  // Now server is ready to listen and verification
  if ((listen(sockfd, 5)) != 0) {
    printf("Listen failed...\n");
    exit(0);
  } else
    printf("Server listening..\n");
  len = sizeof(cli);

  while (!shouldTerminate.load()) {
    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA *)&cli, &len);
    if (connfd < 0) {
      printf("server acccept failed...\n");
      exit(0);
    }

    // TODO: implement
    process_request(connfd);

    // After chatting close the socket
    close(sockfd);

    // char c = fgetc(stdin);
    // if (c == '\n') {
    //   // wake up
    //   for (int i = 0; i < num_workers; i++) {
    //     {
    //       lock_guard<mutex> lck(worker_mutex_[i]);
    //       worker_wakeup[i] = true;
    //     }
    //     worker_conds[i].notify_one();
    //   }
    // } else {
    //   shutdown_server();
    // }
  }
  for (int i = 0; i < num_workers; i++) {
    workers[i].join();
  }
  delete [] worker_wakeup;
  delete [] worker_mutex_;
  delete [] worker_conds;
  delete [] workers;
}