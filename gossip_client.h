
int init_client_gossip(int myport);

int sock_connect(int fd, char server_ip[], int remote_port);

int init_connection(int *server_fd_addr, char server_ip[], int remote_port) ;

void server_close(int server_fd);

int init_client_gossip(int myport);
