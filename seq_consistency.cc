#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <map>
#include <algorithm>
#include "seq_consistency.h"
#include "data_types.h"
#include "key_value.h"
#include "logging.h"

#define MAX_DATA_SIZE 1000

pthread_mutex_t hold_q_mutex;
using namespace std;
//extern volatile bool terminate;
extern int peer_fd[10];
extern int gserver_fd[10];
extern int num_gserver_fd;
extern int old_seq;
extern int replica_count;
extern int verbose;
extern int is_primary ;
extern int myserver_id;
extern map<int, int>sid2cfd;
map<int, map<int, int>> acktrack;
pthread_t peer_th[10];
pthread_t primary_th;
uint8_t block_flag = 0;
uint8_t is_checkpoint_block = 0;
uint8_t is_transfer_block = 0;
extern uint8_t disabled_flag;
int send_count = 0;

typedef struct hold_que {
  uint8_t data[3000];
  int data_len;
}hold_q;

vector<hold_q> hold_queue;

typedef struct local_que {
  uint8_t data[3000];
  int data_len;
}local_q;

map<int, local_q> localq; 

void create_primary_thread()
{

    int iret1 = pthread_create(&primary_th, NULL, &primary_sequentialize, NULL);

    if (iret1 != 0)
    {
        if  (verbose)
            fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }
}

void create_peer_thread(int* fd, int replica_count)
{
   #ifdef DEBUG
    printf("replica_count is %d\n", replica_count);
    printf("peer_fd is %d\n", *fd);
    #endif

    int iret1 = pthread_create(&peer_th[replica_count - 1], NULL, &listen_peer, (void*) fd);

    if (iret1 != 0)
    {
        if  (verbose)
            fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }
}

/* Ask master who is primary for user */
int check_if_primary(char *user, int *primaryId) 
{

  /* *primaryId = ask_master(user); 
      if(*primaryId == myId)
        return 1;
      else
        return 0;
  */
  return 0;
}

/* Obtain socket for serverID from mapping. */
int getSocket(int serverID) 
{

  /* Implies that at startup, the servers within a group 
     have to begin a TCP connection with each other.
     The fds used are mapped to the corresponding fd. 
  */
  map<int, int>::iterator it;
  int replicaFD;

  it = sid2cfd.find(serverID);
  if(it != sid2cfd.end())
    replicaFD = it->second;
  else
    replicaFD = -1;

  return replicaFD;
}

/*    
 *   Sends a request to sequentialize, to 
 *   the primary associated with this object.
 *   @req - pointer to the request data
 *   @seq_num - number within the sending system
 *   @primary_fd - socket descriptor used to communicate
 *                 with primary.
 */                  
void req_sequentialize(uint8_t *req, int req_len, int primary_fd) 
{
    int send_bytes;
    map<int, int>::iterator sid2cfditr;

    sid2cfditr = sid2cfd.find(1);
    send_bytes = send(gserver_fd[sid2cfditr->first], req, req_len, 0);
    if(send_bytes < req_len) 
    {
      printf("Couldn't send message to primary!\n");
    }

}

/*
 *  Function invoked to start 
 *  sequential consistency on 
 *  object. 
 *  Invoked by primary and non-primary servers
 *  within a replica group.

 *  @proceed : Return value enabling proceed
 */  
int ensure_consistency(uint8_t *req, int req_len, int fd) 
{
  
   uint8_t proceed = 0;
   int primaryId;
   //uint8_t primary = check_if_primary(user, &primaryId);
   uint8_t in_order = 0;
   uint32_t replica_seq = 0;
   uint8_t *data = (uint8_t *)malloc(MAX_DATA_SIZE + 4);
   uint8_t *seq_data = (uint8_t *)malloc(MAX_DATA_SIZE);
   static int node_seq_num = 0;
   logging_consensus log_struct;
   memset(&log_struct, 0, sizeof(logging_consensus));

   if(!is_primary) {
    //int primary_fd = getSocket(primaryId);
    int primary_fd = gserver_fd[1];

    if(primary_fd < 0) 
    {
      printf("Failed to obtain primary_fd\n");
      return -1;
    }

    log_struct.primaryId = -1;
    log_struct.glbl_seq_num = -1;
    log_struct.requestor_id = myserver_id;
    log_struct.seq_num = node_seq_num;
    log_struct.is_commit = 0;
    memcpy(log_struct.data, req, req_len);
    req_sequentialize((uint8_t *)&log_struct, sizeof(logging_consensus), primary_fd);    //Send request to primary after appending its own FIFO ordering
    block_flag = 1;
    while(block_flag == 1){printf("Blocked\n"); sleep(1);}
    if(disabled_flag == 0)
      process_command((char *)log_struct.data, sizeof(log_struct.data), fd);
    printf("processing command in non-primary block\n");


/*    const char *nack_data = "+NACK";
    log_struct.primaryId = -1;
    log_struct.glbl_seq_num = -1;
    log_struct.requestor_id = myserver_id;
    log_struct.seq_num = node_seq_num;
    memcpy(log_struct.data, nack_data, strlen(nack_data));
    send_bytes = send(primary_fd, &log_struct, sizeof(logging_consensus), 0);
    if(send_bytes < strlen(nack_data)) 
    {
      perror("Couldn't send NO message to primary!");
    }
    */
  
    node_seq_num++;
   } else {
    
    log_struct.primaryId = -1;
    log_struct.glbl_seq_num = -1;
    log_struct.requestor_id = myserver_id;
    log_struct.seq_num = node_seq_num;
    log_struct.is_commit = 0;
    memcpy(log_struct.data, req, req_len);

    struct hold_que elem;
    memcpy(elem.data, &log_struct, sizeof(logging_consensus));
    elem.data_len = sizeof(logging_consensus);

    pthread_mutex_lock(&hold_q_mutex);
    hold_queue.push_back(elem);
    pthread_mutex_unlock(&hold_q_mutex);
    
    block_flag = 1;
    while(block_flag == 1){sleep(1);}
    process_command((char *)log_struct.data, sizeof(log_struct.data), fd);
    printf("processing command in primary block\n");

    node_seq_num++;
   }

   return in_order; 
}

/*
 * Function that listens on a
 * TCP connection to its peer
 * in replication group.

 * Listens for either new requests
 * or for acknowledgements.
 * Runs only if its the primary.

 * @arg - Contains the peer file descriptor
*/

void *listen_peer(void *arg) 
{

  int peer_fd = *(int *)arg;
  uint8_t *data = (uint8_t *)malloc(sizeof(logging_consensus));
  int write_ptr = 0;
  int read_ptr = 0;
  int ack_count = 0;
  map<int, map<int, int> >::iterator ackitr;
  map<int, int> ::iterator subackitr;
  map<int, int>::iterator sid2cfditr;
  logging_consensus log_struct;
  logging_consensus send_struct;
//  printf("Listening to peer\n");

  char strdata[30] = "Listening to peer";

  while(true) {
  
   int data_len = recv(peer_fd, data, sizeof(logging_consensus), 0); 
   if(data_len <= 0) {
    perror("data_len < 0");
    close(peer_fd);
    pthread_exit(NULL);
   }
   memcpy(&log_struct, data, sizeof(logging_consensus)); 
   printf("data read at server_id: %d is %d:%d:%d:%d@%s\n", myserver_id, log_struct.primaryId, log_struct.glbl_seq_num, log_struct.requestor_id, log_struct.seq_num, log_struct.data);

   if(!strcasecmp((char *)log_struct.data, "+ACK")) {
      ackitr = acktrack.find(log_struct.requestor_id);
      if(ackitr == acktrack.end()) {
        /* Should be an impossible case */
        printf("ERROR: User not in primary DB\n");
      } else {
        subackitr = (ackitr->second).find(log_struct.seq_num);
        (subackitr->second--);
        ack_count = subackitr->second;
        if(ack_count == 0) {
          (ackitr->second).erase(subackitr);
          printf("Erased due to 0 ack_count");
        }  
      }
      
      if(ack_count == 0) {
        if(log_struct.requestor_id != log_struct.primaryId) {
          const char *ok_data = "+OK";
          memset(&send_struct, 0, sizeof(logging_consensus));
          send_struct.primaryId = log_struct.primaryId;
          send_struct.glbl_seq_num = log_struct.glbl_seq_num;
          send_struct.requestor_id = log_struct.requestor_id;
          send_struct.seq_num = log_struct.seq_num;
          send_struct.is_commit = 0;
          memcpy(send_struct.data, ok_data, strlen(ok_data));
            int cfd = getSocket(send_struct.requestor_id);
            printf("cfd : %d, send_struct.requestor_id: %d", cfd, send_struct.requestor_id);
            
            int send_bytes = send(gserver_fd[send_struct.requestor_id], &send_struct, sizeof(logging_consensus), 0);
        /*  for(sid2cfditr = sid2cfd.begin(); sid2cfditr != sid2cfd.end(); ++sid2cfditr) {
            if(sid2cfditr->first != myserver_id) {
            int send_bytes = send(gserver_fd[sid2cfditr->first], &send_struct, sizeof(logging_consensus), 0);

            if(send_bytes < sizeof(logging_consensus)) 
            {
              perror("Couldn't send message to peer1!");
            }
            }
          }
          */
          
        } else {
          block_flag = 0;           /* in case the requestor was the primary itself */
        }
     }  
   } else if(!strcasecmp((char *)log_struct.data, "+OK")) {
      printf("received OK at server_id %d\n", myserver_id);
     block_flag = 0;
   }  else if(!strcasecmp((char *)log_struct.data, "+CP_ACK")) {
        send_count--;
        printf("send count checkpoint: %d\n", send_count);
            if(send_count == 0) {
                is_checkpoint_block = 0;
                printf("checkpoint unblocked on 0 send count\n");
            }

   } else if(!strcasecmp((char *)log_struct.data, "+CHECKPOINT")) {
       printf("received CHECKPOINT at server_id %d\n", myserver_id);

       if(disabled_flag == 0)
        take_checkpoint();

       const char *ack_data = "+CP_ACK";
       memset(&send_struct, 0, sizeof(logging_consensus));
       send_struct.primaryId = log_struct.primaryId;
       send_struct.glbl_seq_num = log_struct.glbl_seq_num;
       send_struct.requestor_id = log_struct.requestor_id;
       send_struct.seq_num = log_struct.seq_num;
       send_struct.is_commit = 0;
       memcpy(send_struct.data, ack_data, strlen(ack_data));

       sid2cfditr = sid2cfd.find(1);            /******1 should be replaced with primaryID ********/
       int send_bytes = send(gserver_fd[sid2cfditr->first], &send_struct, sizeof(logging_consensus), 0);
       if(send_bytes < sizeof(logging_consensus)) 
       {
           perror("Couldn't send ACK message to primary!");
       }

   } else if(log_struct.is_commit == 5555) {

     printf("Received transfer file from primary\n") ;  
     if(disabled_flag == 0)
       process_command((char *)log_struct.data, sizeof(log_struct.data), -1);
     continue;
   } else if(log_struct.is_commit == 1) {
     //doLocalcommit(log_struct.data);
     if(log_struct.requestor_id != myserver_id) {
           printf("processing command in non-primary\n");
           if(disabled_flag == 0)
            process_command((char *)log_struct.data, sizeof(log_struct.data), -1);
       }

       const char *ack_data = "+ACK";
       memset(&send_struct, 0, sizeof(logging_consensus));
       send_struct.primaryId = log_struct.primaryId;
       send_struct.glbl_seq_num = log_struct.glbl_seq_num;
       send_struct.requestor_id = log_struct.requestor_id;
       send_struct.seq_num = log_struct.seq_num;
       send_struct.is_commit = 0;
       memcpy(send_struct.data, ack_data, strlen(ack_data));

     //int send_bytes = write(primary_fd, ack_data, strlen(ack_data));
     sid2cfditr = sid2cfd.find(1);   /******1 should be replaced with primaryID ********/

     int send_bytes = send(gserver_fd[sid2cfditr->first], &send_struct, sizeof(logging_consensus), 0);
    if(send_bytes < sizeof(logging_consensus)) 
    {
      perror("Couldn't send YES message to primary!");
    }

   // memset(&send_struct, 0, sizeof(logging_consensus));
    continue;
   }

/* Adds to queue maintaining FIFO ordering of peer1 */
   if(is_primary && ((char)log_struct.data[0] != '+' && log_struct.data[0] != 0) && log_struct.primaryId != 0) { /* All comm messages have a '+' as their 1st character */
     struct hold_que elem;
     memcpy(elem.data, &log_struct, sizeof(logging_consensus));
     elem.data_len = sizeof(logging_consensus);

     pthread_mutex_lock(&hold_q_mutex);
     hold_queue.push_back(elem);
     pthread_mutex_unlock(&hold_q_mutex);
   }
   memset(data, 0, sizeof(logging_consensus));
   memset(&log_struct, 0, sizeof(logging_consensus));

  }//Go back to listening

}

/*
 * Function: primary_sequentialize
 *
 * Thread to ensure a sequential order to 
 * all recipients by popping from queue
 * and appending a sequence number to each 
 * operation. Is started only if it is the 
 * assigned primary.
*/
void *primary_sequentialize(void *arg) {

  /* glbl_seq_num used by primary to sequentialize all operations */
  uint32_t glbl_seq_num = 0;
  uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * MAX_DATA_SIZE);
  //char *data = (char *)malloc(sizeof(char) * MAX_DATA_SIZE);
  uint8_t *final_data = (uint8_t *)malloc(sizeof(char) * MAX_DATA_SIZE);
  const char *log_commit = "+COMMIT";
  const char *log_checkpoint = "+CHECKPOINT";
  //char *final_data = (char *)malloc(sizeof(char) * MAX_DATA_SIZE);
  int send_bytes = 0;
  logging_consensus log_struct;
  logging_consensus rd_log_struct;
  logging_consensus checkpoint_struct;
  map<int, map<int, int> >::iterator ackitr;
  map<int, int>::iterator subackitr;
  map<int, int>::iterator sid2cfditr;
  int peer_count;
  printf("primary_sequentialize up\n");

  while(true) {

  int final_data_len = 0;

    if(hold_queue.empty()) 
      continue;
    
    peer_count = 0;
    printf("hold queue not empty\n");
    printf("log.write(commit) on primary\n");
    vector<hold_q>::iterator it;
    pthread_mutex_lock(&hold_q_mutex);
    it = hold_queue.begin();
    memcpy(&rd_log_struct, it->data, it->data_len);
    hold_queue.erase(it);

    pthread_mutex_unlock(&hold_q_mutex);
 
    if(!strcasecmp((char *)rd_log_struct.data, "-TRANSFER"))
    {
      printf("Received -TRANSFER, Initiate transfer\n");
      is_transfer_block = 1;
      int transfer_cfd = (sid2cfd.find(rd_log_struct.requestor_id)->second);
      transfer_log(transfer_cfd , rd_log_struct.glbl_seq_num, rd_log_struct.requestor_id );
      continue;
    }

    log_struct.primaryId = myserver_id;
    log_struct.glbl_seq_num = glbl_seq_num;
    log_struct.requestor_id = rd_log_struct.requestor_id;
    log_struct.seq_num = rd_log_struct.seq_num;
    memcpy(log_struct.data, log_commit, strlen(log_commit));


    for(sid2cfditr = sid2cfd.begin(); sid2cfditr != sid2cfd.end(); ++sid2cfditr) {
      if(sid2cfditr->first != log_struct.primaryId)
        peer_count++;
    }
    
    if(peer_count > 0) {
      ackitr = acktrack.find(log_struct.requestor_id);
      if(ackitr == acktrack.end()) {
        map<int, int> temp_m;
        temp_m.insert(std::pair<int, int>(log_struct.seq_num, peer_count));
        acktrack.insert(std::pair<int, map<int,int> >(log_struct.requestor_id, temp_m));
      } else {
        (ackitr->second).insert(std::pair<int, int>(log_struct.seq_num, peer_count)) ;
      }
    }

    ackitr = acktrack.find(log_struct.requestor_id);
    
    for(subackitr = (ackitr->second).begin(); subackitr != (ackitr->second).end(); ++subackitr) 
    {
      printf("seqnum %d : ack_count %d\n", subackitr->first, subackitr->second);  
    }

    for(sid2cfditr = sid2cfd.begin(); sid2cfditr != sid2cfd.end(); ++sid2cfditr) {

      if(sid2cfditr->first != myserver_id) {
        send_bytes = send(gserver_fd[sid2cfditr->first], &log_struct, sizeof(logging_consensus), 0);

        if(send_bytes < sizeof(logging_consensus)) 
        {
          perror("Couldn't send message to peer1!");
        }

      }
    }

    memset(&log_struct, 0, sizeof(logging_consensus));

    /* append_seq_number */
    log_struct.primaryId = myserver_id;
    log_struct.glbl_seq_num = glbl_seq_num;
    log_struct.requestor_id = rd_log_struct.requestor_id;
    log_struct.seq_num = rd_log_struct.seq_num;
    log_struct.is_commit = 1;
    memcpy(&log_struct.data, &rd_log_struct.data, 2048);


 // printf("sending data to all replicas\n");
/*
for(int i = 0; i < num_gserver_fd; i++) {
      send_bytes = send(gserver_fd[i], &log_struct, sizeof(logging_consensus), 0);

      if(send_bytes < final_data_len) 
      {
        perror("Couldn't send message to peer1!");
      }

    }
*/

  for(sid2cfditr = sid2cfd.begin(); sid2cfditr != sid2cfd.end(); ++sid2cfditr) {

      if(sid2cfditr->first != myserver_id) {
        send_bytes = send(gserver_fd[sid2cfditr->first], &log_struct, sizeof(logging_consensus), 0);

        if(send_bytes < sizeof(logging_consensus)) 
        {
          perror("Couldn't send message to peer1!");
        }

      }
    }
   
    /* primary updates its own kv store */
    //doLocalCommit();
  if(log_struct.requestor_id != myserver_id) {
    printf("processing command in primary\n");
    process_command((char *)log_struct.data, sizeof(log_struct.data), -1);
  } else if(log_struct.requestor_id == log_struct.primaryId) {
    block_flag = 0;
    sleep(1); 
  }

    memset(&log_struct, 0, sizeof(logging_consensus));

    glbl_seq_num++;

    if(get_log_sequence_no()  % LOG_LIMIT == 0) {

        checkpoint_struct.primaryId = myserver_id;
        checkpoint_struct.glbl_seq_num = glbl_seq_num;
        checkpoint_struct.requestor_id = -1;
        checkpoint_struct.seq_num = -1;
        memcpy(checkpoint_struct.data, log_checkpoint, strlen(log_checkpoint));
        send_count = 0;
        for(sid2cfditr = sid2cfd.begin(); sid2cfditr != sid2cfd.end(); ++sid2cfditr) {

            if(sid2cfditr->first != myserver_id) {

                send_count++;
                send_bytes = send(gserver_fd[sid2cfditr->first], &checkpoint_struct, sizeof(logging_consensus), 0);

                if(send_bytes < sizeof(logging_consensus)) 
                {
                    perror("Couldn't send message to peer1!");
                    send_count--;
                }
            }
        }
        printf("Primary takes checkpoint\n");

        if(peer_count > 0) {        /* Used to ensure that it blocks on CP_ACK only if there are peers */
          is_checkpoint_block = 1;  /* Need to set the flag before take_checkpoint, else sync issues arise */
          take_checkpoint();
          while(is_checkpoint_block == 1) sleep(1);
        } else {
          take_checkpoint();
        }
        glbl_seq_num++;
    }

  }//Go back to sequentializing

}
