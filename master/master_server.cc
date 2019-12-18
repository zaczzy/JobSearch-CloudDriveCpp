#include <signal.h>
#include <string.h>
#include <unistd.h>
#include "master_thread_handler.h"
#include "server_config_store.h"
#include "thread_pool.h"
#include "util.h"
#define DEFAULT_PORT 2333
#define SEAS_LOGIN "zhaozeyu"
#define FULL_NAME "Zeyu Zhao"
#define QUEUE_LENGTH 50
#define NUM_THREADS 10
using namespace std;

void sig_handler(int signo) {
  if (signo == SIGINT) {
    terminate_thread_pool();
  }
}

static void set_sock_options(int server_socket_fd) {
  exit_guard(server_socket_fd == 0, "Server socket init failed");
  int opt_val = 1;
  exit_guard(setsockopt(server_socket_fd, SOL_SOCKET,
                        SO_REUSEADDR | SO_REUSEPORT, &opt_val, sizeof(opt_val)),
             "setsockopt error");
}

static int bind_listen(int server_socket_fd, int server_port) {
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(server_port);
  if (bind(server_socket_fd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("Port binding failed");
    return -1;
  }
  if (listen(server_socket_fd, QUEUE_LENGTH) < 0) {
    perror("Listen failed");
    return -1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
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
        exit_guard(true, "Error: default case for options is hit");
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
      cout << "Server: " << element.key << ":"
           << "IP " << inet_ntoa(element.serv_addr.sin_addr) << " PORT "
           << ntohs(element.serv_addr.sin_port) << endl;
    }
  }
  // sigaction on the SIGINT and SIGUSER1
  struct sigaction actions;
  memset(&actions, 0, sizeof(actions));
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = sig_handler;
  exit_guard(sigaction(SIGINT, &actions, NULL), "Error: Cannot catch SIGINT");
//  exit_guard(sigaction(SIGUSR1, &actions, NULL),
//             "Error: Cannot catch SIGUSER1");
// FIXME: why catch siguser1

  init_thread_pool(NUM_THREADS, my_handler);
  int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  set_sock_options(server_socket_fd);
  exit_guard(bind_listen(server_socket_fd, DEFAULT_PORT) != 0,
             "Error: Bind failed");
  while (!should_terminate()) {
    sockaddr_in client_addr;
    socklen_t cli_len = sizeof(client_addr);
    int socket_fd =
        accept(server_socket_fd, (struct sockaddr *)&client_addr, &cli_len);
    if (socket_fd < 0) {
      if (errno != EINTR) {
        // accept failure
        perror("Accept failed");
      }
      terminate_thread_pool();
      break;
    }
    thread_pool_enqueue(socket_fd);
  }
  join_all_threads();
  return 0;
}
