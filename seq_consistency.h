#ifndef SEQ_CONSISTENCY_H
#define SEQ_CONSISTENCY_H
void create_peer_thread(int* fd, int );

void create_primary_thread();

int check_if_primary(char *user, int *primaryId) ;

int getSocket(int serverID) ;

void req_sequentialize(uint8_t *req, int req_len, int seq_num, int primary_fd) ;


int ensure_consistency(uint8_t *req, int req_len, int fd);

void *listen_peer(void *arg) ;

void *primary_sequentialize(void *arg);

#endif
