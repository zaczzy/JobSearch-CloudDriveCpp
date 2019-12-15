#include <arpa/inet.h>  //ntohs
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
#define PORT 6007  // <-- go to this port
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
int *client_fb_all_conns;
map<int, string> msgs_to_send;
void waitingForWork(int i) {
  while (!shouldTerminate.load()) {
    {
      unique_lock<mutex> stdout_lck(stdout_mutex);
      cout << i << " Waiting " << endl;
    }
    unique_lock<mutex> lck(worker_mutex_[i]);
    // check if should go back to sleep
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
static std::string replace_first_occurrence(std::string &s,
                                            const std::string &toReplace,
                                            const std::string &replaceWith) {
  std::size_t pos = s.find(toReplace);
  if (pos == std::string::npos) return s;
  return s.replace(pos, toReplace.length(), replaceWith);
}
// static std::string replace_all_occurrences(std::string& s,
//                                     const std::string& toReplace,
//                                     const std::string& replaceWith) {
//   std::size_t pos = s.find(toReplace);
//   while (true) {
//     if (pos == std::string::npos) return s;
//     s = s.replace(pos, toReplace.length(), replaceWith);
//     pos = s.find(toReplace);
//   }
// }

/*
 * Generate 200 headers for HTML
 */
static string getOKHeaders(int length = 0) {
  string statusLine = "HTTP/1.1 200 OK\r\n";
  // if body exists
  if (length > 0) {
    statusLine += "Content-Type: text/html\r\n";
    statusLine += "Content-Length: " + to_string(length) + "\r\n";
  }
  statusLine += "\r\n";
  return statusLine;
}
/*
 * Generate 200 headers for favicon
 */
static string getFaviconHeaders(int length = 0) {
  string statusLine = "HTTP/1.1 200 OK\r\n";
  // if body exists
  if (length > 0) {
    statusLine += "Content-Type: image/x-icon\r\n";
    statusLine += "Content-Length: " + to_string(length) + "\r\n";
  }
  statusLine += "\r\n";
  return statusLine;
}
static void render_admin_page(int conn_fd) {
  string HTML = readHTMLFromFile("admin.html");
  // replace frontend list with html
  string fsHTML;
  for (auto &fs : frontends) {
    if (fs.status == Alive) {
      fsHTML += "<div>Frontend id " + to_string(fs.key) + " IP " +
                string(inet_ntoa(fs.serv_addr.sin_addr)) + " PORT " +
                to_string((int)ntohs(fs.serv_addr.sin_port)) +
                " status: Alive </div>";
    } else {
      fsHTML += "<div>Frontend id " + to_string(fs.key) + " IP " +
                string(inet_ntoa(fs.serv_addr.sin_addr)) + " PORT " +
                to_string((int)ntohs(fs.serv_addr.sin_port)) +
                " status: Dead </div>";
    }
  }
  replace_first_occurrence(HTML, string("{{frontends}}"), fsHTML);
  string bsHTML;
  string bsGroupHTML;
  for (map<int, vector<server_netconfig>>::iterator it = groups.begin();
       it != groups.end(); it++) {
    bsGroupHTML = "<div>Backend Group " + to_string(it->first) + ": <ul>";
    for (server_netconfig &element : it->second) {
      if (element.status == Alive) {
        bsGroupHTML += "<li>Backend id : " + to_string(element.key) + " IP " +
                       string(inet_ntoa(element.serv_addr.sin_addr)) +
                       " PORT " +
                       to_string((int)ntohs(element.serv_addr.sin_port)) +
                       " status: Alive</li>";
      } else {
        bsGroupHTML += "<li>Backend id : " + to_string(element.key) + " IP " +
                       string(inet_ntoa(element.serv_addr.sin_addr)) +
                       " PORT " +
                       to_string((int)ntohs(element.serv_addr.sin_port)) +
                       " status: Dead</li>";
      }
    }
    bsGroupHTML += "</ul></div>";
    bsHTML += bsGroupHTML;
  }
  replace_first_occurrence(HTML, string("{{backends}}"), bsHTML);
  string header = getOKHeaders(HTML.size());
  HTML.insert(0, header);
  write(conn_fd, HTML.c_str(), HTML.size());
}

void disable_frontend(string body, int conn_fd) {
  size_t id = (size_t) stoi(body.substr(strlen("fid=")));
  size_t thread_id =
      (size_t) frontends[id].key;  // location which thread is supposed to do the job
  msgs_to_send[thread_id] = "DISABLE";
  frontends[id].status = Dead;
}

void disable_backend(string body, int conn_fd) {
  size_t i_sep = body.find("&");
  string s_bgid = body.substr(0, i_sep);
  string s_bid = body.substr(i_sep + 1);
  size_t bgid = (size_t)stoi(s_bgid.substr(strlen("bgid=")));
  size_t bid = (size_t)stoi(s_bid.substr(strlen("bid=")));
  size_t thread_id = (size_t)groups[bgid][bid].key;
  msgs_to_send[thread_id] = "disable";
  groups[bgid][bid].status = Dead;
}

void restart_backend(string body, int conn_fd) {
  size_t i_sep = body.find("&");
  string s_bgid = body.substr(0, i_sep);
  string s_bid = body.substr(i_sep + 1);
  size_t bgid = (size_t)stoi(s_bgid.substr(strlen("bgid=")));
  size_t bid = (size_t)stoi(s_bid.substr(strlen("bid=")));
  size_t thread_id = (size_t) groups[bgid][bid].key;
  msgs_to_send[thread_id] = "restart";
  groups[bgid][bid].status = Alive;
}

void sig_handler(int signo) {
  if (signo == SIGINT) {
    shutdown_server();
  } else {
    printf("Unknown signal sent.\n");
  }
}
#define REQUEST_MAX_LEN 100000

void process_request(int conn_fd) {
  char *buffer = new char[REQUEST_MAX_LEN];
  // main loop
  while (!shouldTerminate) {
    int buffer_size = read(conn_fd, buffer, REQUEST_MAX_LEN);
    if (buffer_size == 0) {
      // client disconnect
      break;
    }
    buffer[buffer_size] = 0;
    string request = buffer;
    size_t firstSpace = request.find(" ");
    size_t secondSpace = request.find(" ", firstSpace + 1);
    string URI = request.substr(firstSpace + 1, secondSpace);
    string body =
        request.substr(request.find("\r\n\r\n") + (string::size_type)4);
    if (request.substr(0, 3).compare("GET") == 0 &&
        request.substr(5, 7).compare("favicon")) {
      // send the dynamic view page
      render_admin_page(conn_fd);
    } else if (request.substr(0, 3).compare("GET") == 0 &&
               request.substr(5, 7).compare("favicon") == 0) {
      // send favicon
      string favicon_binary = readHTMLFromFile("favicon.ico");
      string header = getFaviconHeaders(favicon_binary.size());
      favicon_binary.insert(0, header);
      write(conn_fd, favicon_binary.c_str(), favicon_binary.size());
    } else if (request.substr(0, 4).compare("POST") == 0 &&
               URI.compare("/disable/frontend")) {
      disable_frontend(body, conn_fd);
    } else if (request.substr(0, 4).compare("POST") == 0 &&
               URI.compare("/disable/backend")) {
      disable_backend(body, conn_fd);
    } else if (request.substr(0, 4).compare("POST") == 0 &&
               URI.compare("/restart/backend")) {
      restart_backend(body, conn_fd);
    } else {
      fprintf(stderr, "anomaly.\n");
      break;
    }
  }
  delete[] buffer;
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
    for (server_netconfig &element : it->second) {
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
    num_workers += (int)it->second.size();
  }
  num_workers += (int)frontends.size();

  // init global vars
  workers = new thread[num_workers];
  worker_conds = new condition_variable[num_workers];
  worker_wakeup = new bool[num_workers];
  worker_mutex_ = new mutex[num_workers];
  client_fb_all_conns = new int[num_workers];
  // init all connections
  // backend first
  for (map<int, vector<server_netconfig>>::iterator it = groups.begin();
       it != groups.end(); it++) {
    for (server_netconfig &element : it->second) {
      int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
      client_fb_all_conns[element.key] = sock_fd;
      if (connect(sock_fd, (struct sockaddr *)&element.serv_addr,
                  sizeof(element.serv_addr)) != 0) {
        printf("connection with the backend %d failed...\n", element.key);
      }
    }
  }
  // then frontend
  for (server_netconfig &element : frontends) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    client_fb_all_conns[element.key] = sock_fd;
    if (connect(sock_fd, (struct sockaddr *)&element.serv_addr,
                sizeof(element.serv_addr)) != 0) {
      printf("connection with the frontend %d failed...\n", element.key);
    }
  }
  for (int i = 0; i < num_workers; i++) {
    client_fb_all_conns[i] = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fb_all_conns[i] == -1) {
      printf("socket creation failed...\n");
      exit(0);
    }
  }
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
  int enable = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    fprintf(stderr, "setsockopt(SO_REUSEADDR) failed\n");

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

  // main loop
  while (!shouldTerminate.load()) {
    // Accept the data packet from client and verification
    connfd = accept(sockfd, (SA *)&cli, &len);
    if (connfd < 0) {
      perror("server acccept failed...");
      exit(0);
    }
    process_request(connfd);
    // After close the client connection when
    close(connfd);
    msgs_to_send.clear();
    // enter key demo logic DISABLE PLZ
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
  // final clean up
  close(sockfd);
  for (int i = 0; i < num_workers; i++) {
    workers[i].join();
    close(client_fb_all_conns[i]);
  }
  delete[] worker_wakeup;
  delete[] worker_mutex_;
  delete[] worker_conds;
  delete[] workers;
}