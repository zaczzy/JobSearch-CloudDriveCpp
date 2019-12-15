#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

extern is_stop;

/*    
 *   Sends a request to sequentialize, to 
 *   the primary associated with this object.
 *   @req - pointer to the request data
 *   @seq_num - number within the sending system
 *   @primary_fd - socket descriptor used to communicate
 *                 with primary.
 */                  
void req_sequentialize(uint8_t *req, int req_len, int seq_num, int primary_fd) 
{
    int send_bytes;

    send_bytes = send(primary_fd, req, req_len, 0);
    if(send_bytes < req_len) 
    {
      printf("Couldn't send message to primary! %d-%d", myId, primaryId);
      perror(errno);
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
int ensure_consistency(void *arg, char *req, int seq_num) 
{
  
   char *user = (char *)arg;
   uint8_t proceed = 0;
   uint8_t primary = check_if_primary(user);
   uint8_t in_order = 0;

   if(!primary) {
    
    int primaryId = getprimary(user);
    primary_fd = getSocket(primaryId);

    req_sequentialize(req, seq_num, primary_fd);    //Send request to primary after appending its own FIFO ordering

    do {                                  //Checks if received message is next seq number

      proceed = receive_resp(primary_fd, data);
      replica_seq = parse_recv_data(data);
      in_order = check_seq_in_order(replica_seq);

      if(!in_order)
        addtoQ();

    } while(!in_order)

    send_ok(primary_fd);    //Send ack to primary, may change in 2PC

   } else {

     proceed = sequentialize(req);    //Send request to itself

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

void *listen_peer1(void *arg) 
{

  int peer_fd1 = *(int *)arg;
  while(!is_stop) {

   recv(peer_fd1, data); 

   if(waiting_for_ack1 == 1) {
     if(!strcasecmp(data, "ACK")) {
       waiting_for_ack1 = 0;
       continue;
     }
   }

   pthread_mutex_lock(hold_queue);
/* Adds to queue maintaining FIFO ordering of peer1 */
   hold_queue.push_back(data);
   pthread_release_unlock(hold_queue);

  }//Go back to listening

}

/**
*  Function that listens on a
*  TCP connection to its peer
*  in replication group.
*
*  Listens for either new requests
*  or for acknowledgements
*  Runs only if its the primary.
*
*  @arg - Contains the peer file descriptor
*/
void *listen_peer2(void *arg) 
{

  int peer_fd2 = *(int *)arg;
  while(!is_stop) {

   recv(peer_fd2, data); 

   if(!strcasecmp(data, "ACK")) {
     obtain_lock(send_list);
     remove_sendlist(seq_num);
     release_lock(send_list);

     continue;
   }

   pthread_mutex_lock(hold_queue);
/* Adds to queue maintaining FIFO ordering of peer2 */
   hold_queue.push_back(data);
   pthread_mutex_unlock(hold_queue);

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

  while(!is_stop) {

    if(hold_queue.empty()) 
      continue;

    obtain_lock(hold_queue);

    vector<hold_queue>::iterator it;
    it = hold_queue.begin();
    memcpy(data, it->data, it->len);
    hold_queue.erase(it);

    data = pop_from_queue();
    release_lock(hold_queue);
 
    append_seq_number(glbl_seq_num, data);
    memcpy(final_data, glbl_seq_num, sizeof(uint32_t));
    memcpy(final_data + sizeof(uint32_t), data, data_len);

    send_data_peer1(final_data);
    send_data_peer2(final_data);

    /* list used to track sent messages and wait for acks.
    *  If no acks received, resend the message. Perhaps TCP will 
    *  take care of it. Disabling for now.
    
    obtain_lock(send_list1);
    add_sendlist1(data);
    release_lock(send_list1);

    obtain_lock(send_list2);
    add_sendlist2(data);
    release_lock(send_list2);
    */
    glbl_seq_num++;

  }//Go back to sequentializing

}
