/*
 * peer.c
 *
 * Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *          Dave Andersen
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2.
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "uthash.h"
#include "utlist.h"
#include "chunk.h"
#include "common.h"
#include "discovery.h"
#include "log.h"
#include "timer.h"
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include "utarray.h"


#define FILE_NAME_SIZE 1024
#define ID_LEN 5
#define CHUNK_HASH_SIZE 40
#define START_PROCESSING 3
#define FILE_NAME_LEN 255
#define PORT_LEN 10
#define HASH_STR_FORMAT_MAX_LEN 100
#define ChunkOwners_Is_Empty 10
#define FINISHED_PROCESSING_GET 1
#define NOT_DONE_PROCESSSING_GET 0
#define MAX_CLIENTS_SERVING 1020



struct nodeFormat
{
    char ipPort[IP_PORT];
};

/*******Declaring Global Variables*********/
/** Hash Tables **/
chunkOwners *chunkOwnersHash = NULL;
hasChunks *hasChunksHash = NULL;
Peer *peerHash = NULL;
master_chunk_table *masterChunkTableHash = NULL;
TimerTable *timer_table = NULL;
get_file_chunk_table* getChunksHash = NULL;

/** strings **/
// char **file_chunks = NULL;
char **nodeArr = NULL;
char masterChunkFile[FILE_NAME_LEN];
char my_ipPort[IP_PORT];
char *globalPath = NULL;
FILE *outputFile = NULL;

/** integers **/
int numElems = 0; /** Num of Peers **/
int my_sock = -1; /** Peer's own socket **/
int exit_stat = EXIT_SUCCESS;
int timer_set = 0;
int done_processing_get = NOT_DONE_PROCESSSING_GET;
int total_get_serving = 0;

/**Function Declarations**/
void peer_run(bt_config_t *config, char **argv);

/**
 * @brief Called when the 3 second periodic timer goes off. Checks if any
 * timer in the timer_table has expired
 * 
 * @param signo 
 */
void timer_handler(int signo)
{
  timer_set = check_if_timer_expired();
  log_info("%d", timer_set);
}

/**
 * @brief Called when starting the program. Does initializations and calls peer
 * run and in the end clears out the global hash tables.
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv) 
{
  bt_config_t config;
  struct itimerval tv;
  struct sigaction psa;


  log_set_level(LOG_INFO);

  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

#ifdef DEBUG
  if (debug & DEBUG_INIT) {
    bt_dump_config(&config);
  }
#endif
  bzero(masterChunkFile, FILE_NAME_LEN);
  strcpy(masterChunkFile, (&config)->chunk_file);

  /** Initializing Periodic Timer to go off every 3sec**/
  memset (&psa, 0, sizeof (psa));
  psa.sa_handler = timer_handler;
  sigaction(SIGALRM, &psa, NULL);
  tv.it_interval.tv_sec = 1;
  tv.it_interval.tv_usec = 0;  /** when timer expires, reset to 3sec **/
  tv.it_value.tv_sec = 1;
  tv.it_value.tv_usec = 0;   /** 3000 ms == 3000000 us **/
  setitimer(ITIMER_REAL, &tv, NULL);

  peer_run(&config, argv);

 /**Freeing allocated stuff here**/
  if(globalPath != NULL){
    free(globalPath);
  }
  if (outputFile != NULL)
  {
    fclose(outputFile);
  }
  if (nodeArr != NULL)
  {
    free(nodeArr);
  }
  
  clear_chunkOwnersHash();
  clear_getChunksHash();
  clear_timer_table();
  clean_peerHash();
  clean_hasChunksHash();
  clean_masterChunkTableHash();
  return EXIT_SUCCESS;
}
 
/**
 * @brief Sort function used to sort elements in the chunkOwnersHash table.
 * 
 * @param a 
 * @param b 
 * @return int 
 */
int sort_function(void *a, void *b) 
{
  peer_list *elemOne = NULL;
  peer_list *elemTwo = NULL;
  chunkOwners *firstElem = (chunkOwners *)a;
  chunkOwners *secondElem = (chunkOwners *)b;

  int countOne;
  int countTwo;
  
  DL_COUNT(firstElem->ownerList,elemOne,countOne);
  DL_COUNT(secondElem->ownerList,elemTwo,countTwo);
  
  if (countOne < countTwo)
  {
    return -1;
  }
  if(countOne == countTwo)
  {
    return 0;
  }
  return 1;
}

/**
 * @brief Sort function used to sort a peer's seq number hash table.
 * 
 * @param a 
 * @param b 
 * @return int 
 */
int sort_function_seq_num_hash(void *a, void *b) 
{
  seq_num *firstElem = (seq_num *)a;
  seq_num *secondElem = (seq_num *)b;
  
  if (firstElem->seqNum < secondElem->seqNum)
  {
    return -1;
  }
  if(firstElem->seqNum == secondElem->seqNum)
  {
    log_fatal("HASH TABAL INTEGRITY FAILED");
    return 0;
  }
  return 1;
}

/**
 * @brief 
 * 
 * @param hash_value 
 * @return true 
 * @return false 
 */
bool receiving_from_peer(uint8_t* hash_value){
  Peer *current_peer, *tmp;

  HASH_ITER(hh, peerHash, current_peer, tmp) 
  {
    if (memcmp((void *)current_peer->chunkProcessing, (void*)hash_value, HASH_SIZE) == 0) 
    {
      return true;
    }
  }
  return false;
}

/**
 * @brief This function is used to send a GET request to the first free peer
 * who owns a chunk in the chunkOwnersHash table if one exists and returns an
 * error otherwise.
 * 
 * @return int 
 */
int startProcessing()
{
  chunkOwners *elem;
  Peer *currPeer;
  peer_list *chunkPeers = NULL;
  chunk_list *reqChunkList = NULL;
  chunk_list *reqChunk = NULL;
  packet_list* result = NULL;
  
  bool sent = false;
  bool hash_table_empty = true;
  int user_exists = 0;
  char* temp_ptr;
  
  HASH_SORT(chunkOwnersHash, sort_function);
  for(elem=chunkOwnersHash; elem != NULL; elem=elem->hh.next) 
  {
    hash_table_empty = false;
    if (!receiving_from_peer(elem->id))
    {
      sent = false;
      DL_FOREACH(elem->ownerList, chunkPeers)
      {
        HASH_FIND_STR(peerHash, chunkPeers->hash_value, currPeer);
        if(currPeer)
        {
          if(currPeer->busy == PEER_NOT_BUSY && !sent)
          {
            sent = true;
            reqChunk = (chunk_list *)malloc(sizeof(chunk_list));
            memcpy((void *)&reqChunk->hash_value, (void *)elem->id, 
                  sizeof(elem->id));
            DL_APPEND(reqChunkList, reqChunk);
            
            currPeer->busy = PEER_IS_BUSY;
            memcpy((void *)&currPeer->chunkProcessing, (void *)elem->id, 
                  sizeof(elem->id));

            result = make_whohas_ihave_get(reqChunkList, GET);
            temp_ptr = currPeer->ipPort;    
            flood_all(result, &temp_ptr, ONLY_ONE_PEER, my_sock);
            set_timer((double)TIMEOUT_DURATION, GET, currPeer->ipPort, INVALID_SEQ_NUM);
            
            log_info("Sent GET for hash: to peer %s", currPeer->ipPort);
            print_hash_value(reqChunk->hash_value);
            
            free_packet_list(result);
            free_chunk_list(reqChunkList);
            result = NULL;
            reqChunkList = NULL;
          }
          user_exists = 1;
        }
      }
      if (user_exists == 0)
      {
        /* raise error here that no user has requested chunk */
        log_fatal("Peer not found in PeerHash, but peer was in ownerlist of chunkhash");
        return EXIT_FAILURE;
      }
      user_exists = 0;
    }
  }

  if (hash_table_empty)
  {
        log_fatal("ChunkOwners Table is empty, probably no peer in network");
        return ChunkOwners_Is_Empty;
  }

  return EXIT_SUCCESS;
}


/**
 * @brief Processes inbound UDP packets
 *        WHOHAS, IHAVE, GET, ACK, Data
 * 
 * @param sock 
 */
void process_inbound_udp(int sock) 
{  

  struct sockaddr_in from;
  socklen_t fromlen;

  UT_array *rem_timers;
  UT_array *dup_ack_retransmit;
  utarray_new(rem_timers, &ut_int_icd);
  utarray_new(dup_ack_retransmit, &ut_int_icd);
  packet_list* retransmit_packet = NULL;

   
  u_int LPAcked;
  u_int iter = 0;
  u_int seqNum;
  int packet_type = -1;
  int list_count = 0;
  int foundSeqNum = 0;
  int num_seq = 0;
  int rem_timer_iter = 0;

  data_packet_t *packet;
  chunk_list* chunklist_pkt;
  uint8_t* startingp;
  chunkOwners *current_user, *tmp;
  peer_list* elem;
  Peer *currPeer;
  seq_num *seqInstance;
  seq_num *seqTemp, *iter_elem;

  hasChunks* find_result = NULL;
  packet_list* result = NULL;
  chunk_list* head = NULL;
  hasChunks* new_chunk_rec = NULL;
  hasChunks* already_own = NULL;
  get_file_chunk_table* get_chunk_result = NULL;
  
  uint8_t chunkRecieving_buffer[BT_CHUNK_SIZE + 1];
  uint8_t hash_value_temp[SHA_HASH_LEN];
  packet_list *seqPacket;
  packet_list *denied_packet = NULL;
  
  char* temp_ptr = NULL;
  char *tempPeer = NULL;
  char peer[IP_PORT];
  char buf[PACKETLEN];
  char peerlist[IP_PORT];
  char string_port[PORT_LEN];
  char portBuf[PORT_LEN];
  char packet_type_buf[PACKET_NAME_SIZE];
  char hash_str_buf_original[HASH_STR_FORMAT_MAX_LEN];
  char hash_str_buf_recieved[HASH_STR_FORMAT_MAX_LEN];
  int chunkRecieving_buffer_pos = 0;
  int start_processing_result = 0;
  double time_diff;
  double new_rtt_val;
  size_t i;
  
  
  
  bzero(buf, PACKETLEN);
  bzero(peerlist, IP_PORT);
  bzero(string_port, PORT_LEN);
  bzero(hash_value_temp, SHA_HASH_LEN);
  bzero(peer, IP_PORT);
  bzero(portBuf, PORT_LEN);
  bzero(packet_type_buf, sizeof(packet_type_buf));
  bzero(chunkRecieving_buffer, sizeof(chunkRecieving_buffer));
  bzero(hash_str_buf_recieved, sizeof(hash_str_buf_recieved));
  bzero(hash_str_buf_original, sizeof(hash_str_buf_original));


  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, PACKETLEN, 0, (struct sockaddr *) &from, &fromlen);

  packet = (data_packet_t*)(buf);
  packet_type = (packet->header).packet_type;
  packet_type_store_in_buf(packet_type, packet_type_buf);

  log_trace("GOT INBOUND PACKET of type: %s with Seq Num: %u Ack Num %u , from %s:%d Magic Number is %d", packet_type_buf, ntohl((packet->header).seq_num) , ntohl((packet->header).ack_num) ,inet_ntoa(from.sin_addr),
  ntohs(from.sin_port), ntohs((packet->header).magicnum));
  
  /**Constructing IP:Port String for peer who sent packet**/
  inet_ntop(AF_INET, &from.sin_addr, peerlist, 16);
  strcat(peerlist, ":");
  sprintf(string_port,"%d", ntohs(from.sin_port));
  strcat(peerlist, string_port);
  temp_ptr = peerlist;

  strcat(peer, inet_ntoa(from.sin_addr));
  strcat(peer, ":");
  sprintf(portBuf, "%d", ntohs(from.sin_port));
  strcat(peer, portBuf);
  switch (packet_type)
  {
      case WHO_HAS: 
        startingp = (uint8_t*)(packet->data+4);
        for (i = 0; i < packet->data[0]; i++)
        {
            memcpy(hash_value_temp, startingp, SHA_HASH_LEN);
            HASH_FIND(hh, hasChunksHash, hash_value_temp, SHA_HASH_LEN, find_result);
 
            if (find_result != NULL)
            {
              chunklist_pkt = (chunk_list*)malloc(sizeof(chunk_list));
              memset(&(chunklist_pkt->hash_value), 0, sizeof(chunklist_pkt->hash_value));
              memcpy((void*)(&chunklist_pkt->hash_value), (void*)&hash_value_temp, 
                    sizeof(hash_value_temp));
              DL_APPEND(head, chunklist_pkt);
            }
            find_result = NULL;
            bzero(hash_value_temp, SHA_HASH_LEN);
            startingp = startingp + SHA_HASH_LEN;
        }

        if (head != NULL)
        {
            result = make_whohas_ihave_get(head, 1);
            flood_all(result, &temp_ptr, 1, my_sock);
            free_packet_list(result);
            free_chunk_list(head);
            log_trace("Sent IHAVE packet");
        }
        else
        {
          log_trace("Recieved WHOHAS request, didnt have packets");
        }
        break;
      
      case I_HAVE:  
        log_trace("Peer: %s has hash(s):", temp_ptr);
        startingp = (uint8_t*)(packet->data+4);
        for (i = 0; i < packet->data[0]; i++)
        {
            memcpy(hash_value_temp, startingp, SHA_HASH_LEN);
            chunklist_pkt = (chunk_list*)malloc(sizeof(chunk_list));
            memset(&(chunklist_pkt->hash_value), 0, sizeof(chunklist_pkt->hash_value));
            memcpy((void*)(&chunklist_pkt->hash_value), (void*)&hash_value_temp, 
                  sizeof(hash_value_temp));
            DL_APPEND(head, chunklist_pkt);
            print_hash_value(hash_value_temp);
            bzero(hash_value_temp, SHA_HASH_LEN);
            startingp = startingp + SHA_HASH_LEN;
        }
        if(head != NULL)
        {
          log_trace("Recieved IHAVE: Going to modify chunk_owners table");
          make_chunk_owners(head, temp_ptr);

          fprintf(stdout, "chunk_owners state after modification: \n");
          HASH_ITER(hh, chunkOwnersHash, current_user, tmp) {
              DL_COUNT(current_user->ownerList, elem, list_count);
              fprintf(stdout, "Hash Value is : \n");
              print_hash_value(current_user->id);
              fprintf(stdout, "Peer list length = %d \n", list_count);
              elem = NULL;
              DL_FOREACH(current_user->ownerList, elem){
                 fprintf(stdout, "Peer is %s \n", elem->hash_value);
              }
          }
          /** Freeing **/
          free_chunk_list(head);
        } 
        else
        {
          log_fatal("Recieved IHAVE with no packets, NOT EXPECTED");
        }
        break;

      case GET:  
        startingp = (uint8_t*)(packet->data);
        bzero(hash_value_temp, sizeof(SHA_HASH_LEN));
        memcpy(hash_value_temp, startingp, SHA_HASH_LEN);
        log_trace("GOT GET PACKET, HASH IS:");
        print_hash_value(hash_value_temp);
        HASH_FIND(hh, hasChunksHash, hash_value_temp, HASH_SIZE, already_own);

        if (already_own != NULL)
        {
            if (total_get_serving <= MAX_CLIENTS_SERVING)
            {
                HASH_FIND_STR(peerHash, peer, currPeer);
                if (currPeer == NULL)
                {
                  log_fatal("Could not find peer who sent GET request, NOT EXPECTED");
                }
                else if (currPeer->LPAvailable != 0)
                {
                  log_warn("Got another get request from peer to which already sending data. Sending denied!");
                  denied_packet = make_ack_or_denied(INVALID_SEQ_NUM, DENIED);
                  tempPeer = peer;
                  flood_all(denied_packet, &tempPeer, ONLY_ONE_PEER, my_sock);
                  free_packet_list(denied_packet);
                }
                else
                {
                    currPeer->LPAvailable = (u_int)currPeer->window_size;
                    memcpy((void *)&currPeer->chunkSending, (void *)hash_value_temp, 
                              sizeof(hash_value_temp));

                    if (sliding_window_send(hash_value_temp, peer))
                    {
                      log_fatal("Cound not reply to GET with window size packets!");
                    }
                    else
                    {
                      log_trace("Successfully sent window size packets as reply to get");
                      total_get_serving++;
                    }
                }
            }
            else
            {
                denied_packet = make_ack_or_denied(INVALID_SEQ_NUM, DENIED);
                tempPeer = peer;
                flood_all(denied_packet, &tempPeer, ONLY_ONE_PEER, my_sock);
                free_packet_list(denied_packet);
            }
        }
        else
        {
            denied_packet = make_ack_or_denied(INVALID_SEQ_NUM, DENIED);
            tempPeer = peer;
            flood_all(denied_packet, &tempPeer, ONLY_ONE_PEER, my_sock);
            free_packet_list(denied_packet);
        }
       break; 

      case DATA: 
        seqNum = ntohl((packet->header).seq_num);
        HASH_FIND_STR(peerHash, peer, currPeer);
        if (currPeer)
        {
          if (currPeer->busy == PEER_IS_BUSY)
          {
              if ((int)seqNum > (int)currPeer->maxSeq)
              {
                currPeer->maxSeq = seqNum;
              }
              /** Only add to table if not already there**/
              HASH_FIND_INT(currPeer->seqNumHash, &seqNum, seqTemp);
              if (seqTemp == NULL)
              {
                seqInstance = (seq_num *)malloc(sizeof(seq_num));
                seqInstance->seqNum = seqNum;
                seqInstance->num_bytes = (int)(ntohs(packet->header.packet_len) - ntohs(packet->header.header_len));
                bzero(seqInstance->data, sizeof(seqInstance->data));
                memcpy(seqInstance->data, packet->data, seqInstance->num_bytes);
                HASH_ADD_INT(currPeer->seqNumHash, seqNum, seqInstance);
                currPeer->num_bytes_recieved = currPeer->num_bytes_recieved + seqInstance->num_bytes;
              }
              seqTemp = NULL;
              LPAcked = currPeer->LPAcked_sent;

              for(iter = LPAcked + 1; iter < seqNum; iter++)
              {
                HASH_FIND_INT(currPeer->seqNumHash, &iter, seqTemp);
                if((seqTemp == NULL) && (foundSeqNum == 0))
                {
                  seqNum = iter - 1;
                  foundSeqNum = 1;
                }
                seqTemp = NULL;
              }
              if (foundSeqNum == 0)
              {
                num_seq = (int)currPeer->maxSeq;
                for(iter = seqNum + 1; iter < num_seq; iter++)
                {
                  HASH_FIND_INT(currPeer->seqNumHash, &iter, seqTemp);
                  if((seqTemp == NULL) && (foundSeqNum == 0))
                  {
                    seqNum = iter - 1;
                    foundSeqNum = 1;
                  }
                  seqTemp = NULL;
                }
                if (foundSeqNum == 0)
                {
                  seqNum = num_seq;
                }
              }
              currPeer->LPAcked_sent = seqNum;
              seqPacket = make_ack_or_denied(seqNum, ACK);
              tempPeer = peer;
              flood_all(seqPacket, &tempPeer, ONLY_ONE_PEER, my_sock);
              free_packet_list(seqPacket);

              if (currPeer->num_bytes_recieved == BT_CHUNK_SIZE)
              {          
                  HASH_SORT(currPeer->seqNumHash, sort_function_seq_num_hash);
                  for(iter_elem=currPeer->seqNumHash; iter_elem != NULL; iter_elem=iter_elem->hh.next) 
                  {
                    memcpy(chunkRecieving_buffer + chunkRecieving_buffer_pos, iter_elem->data, iter_elem->num_bytes);
                    chunkRecieving_buffer_pos = chunkRecieving_buffer_pos + iter_elem->num_bytes;
                  }
                  print_hash_value_into_buf(currPeer->chunkProcessing, hash_str_buf_original);
                  bzero(hash_value_temp, sizeof(SHA_HASH_LEN));
                  shahash(chunkRecieving_buffer, BT_CHUNK_SIZE, hash_value_temp);
                  print_hash_value_into_buf(hash_value_temp, hash_str_buf_recieved);

                  if (strcmp(hash_str_buf_original, hash_str_buf_recieved) == 0)
                  {
                    log_info("Recieved a complete Chunk %s. Hash Matched!", hash_str_buf_original);

                    /** Addind to hasChunksHash**/
                    new_chunk_rec = (hasChunks *)malloc(sizeof(hasChunks));
                    memcpy((void*)(&new_chunk_rec->id), (void*)&hash_value_temp, 
                          sizeof(hash_value_temp));
                    new_chunk_rec->index = 0;
                    HASH_ADD(hh, hasChunksHash, id, sizeof(new_chunk_rec->id), new_chunk_rec);
        
                    /** Remove from hunkOwnersHash (for recursive call)**/
                    if(remove_from_chunkOwnersHash(hash_value_temp)){
                      log_fatal("Error removing hash value from chunkOwnersHash");
                    }

                    /** Write Data to File**/
                    HASH_FIND(hh, getChunksHash, hash_value_temp, HASH_SIZE, get_chunk_result);
                    if (get_chunk_result == NULL)
                    {
                      log_fatal("Could not find chunk %s in getchunkshash");
                    }
                    else
                    {
                      if (fseek(outputFile, (BT_CHUNK_SIZE * get_chunk_result->offset), SEEK_SET))
                      {
                          log_fatal("File seek failed to seek to chunk index");
                      }
                      else if(fwrite(chunkRecieving_buffer, sizeof(uint8_t), BT_CHUNK_SIZE , outputFile) != BT_CHUNK_SIZE)
                      {
                            log_fatal("Failed to write chunk to file!");
                      }
                      else
                      {
                        log_info("Succsssfully wrote chunk to file!");
                        get_chunk_result->written = 1;
                      }
                    }
                  }
                  else
                  {
                    log_fatal("Recieved a complete Chunk. Hash Not Matched\n Original: %s\n Recieved: %s", hash_str_buf_original, hash_str_buf_recieved);
                  }

                  remove_timer(peer, INVALID_SEQ_NUM);
                  
                  /** Clearing State **/
                  clean_peer_state_recieving(currPeer);
                  start_processing_result = startProcessing();
                  if (start_processing_result)
                  {
                    if (start_processing_result == ChunkOwners_Is_Empty)
                    {
                      log_info("Done recieving all chunks!");
                      done_processing_get = FINISHED_PROCESSING_GET;
                    }
                    else
                    {
                      log_fatal("StartProcessing threw an eror when called after receving a chunk!");
                    }              
                  }
              }
              else
              {
                modify_timer(peer, (double)TIMEOUT_DURATION, INVALID_SEQ_NUM);
              }
          }
          else
          {
            log_fatal("Recieved DATA packet from PEER from whom I am not recieving data, peer id: %s", peer);
          }
        }
        break;
        
      case ACK:
        HASH_FIND_STR(peerHash, peer, currPeer);
        if (currPeer == NULL)
        {
          log_fatal("Could not find peer who sent GET request, NOT EXPECTED");
        }

        if (currPeer->LPSent != 0)
        {
          if (currPeer->LPAcked_recieved == ((u_int)ntohl((packet->header).ack_num)))
          {
            currPeer->dup_acks_recieved++;
            if (currPeer->dup_acks_recieved == 3)
            {
              /** Experienced Loss**/
              if (currPeer->slow_start == SLOW_START_PHASE)
              {
                log_info("In slow start, shifting to congestion avoidance due to dup ack.");
                currPeer->slow_start = CONGESTION_AVOIDANCE_PHASE;
                currPeer->ssthresh = max_num(currPeer->window_size/2,2);
                currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
                if (currPeer->rtt_val != (double)0)
                {
                  log_info("here T");
                  set_timer(2 * currPeer->rtt_val, CONGESTION_AVOIDANCE_TIMER, currPeer->ipPort, CONGESTION_SEQ_NUM);
                }
                else
                {
                  log_info("here T");
                  set_timer((double)TIMEOUT_DURATION, CONGESTION_AVOIDANCE_TIMER, currPeer->ipPort, CONGESTION_SEQ_NUM);
                }     
              }
              else
              {
                  /** Ahmed this stuff have a look at it, this is the phase were we experience loss in Congestion avoidance!**/
                  log_info("In congestion avoidance, shifitng to slow start due to dup ack.");
                  currPeer->ssthresh = max_num(currPeer->window_size/2,2);
                  currPeer->slow_start = SLOW_START_PHASE;
                  currPeer->window_size = INITIAL_WINDOW_SIZE;
                  currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
                  log_info("AZL");
                  remove_timer(currPeer->ipPort, CONGESTION_SEQ_NUM);
              }

              currPeer->dup_acks_recieved = 0;
              for(rem_timer_iter= (int)(currPeer->LPAcked_recieved+1); rem_timer_iter < (int)currPeer->LPAcked_recieved+2; rem_timer_iter++) {
                  utarray_push_back(dup_ack_retransmit, &rem_timer_iter);
              }
              retransmit_packet = make_data_packets(currPeer->chunkSending, dup_ack_retransmit);
              temp_ptr = peer;
              log_fatal("Calling it form here");
              flood_all(retransmit_packet, &temp_ptr, 1, my_sock);
              free_packet_list(retransmit_packet);
              log_info("Retransmitted seqnum %d for %s", currPeer->LPAcked_recieved+1, peer);
            }
          }
          else
          {         
            for(rem_timer_iter=currPeer->LPAcked_recieved+1; rem_timer_iter < ((int)ntohl((packet->header).ack_num))+1; rem_timer_iter++)
            {
              utarray_push_back(rem_timers, &rem_timer_iter);
            }

            time_diff = timer_duration(currPeer->ipPort, (u_int)ntohl((packet->header).ack_num));

            if (time_diff == (double)TIMER_DOES_NOT_EXIST)
            {
              log_fatal("Timer for TCP packet does not exist in table");
            }
            else
            {
              // log_info("Time difference is %f", time_diff);
              new_rtt_val = ((currPeer->rtt_val * (double)currPeer->num_rtt_samples) 
                            + (double)time_diff) / (double)(currPeer->num_rtt_samples + 1);
              currPeer->rtt_val = new_rtt_val;
              currPeer->num_rtt_samples++;
              // log_info("RTT and #sample are %f and %d", currPeer->rtt_val, currPeer->num_rtt_samples);
            }


            if (currPeer->slow_start == SLOW_START_PHASE)
            {
              log_info("In slow start ack case");
              currPeer->window_size++;
              if (currPeer->window_size >= currPeer->ssthresh)
              {
                log_info("In slow start, shifting to congestion avoidance due to ssthresh");
                currPeer->slow_start = CONGESTION_AVOIDANCE_PHASE;
                currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
                if (currPeer->rtt_val != (double)0)
                {
                  log_info("Time is %f", currPeer->rtt_val);
                  set_timer(2 * currPeer->rtt_val, CONGESTION_AVOIDANCE_TIMER, currPeer->ipPort, CONGESTION_SEQ_NUM);
                }
                else
                {
                  log_info("Time is Now %f", (double)TIMEOUT_DURATION);
                  set_timer((double)TIMEOUT_DURATION, CONGESTION_AVOIDANCE_TIMER, currPeer->ipPort, CONGESTION_SEQ_NUM);
                  log_info("Here First");
                }  
              }
            }
            else
            {
              log_info("In congestion avoidance ack case");
              currPeer->detected_loss_or_ACK = DETECTED_ACK;
              /** ahmed **/
            }
            remove_reliability_timers(peer, rem_timers);
            currPeer->LPAcked_recieved = (u_int)(ntohl((packet->header).ack_num));
            if (currPeer->LPAcked_recieved + currPeer->window_size >= 512)
            {
              currPeer->LPAvailable = 512;
            }
            else
            {
                currPeer->LPAvailable = currPeer->LPAcked_recieved + currPeer->window_size;
            }

            if (currPeer->LPAcked_recieved == 512)
            {
              /** Clearing State **/
              bzero(currPeer->chunkSending, sizeof(currPeer->chunkSending));
              currPeer->LPAcked_recieved = 0;
              currPeer->LPAvailable = 0;
              currPeer->LPSent = 0;
              currPeer->dup_acks_recieved = 0;
              currPeer->window_size = INITIAL_WINDOW_SIZE;
              currPeer->slow_start = SLOW_START_PHASE;
              currPeer->ssthresh = INITIAL_SSTHRESH;
              currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
              currPeer->rtt_val = (double)0;
              currPeer->num_rtt_samples = 0;
              log_info("AZL");
              double time_diff1;
              time_diff1 = timer_duration(currPeer->ipPort, CONGESTION_SEQ_NUM);
              log_info("Time completed is %f", time_diff1);
              remove_timer(currPeer->ipPort, CONGESTION_SEQ_NUM);
            }
            else
            {
              sliding_window_send(currPeer->chunkSending, peer);
            }
          }
        }
        else
        {
          log_fatal("Recieved a Malicious ACK");
        }
        break;
      case DENIED:
        break;

      default:
        break;
  }

}
/**
 * @brief Processes incoming GET request from STDIN of peer
 * 
 * THIS FUNCTION NEEDS REVISITING
 * @param chunkfile 
 * @param outputfile 
 */
void process_get(char *chunkfile, char *outputfile) 
{
  int numBytesRead = 0;
  int chunkId;
  int varsUsed = 0;
  size_t bufSize = 0;
  int count = 0;
  
  uint8_t temp_hash_value[HASH_SIZE];
  char chunkHash[CHUNK_HASH_SIZE];
  bzero(chunkHash, CHUNK_HASH_SIZE);
  bzero(temp_hash_value, HASH_SIZE);
  
  char *buf = NULL;
  chunk_list* list_pkt = NULL;
  packet_list *result;
  chunk_list* req_list = NULL;
  chunk_list* elt = NULL;
  hasChunks *found = NULL;

  get_file_chunk_table* new_chunk = NULL;
  
  outputFile = fopen(outputfile, "w");
  if (outputFile == NULL)
  {
    log_fatal("Could not open new output file for writing.");
  }
  

  // hasChunks* cuuu, *tmmpp;
  // HASH_ITER(hh, hasChunksHash, cuuu, tmmpp) {
  //   print_hash_value(cuuu->id);
  // }


  FILE *getChunkFile = fopen(chunkfile, "r");
  if (getChunkFile == NULL)
  {
    log_fatal("%s", "Get chunk file not present");
    exit_stat = EXIT_FAILURE;
    return;
  } 
  else
  {
    numBytesRead = getline(&buf, &bufSize, getChunkFile);
  
    while(numBytesRead >= 0)
    {
      varsUsed = sscanf(buf, "%d %s", &chunkId, chunkHash);
      if (varsUsed == 2)
      {
        memset(temp_hash_value, 0, HASH_SIZE);
        hex2binary(chunkHash, CHUNK_HASH_SIZE, temp_hash_value);
        list_pkt = (chunk_list*)malloc(sizeof(chunk_list));

        memset(&(list_pkt->hash_value), 0, sizeof(list_pkt->hash_value));
        memcpy((void*)(&list_pkt->hash_value), (void*)&temp_hash_value, 
              sizeof(temp_hash_value));
        
        HASH_FIND(hh, hasChunksHash, temp_hash_value, HASH_SIZE, found);
        if (!found)
        {
          log_info("came here for some reason");
          DL_APPEND(req_list,list_pkt);
        }

        /** Add to hash **/
        new_chunk= (get_file_chunk_table *)malloc(sizeof(get_file_chunk_table));
        memcpy((void*)(&new_chunk->id), (void*)&temp_hash_value, 
              sizeof(temp_hash_value));
        new_chunk->offset = chunkId;
        new_chunk->written = 0;
        HASH_ADD(hh, getChunksHash, id, sizeof(new_chunk->id), new_chunk);
      }
      numBytesRead = getline(&buf, &bufSize, getChunkFile);
    }
    DL_COUNT(req_list, elt, count); 
    if(count == 0)
    {
      log_info("OWNED ALL CHUNKS");
      done_processing_get = FINISHED_PROCESSING_GET;
      return;
    }
    result = make_whohas_ihave_get(req_list, WHO_HAS);
    flood_all(result, nodeArr, numElems, my_sock);
    if (set_timer((double)TIMEOUT_DURATION, PROCESSING, my_ipPort, INVALID_SEQ_NUM))
    {
      log_fatal("Could not set PROCESSING timer after sending WHO_HAS requests");
    }

    free_packet_list(result);
    free_chunk_list(req_list);
    fclose(getChunkFile);
  }
}

/**
 * @brief Parses the STDIN GET request and calls process_get to process the
 * request
 * 
 * @param line 
 * @param cbdata 
 */
void handle_user_input(char *line, void *cbdata) 
{
  char chunkf[128], outf[128];
  bzero(chunkf, sizeof(chunkf));
  bzero(outf, sizeof(outf));

  if (sscanf(line, "GET %120s %120s", chunkf, outf)) 
  {
    if (strlen(outf) > 0) 
    {
      process_get(chunkf, outf);
    }
  }
}

/**
 * @brief Initialises a peer's state
 * 
 * @param ipAddress 
 * @param port 
 * @return Peer* 
 */
Peer *peer_init(char *ipAddress, int port)
{
  Peer *peer = malloc(sizeof(struct Peers));
  bzero(peer->ipAddress, IP_ADDRESS);
  bzero(peer->port, PORT);
  bzero(peer->ipPort, IP_PORT);
  strcpy(peer->ipAddress, "");
  strcpy(peer->port, "");
  strcpy(peer->ipPort,"");
  
  /** Receiver Uses these**/
  peer->busy = 0;
  peer->LPAcked_sent = 0;
  peer->NextExpectedPacket = 1;
  peer->maxSeq = 0;
  peer->seqNumHash = NULL;
  peer->num_bytes_recieved = 0;
  bzero(peer->chunkProcessing, HASH_SIZE);
  /**/

  /*Sender uses these*/
  peer->window_size = INITIAL_WINDOW_SIZE;
  peer->slow_start = SLOW_START_PHASE;
  peer->ssthresh = INITIAL_SSTHRESH;
  peer->LPAcked_recieved = 0;
  peer->LPAvailable = 0;
  peer->LPSent = 0;
  peer->dup_acks_recieved = 0;
  peer->rtt_val = (double)0;
  peer->num_rtt_samples = 0;
  peer->detected_loss_or_ACK = NOTHING_DETECTED;
  bzero(peer->chunkSending, HASH_SIZE);
  /**/
  return peer;
}

/**
 * @brief Initialises the global Hash tables masterChunkTableHash, peerHash
 * and hasChunksHash
 * 
 * @param argv 
 * @param peer_info 
 */
void do_preprocessing(char **argv, Peer *peer_info)
{
  char peerFileName[FILE_NAME_SIZE];
  char chunkFileName[FILE_NAME_SIZE];
  char peerHashKey[IP_PORT];
  char ipAddress[IP_PORT], port[PORT_LEN], chunkHash[HASH_SIZE], id[ID_LEN], 
       symbol[100], masterHash[40];
  char *buf = NULL;
  char *chunkBuf = NULL;
  char *ptr = NULL;
  
  uint8_t temp_hash_value[HASH_SIZE];
  uint8_t temp_chunkHash_value[HASH_SIZE];
  size_t bufSize = 0;
  size_t chunkBufSize = 0;
  
  int numBytesRead = 0;
  int masterBytesRead = 0;
  int varsUsed = 0;
  int chunkVarsUsed = 0;
  int offset = 0;
  int num_chunks_in_hash =0;
  int chunkId;
  
  hasChunks *chunkInst = NULL;
  master_chunk_table *master_chunk = NULL;
  
  
  bzero(ipAddress, sizeof(ipAddress));
  bzero(port, sizeof(port));
  bzero(chunkHash, sizeof(chunkHash));
  bzero(id, sizeof(id));
  bzero(peerFileName, sizeof(peerFileName));
  bzero(chunkFileName, sizeof(chunkFileName));
  bzero(peerHashKey, sizeof(peerHashKey));

  strcpy(peerFileName, argv[2]);
  strcpy(chunkFileName, argv[4]);

  FILE *masterCFile = fopen(masterChunkFile, "r");
  if (masterCFile == NULL)
  {
    log_fatal("%s", "Master Chunks file not present");
    exit_stat = EXIT_FAILURE;
    return;
  }
  else
  {
    masterBytesRead = getline(&chunkBuf, &chunkBufSize, masterCFile);
    globalPath = malloc(masterBytesRead + 1);
    chunkVarsUsed = sscanf(chunkBuf, "%s %s", symbol, globalPath); 

    masterBytesRead = getline(&chunkBuf, &chunkBufSize, masterCFile);
    masterBytesRead = getline(&chunkBuf, &chunkBufSize, masterCFile);
    while(masterBytesRead >= 0)
    {
      chunkVarsUsed = sscanf(chunkBuf, "%d %s", &offset, masterHash); 
      if(chunkVarsUsed == 2)
      {
        master_chunk = (master_chunk_table *)malloc(sizeof(master_chunk_table));
        hex2binary(masterHash, CHUNK_HASH_SIZE, temp_chunkHash_value);
        memcpy((void*)(&master_chunk->id), (void*)&temp_chunkHash_value, 
              sizeof(temp_chunkHash_value));
        master_chunk->offset = offset;
        HASH_ADD(hh, masterChunkTableHash, id, sizeof(master_chunk->id), master_chunk);
      }
      masterBytesRead = getline(&chunkBuf, &chunkBufSize, masterCFile);
    }
    fclose(masterCFile);
  }
  num_chunks_in_hash = HASH_COUNT(masterChunkTableHash);
  log_trace("Num Chunks in masterChunkTableHash are %d", num_chunks_in_hash);
  
  FILE *requestedFile = fopen(peerFileName, "r");
  if (requestedFile == NULL)
  {
    log_fatal("%s", "Peer file not present");
    exit_stat = EXIT_FAILURE;
    return;
  } 
  else
  {
    numBytesRead = getline(&buf, &bufSize, requestedFile);
    while(numBytesRead >= 0)
    {
      varsUsed = sscanf(buf, "%s %s %s", id, ipAddress, port); 
      
      if (varsUsed == 3)
      {
        if (!strcmp(id, argv[10]))
        {
          bzero(my_ipPort, IP_PORT);
          strcat(peer_info->ipAddress, ipAddress);
          strcat(peer_info->port, port);
          strcat(peerHashKey, ipAddress);
          strcat(peerHashKey, ":");
          strcat(peerHashKey, port);
          strcat(my_ipPort, peerHashKey);
          strcat(peer_info->ipPort, peerHashKey);
          HASH_ADD_STR(peerHash, ipPort , peer_info);

        }
        else
        {
          Peer *peer_temp = peer_init(ipAddress, atoi(port));
          strcat(peer_temp->ipAddress, ipAddress);
          strcat(peer_temp->port, port);
          numElems++;
          strcat(ipAddress, ":");
          strcat(ipAddress, port);
          strcat(peer_temp->ipPort, ipAddress);
          HASH_ADD_STR(peerHash, ipPort , peer_temp);
          ptr = malloc(strlen(ipAddress)+1);
          strcpy(ptr,ipAddress);
          if (nodeArr == NULL)
          {
            nodeArr = malloc(sizeof(char *));
          }
          else
          {
            nodeArr = realloc(nodeArr, sizeof(char *) * numElems);
          }
          nodeArr[numElems - 1] = ptr;
          bzero(ipAddress, sizeof(ipAddress));
          bzero(port, sizeof(port));
        }
      }
      numBytesRead = getline(&buf, &bufSize, requestedFile);
    }
    fclose(requestedFile);
  }

  FILE *chunkFile = fopen(chunkFileName, "r");
  if (chunkFile == NULL)
  {
    log_fatal("%s", "Chunk file not present");
    exit_stat = EXIT_FAILURE;
    return;
  }
  else
  {
    numBytesRead = getline(&buf, &bufSize, chunkFile);
    while(numBytesRead >= 0)
    {
      varsUsed = sscanf(buf, "%d %s", &chunkId, chunkHash);
      if (varsUsed == 2)
      {
        chunkInst = (struct Chunks *)malloc(sizeof(struct Chunks));
        hex2binary(chunkHash, CHUNK_HASH_SIZE, temp_hash_value);
        memcpy((void*)(&chunkInst->id), (void*)&temp_hash_value, 
              sizeof(temp_hash_value));
        chunkInst->index = chunkId;
        HASH_ADD(hh, hasChunksHash, id, sizeof(chunkInst->id), chunkInst);
      }
      numBytesRead = getline(&buf, &bufSize, chunkFile);
    }
    //log_info("hasChunksHash size is %d", HASH_COUNT(hasChunksHash));
    fclose(chunkFile);
  }
}

/**
 * @brief The main function for a running peer i.e. where peer waits for either
 * a STDIN GET request or bytes from other peers.
 * 
 * @param config 
 * @param argv 
 */
void peer_run(bt_config_t *config, char **argv) 
{
  int sock;
  int timer_val;

  char *ipAddress = NULL;
  
  uint8_t tempo_buffer[BT_CHUNK_SIZE +1];

  fd_set readfds;

  struct user_iobuf *userbuf;
  struct sockaddr_in myaddr;

  Peer * peer_info;
  get_file_chunk_table *current_chunk, *tmp;
  hasChunks *found_chunk = NULL;
  FILE* master_file = NULL;
  master_chunk_table* found_result = NULL;


  bzero(tempo_buffer, sizeof(tempo_buffer));
  
  
  if ((userbuf = create_userbuf()) == NULL) 
  {
    perror("peer_run could not allocate userbuf");
    exit(-1);
  }
  
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) 
  {
    perror("peer_run could not create socket");
    exit(-1);
  }
  
  bzero(&myaddr, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(config->myport);
  
  if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) 
  {
    perror("peer_run could not bind socket");
    exit(-1);
  }

  my_sock = sock;
  ipAddress = inet_ntoa(myaddr.sin_addr);
  spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
  
  
  log_trace("Peer Identity is :%d", config->identity);
  peer_info = peer_init(ipAddress, config->myport);
  do_preprocessing(argv, peer_info);
  
  while (1) 
  {

    if (exit_stat)
    {
      log_fatal("An error occured while trying to request file from peers. Please try again. EXIT_STAT = EXIT_FALIURE");
      return;
    }
    if (timer_set)
    {
      log_info("In timer_set");
      timer_set = 0;
      timer_val = handle_timers();

      if (timer_val == EXIT_FAILURE)
      {
          log_fatal("handle_timers() returned EXIT_FAILURE");
          return;
      }

      if (timer_val == START_PROCESSING)
      {
        log_trace("Going to start processing because PROCESSING timer went off"); 
        if(startProcessing())
        {
          log_fatal("Something bad happened in startProcessing, EXITING!");
          return;
        }
      }
    }

    if (done_processing_get)
    {
      done_processing_get = NOT_DONE_PROCESSSING_GET;
      /** Writing the chunks that I already owned, if there were any!**/
      
      HASH_ITER(hh, getChunksHash, current_chunk, tmp) 
      {
          if (current_chunk->written != 1)
          {
            HASH_FIND(hh, hasChunksHash, current_chunk->id, HASH_SIZE, found_chunk);
            if (found_chunk == NULL)
            {
              log_fatal("There exists an unwritten chunk that I do not own! exiting!");
              exit_stat = 1;
            }
            else if ((master_file = fopen(globalPath, "r")) == NULL)
            {
              log_fatal("Coundnt open masterchunks file");
              exit_stat = 1;
            }
            else
            {
                HASH_FIND(hh, masterChunkTableHash, current_chunk->id, HASH_SIZE, found_result);
                if (found_result == NULL)
                {
                    log_fatal("Coundnt find chunk that I own in masterChunkTableHash");
                    exit_stat = 1;
                }
                else if (fseek(outputFile, (BT_CHUNK_SIZE * current_chunk->offset), SEEK_SET))
                {
                    log_fatal("File seek failed to seek to chunk index in output file");
                    exit_stat = 1;
                }
                else if (fseek(master_file, (BT_CHUNK_SIZE * found_result->offset), SEEK_SET))
                {
                    log_fatal("File seek failed to seek to chunk index in master data file");
                    exit_stat = 1;
                }
                else if (fread(tempo_buffer, sizeof(uint8_t), BT_CHUNK_SIZE, master_file) != BT_CHUNK_SIZE)
                {
                    log_fatal("An error happened while reading masterchunk file for a chunk I own. Did not read complete data!");
                    exit_stat = 1;
                }
                else if(fwrite(tempo_buffer, sizeof(uint8_t), BT_CHUNK_SIZE, outputFile) != BT_CHUNK_SIZE)
                {
                    log_fatal("Failed to write chunk to output file!");
                    exit_stat = 1;
                }              
                else
                {
                  log_info("Successfilly wrote already owned chunk in File");
                }
            }
          }
       }
      /****/
      clear_chunkOwnersHash();
      clear_getChunksHash();
      clear_timer_table();
      fclose(outputFile);
      outputFile = NULL;

      if(total_get_serving > 0)
      {
          total_get_serving = total_get_serving - 1;
      }
      if (exit_stat != 1)
      {
        log_info("Done clearing state after successful GET file transfer!");
      }
      else
      {
         log_fatal("An error occured while trying to write already owned chunk to file! EXITING");
         return;
      }
    }
    
    int nfds;
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);
    nfds = select(sock+1, &readfds, NULL, NULL, NULL);
    
    if (nfds > 0) 
    {
      if (FD_ISSET(sock, &readfds)) 
      {
	process_inbound_udp(sock);
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) 
      {
	process_user_input(STDIN_FILENO, userbuf, handle_user_input,
			   "Currently unused");
      }
    }
  }
}

