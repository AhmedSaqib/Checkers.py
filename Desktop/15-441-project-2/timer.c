#include <time.h>
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
#include "utarray.h"
#include <math.h>

#define START_PROCESSING 3
#define TIME_OUT 5
#define SEQ_NUM_LEN 5

extern TimerTable *timer_table;
extern char my_ipPort[IP_PORT];
extern Peer *peerHash;
extern int my_sock;
extern int exit_stat;
extern chunkOwners *chunkOwnersHash;

/**
 * @brief Used to check if any timer in the timer table has expired.
 * 
 * @return int 
 */
int check_if_timer_expired(){
   TimerTable *current_timer, *tmp;

   /** Uncomment for debugging
   unsigned int num_users;
   num_users = HASH_COUNT(timer_table);
   printf("there are %u timers\n", num_users);
   **/

   HASH_ITER(hh, timer_table, current_timer, tmp) {
      if (difftime(time(NULL), current_timer->timestamp) >= current_timer->duration)
      {
         return 1;
      }
   }
   return 0;
}

/**
 * @brief Adds a new timer to the timer_table by making the appropriate timer
 * object and adding it to the timer_table or returning EXIT_FAILURE upon 
 * failure.
 * 
 * @param duration 
 * @param type 
 * @param peer 
 * @param seq_num 
 * @return int 
 */
int set_timer(double duration, int type, char* peer, u_int seq_num)
{
      TimerTable  *new, *result;
      char timer_type[PACKET_NAME_SIZE];
      char seq_num_buf[SEQ_NUM_LEN];
      bzero(seq_num_buf, sizeof(seq_num_buf));
      bzero(timer_type, sizeof(timer_type));
      
      new = (TimerTable*)malloc(sizeof(TimerTable));
      new->duration = duration;
      // log_info("duration in set is %f", duration);
      new->type = type;
      new->seq_num = seq_num;
      new->succ_timeout_count = 0;
      new->timestamp = time(NULL);    
      struct timeval tv;
      gettimeofday(&tv, NULL);
      new->utime = tv;
      bzero(new->id, TIMER_ID_LEN);
      packet_type_store_in_buf(type, timer_type);
      
      if(strlen(peer) > TIMER_ID_LEN){
         log_fatal("peer %s has length longer than TIMER_ID_LEN , not expected", peer);
         free(new);
         return EXIT_FAILURE;
      }

      //memcpy((void*)(&new->id), (void*)peer, strlen(peer)); 
      strcpy(new->id, peer);

      if (type == TCP_RELIABILITY)
      {
         strcat(new->id, "|");
         sprintf(seq_num_buf, "%u", seq_num);
         strcat(new->id, seq_num_buf);
      }

      if (type == CONGESTION_AVOIDANCE_TIMER)
      {
         strcat(new->id, "|CT");
      }
       
      HASH_ADD(hh, timer_table, id, sizeof(new->id), 
            new);
      HASH_FIND(hh, timer_table, new->id, TIMER_ID_LEN, result);

      if (result)
      {
         log_trace("Added timer of type %s for duration %f for peer %s", timer_type, duration, new->id);
         // log_info("here");
         return EXIT_SUCCESS;
      }
      else
      {
         log_fatal("Could NOT add timer of type %s for duration %f for peer %s", timer_type, duration, new->id);
         return EXIT_FAILURE;
      }
     
}

/**
 * @brief Removes a timer from timer_table. Peer and seq_num together identify
 * the timer to be removed.
 * 
 * @param peer 
 * @param seq_num 
 * @return int 
 */
int remove_timer(char* peer, u_int seq_num)
{
   TimerTable *result;
   char id_temp[TIMER_ID_LEN];
   char timer_type[PACKET_NAME_SIZE];
   char seq_num_buf[SEQ_NUM_LEN];
   
   bzero(seq_num_buf, sizeof(seq_num_buf));
   bzero(id_temp, sizeof(id_temp));
   bzero(timer_type, sizeof(timer_type));
   
   if(strlen(peer) > TIMER_ID_LEN){
      log_fatal("peer %s has length longer than TIMER_ID_LEN, not expected", peer);
      return EXIT_FAILURE;
   }
   //memcpy((void*)id_temp, (void*)peer, strlen(peer));
   strcpy(id_temp, peer);


   if(seq_num == CONGESTION_SEQ_NUM)
   {
      log_trace("Trying to modify a TCP_RELIABILITY TIMER");
      strcat(id_temp, "|CT");
   }
   else if (seq_num != 0)
   {
      // log_trace("Trying to remove a TCP_RELIABILITY TIMER");
      strcat(id_temp, "|");
      sprintf(seq_num_buf, "%u", seq_num);
      strcat(id_temp, seq_num_buf);
   }

   HASH_FIND(hh, timer_table, id_temp, TIMER_ID_LEN, result);

   if (result == NULL){
      log_fatal("timer for peer %s not found in table. Not expected", peer);
      return EXIT_FAILURE;
   }
   else{
      HASH_DEL(timer_table, result);
      free(result);
      packet_type_store_in_buf(result->type, timer_type);
      log_trace("Removed timer of type %s of duration %d for peer %s", timer_type, result->duration, id_temp);
      return EXIT_SUCCESS;
   }
}

/**
 * @brief Returns timer_duration if timer was found in timer_table
 *        returns TIMER_DOES_NOT_EXIST otherwise
 * 
 * @param peer 
 * @param seq_num 
 * @return int 
 */
double timer_duration(char* peer, u_int seq_num){
   TimerTable *result; 
   char id_temp[TIMER_ID_LEN];
   char seq_num_buf[SEQ_NUM_LEN];
   
   bzero(seq_num_buf, sizeof(seq_num_buf));
   bzero(id_temp, sizeof(id_temp));

   if(strlen(peer) > TIMER_ID_LEN)
   {
      log_fatal("peer %s has length longer than TIMER_ID_LEN, not expected", peer);
      return (double)TIMER_DOES_NOT_EXIST;
   }
   strcpy(id_temp, peer);

   if (seq_num == CONGESTION_SEQ_NUM)
   {
      strcat(id_temp, "|CT");
   }
   else if (seq_num != INVALID_SEQ_NUM)
   {
      strcat(id_temp, "|");
      sprintf(seq_num_buf, "%u", seq_num);
      strcat(id_temp, seq_num_buf);
   }
   HASH_FIND(hh, timer_table, id_temp, TIMER_ID_LEN, result);
   if (result == NULL)
   {
      log_fatal("%u", seq_num);
      return (double)TIMER_DOES_NOT_EXIST;
   }
   else
   {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      // printf("Current time is %ld\n", tv.tv_sec * (int)1e6 + tv.tv_usec);
      // printf("Set time is %ld\n", result->utime.tv_sec * (int)1e6 + result->utime.tv_usec);
      return ((double)((tv.tv_sec * (int)1e6 + tv.tv_usec) - (result->utime.tv_sec * (int)1e6 + result->utime.tv_usec)))/(double)1e6; 
   }

}


/**
 * @brief Used to modify an existing timer's duration and at the same time 
 * updates other information e.g. timestamp. peer and seq_num are together
 * used to identify the timer to be modified.
 * 
 * @param peer 
 * @param duration 
 * @param seq_num 
 * @return int 
 */
int modify_timer(char* peer, double duration, u_int seq_num){
   TimerTable *result;
   char id_temp[TIMER_ID_LEN];
   char timer_type[PACKET_NAME_SIZE];
   char seq_num_buf[SEQ_NUM_LEN];
   
   bzero(seq_num_buf, sizeof(seq_num_buf));
   bzero(id_temp, sizeof(id_temp));
   bzero(timer_type, sizeof(timer_type));
   
   if(strlen(peer) > TIMER_ID_LEN){
      log_fatal("peer %s has length longer than TIMER_ID_LEN, not expected", peer);
      return 1;
   }
   //memcpy((void*)id_temp, (void*)peer, sizeof(id_temp));
   strcpy(id_temp, peer);

   if(seq_num == CONGESTION_SEQ_NUM)
   {
      log_trace("Trying to modify a TCP_RELIABILITY TIMER");
      strcat(id_temp, "|CT");
   }
   else if (seq_num != 0)
   {
      log_trace("Trying to modify a TCP_RELIABILITY TIMER");
      strcat(id_temp, "|");
      sprintf(seq_num_buf, "%u", seq_num);
      strcat(id_temp, seq_num_buf);
   }
   HASH_FIND(hh, timer_table, id_temp, TIMER_ID_LEN, result);
   
   if (result == NULL)
   {
      return EXIT_FAILURE;
   }
   else 
   {
      if (result->type == GET)
      {
         result->type = DATA;
         log_trace("TYPE changed from GET to DATA");
      }
      result->succ_timeout_count = 0;
      result->timestamp = time(NULL);
      result->duration = duration;
      packet_type_store_in_buf(result->type, timer_type);
      log_trace("Modified timer of type %s for peer %s. New duration: %d", timer_type, peer, duration);
      return EXIT_SUCCESS;
   }
}

/**
 * @brief this function sends the chunk that was requested from remPeer to some
 * other peer (if one is available) since remPeer is no longer active.
 * 
 * @param remPeer 
 */
void sendDiffGET(Peer *remPeer)
{
   chunkOwners *found = NULL;
   peer_list *chunkPeers = NULL;
   Peer *currPeer;
   chunk_list *reqChunkList = NULL;
   chunk_list *reqChunk = NULL;
   packet_list* result = NULL;
   int user_exists = 0;
   char *temp_ptr;
 
   peer_list *delPkt, *temp_chunk;
   chunkOwners *current_user, *tmp;
 
   log_trace("In SendDiffGet for  %s", remPeer->ipPort);   
   HASH_FIND(hh, chunkOwnersHash, remPeer->chunkProcessing, sizeof(remPeer->chunkProcessing), found);
   if (found)
   {     
      /** Remove this peer from every chunk's peer list**/
      HASH_ITER(hh, chunkOwnersHash, current_user, tmp) 
      {
         DL_FOREACH_SAFE(current_user->ownerList, delPkt, temp_chunk) 
         {
            if(strcmp(delPkt->hash_value, remPeer->ipPort) == 0)
            {
                  DL_DELETE(current_user->ownerList, delPkt);
                  free(delPkt);
                  log_trace("Deleted peer %s", remPeer->ipPort);
            }
         }
      }

      DL_FOREACH(found->ownerList, chunkPeers)
      {
         HASH_FIND_STR(peerHash, chunkPeers->hash_value, currPeer);
         if(currPeer)
         {
            if(currPeer->busy == 0)
            {
               log_trace("Available Peer is %s", chunkPeers->hash_value);
               reqChunk = (chunk_list *)malloc(sizeof(chunk_list));
               memcpy((void *)&reqChunk->hash_value, (void *)found->id, 
                     sizeof(found->id));
               DL_APPEND(reqChunkList, reqChunk);
               
               currPeer->busy = 1;
               memcpy((void *)&currPeer->chunkProcessing, (void *)found->id, 
                     sizeof(found->id));

               result = make_whohas_ihave_get(reqChunkList, 2);
               temp_ptr = currPeer->ipPort;
               flood_all(result, &temp_ptr, 1, my_sock);
               free_packet_list(result);
               free_chunk_list(reqChunkList);
               result = NULL;
               reqChunkList = NULL;
               set_timer((double)3, 2, currPeer->ipPort, 0);
            }
            user_exists = 1;
         }
      }

      if (user_exists == 0)
      {
         log_fatal("There exist no peer for chunk:");
         print_hash_value(remPeer->chunkProcessing);
         exit_stat = 1;
         //return;
      }   
   }
   else
   {
      log_fatal("Should not come here");
   }
   clean_peer_state_recieving(remPeer);
}

/**
 * @brief removes all the TCP reliability (packet) timers for whom a cumulative
 * ack has been recieved.
 * 
 * @param peer 
 * @param seq_num_array 
 */
void remove_reliability_timers(char *peer, UT_array* seq_num_array){
   TimerTable *result;
   char temp_buffer[TIMER_ID_LEN];
   char seq_num_buf[SEQ_NUM_LEN];
   int* p;

   bzero(temp_buffer, sizeof(temp_buffer));
   bzero(seq_num_buf, sizeof(seq_num_buf));

   for(p=(int*)utarray_front(seq_num_array); p != NULL; p=(int*)utarray_next(seq_num_array,p)) 
   {
      strcpy(temp_buffer, peer);
      strcat(temp_buffer, "|");
      sprintf(seq_num_buf, "%d", *p);
      strcat(temp_buffer, seq_num_buf);
      HASH_FIND(hh, timer_table, temp_buffer, TIMER_ID_LEN, result);
      if (result != NULL)
      {
         remove_timer(peer, (u_int)(*p));
      }
      bzero(temp_buffer, sizeof(temp_buffer));
      bzero(seq_num_buf, sizeof(seq_num_buf));
      result = NULL;
   } 
}

/**
 * @brief This function is called when one or more timers in the timer table 
 * go off. This function deals with all types of timers i.e. GET, DATA, TCP and
 * Congestion timers. It either resets, modifies or removes these timers and
 * takes other relevant actions relating to the timer going off.
 * 
 * @return int 
 */
int handle_timers()
{
   log_trace("Entering Handle Timers");
   
   UT_array *seq_nums;
   TimerTable *current_timer, *tmp;
   packet_list *result = NULL;
   packet_list* retransmit_packet = NULL;
   Peer *currPeer;
   chunk_list *reqChunkList = NULL;
   chunk_list *reqChunk = NULL;
   char temp_buffer[TIMER_ID_LEN];
   char *temp_ptr;
   unsigned int num_timers;

   bzero(temp_buffer, TIMER_ID_LEN);

   num_timers = HASH_COUNT(timer_table);

   utarray_new(seq_nums, &ut_int_icd);
   // printf("there are total %u timers in table \n", num_timers);

   HASH_ITER(hh, timer_table, current_timer, tmp) 
   {
      if (difftime(time(NULL), current_timer->timestamp) >= current_timer->duration)
      {
         if (strcmp(my_ipPort, current_timer->id) == 0)
         {
            if(current_timer->type == PROCESSING)
            {
               remove_timer(my_ipPort, 0);
               return START_PROCESSING;
            }
            else
            {
               log_fatal("Got an unexpected timer");
               remove_timer(my_ipPort, 0);
            }
         }
         else
         {
            if(current_timer->type == DATA)
            {
               HASH_FIND_STR(peerHash, current_timer->id, currPeer);

               if (currPeer == NULL)
               {
                  log_fatal("Timer for peer %s exists, but peer not found in peerHash. NOT EXPECTED! RETURNING", current_timer->id);
                  return EXIT_FAILURE;
               }
               current_timer->succ_timeout_count++;
               if (current_timer->succ_timeout_count > TIME_OUT)
               {
                  /**Remove timer for peer**/
                  remove_timer(currPeer->ipPort, INVALID_SEQ_NUM);
                  sendDiffGET(currPeer);
               }
               else
               {
                  /**THINK ABOUT THIS CASE**/
                  log_trace("Reset Data timer for peer %s", current_timer->id);
                  current_timer->timestamp = time(NULL);
               }
            }
            else if(current_timer->type == GET)
            {
               HASH_FIND_STR(peerHash, current_timer->id, currPeer);

               if (currPeer == NULL)
               {
                  log_fatal("Timer for peer %s exists, but peer not found in peerHash. NOT EXPECTED! RETURNING", current_timer->id);
                  return EXIT_FAILURE;
               }
               current_timer->succ_timeout_count++;
               
               if (current_timer->succ_timeout_count <= TIME_OUT)
               {

                  reqChunk = (chunk_list *)malloc(sizeof(chunk_list));
                  memcpy((void *)&reqChunk->hash_value, 
                        (void *)currPeer->chunkProcessing, 
                        sizeof(currPeer->chunkProcessing));
                  DL_APPEND(reqChunkList, reqChunk);

                  result = make_whohas_ihave_get(reqChunkList, GET);
                  temp_ptr = currPeer->ipPort;
                  flood_all(result, &temp_ptr, ONLY_ONE_PEER, my_sock);
                  free_packet_list(result);
                  free_chunk_list(reqChunkList);
                  log_trace("Reset GET timer for peer %s Sent Request Again", current_timer->id);
                  current_timer->timestamp = time(NULL);       
               }
               else
               {
                  current_timer->succ_timeout_count = 0;
                  remove_timer(currPeer->ipPort, 0);
                  sendDiffGET(currPeer);
               }
            }
            else if(current_timer->type == TCP_RELIABILITY)
            {
               strcpy(temp_buffer, current_timer->id);
               temp_ptr = strtok(temp_buffer, "|");
               if (temp_ptr == NULL)
               { 
                 log_fatal("strtok() failed in handletimers()");
                 return EXIT_FAILURE;
               }
               HASH_FIND_STR(peerHash, temp_ptr, currPeer);
               if (currPeer == NULL)
               {
                  log_fatal("TCP timer with id %s exists for peer %s exists, but peer not found in peerHash. NOT EXPECTED! RETURNING", current_timer->id, temp_ptr);
                  return EXIT_FAILURE;
               }

               if (currPeer->LPAcked_recieved >= (u_int)current_timer->seq_num)
               {
                  remove_timer(temp_ptr, (u_int)current_timer->seq_num);
               }
               else
               {
                  /** Experienced Loss**/
                  if (currPeer->slow_start == SLOW_START_PHASE)
                  {
                     log_info("In slow start, shifting to congestion avoidance due to timer going off");
                     currPeer->slow_start = CONGESTION_AVOIDANCE_PHASE;
                     currPeer->ssthresh = max_num(currPeer->window_size/2,2);
                     currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
                     if (currPeer->rtt_val != (double)0)
                     {
                        set_timer(2 * currPeer->rtt_val, CONGESTION_AVOIDANCE_TIMER, currPeer->ipPort, CONGESTION_SEQ_NUM);
                     }
                     else
                     {
                        set_timer((double)TIMEOUT_DURATION, CONGESTION_AVOIDANCE_TIMER, currPeer->ipPort, CONGESTION_SEQ_NUM);
                     }    
                  }
                  else
                  {
                     log_info("In congestion avoidance, shifitng to slow start due to timer going off");
                     currPeer->ssthresh = max_num(currPeer->window_size/2,2);
                     currPeer->slow_start = SLOW_START_PHASE;
                     currPeer->window_size = INITIAL_WINDOW_SIZE;
                     currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
                     log_info("AZL");
                     remove_timer(currPeer->ipPort, CONGESTION_SEQ_NUM);
                  }
                  
                  utarray_push_back(seq_nums, &current_timer->seq_num);
                  retransmit_packet = make_data_packets(currPeer->chunkSending, seq_nums);
                  log_fatal("Calling it from here in timer.c");
                  flood_all(retransmit_packet, &temp_ptr, 1, my_sock);
                  free_packet_list(retransmit_packet);
                  
                  if (currPeer->rtt_val != (double)0)
                  {
                     modify_timer(temp_ptr, 2 * currPeer->rtt_val, (u_int)current_timer->seq_num);
                  }
                  else
                  {
                     log_info("here");
                     modify_timer(temp_ptr, (double)TIMEOUT_DURATION, (u_int)current_timer->seq_num);
                  
                  } 
                  
                  log_info("Retransmitted and modified timer! for %s", current_timer->id);
                  utarray_clear(seq_nums);
               }     
            }
            else if(current_timer->type == CONGESTION_AVOIDANCE_TIMER)
            {
               log_info("In Congestion Avoidance Timer");
               strcpy(temp_buffer, current_timer->id);
               temp_ptr = strtok(temp_buffer, "|");
               HASH_FIND_STR(peerHash, temp_ptr, currPeer);
               // log_info("%d", currPeer->detected_loss_or_ACK);
               if (!currPeer)
               {
                  log_fatal("peer does not exist");
               }
               else
               {
                  if (currPeer->detected_loss_or_ACK == DETECTED_ACK)
                  {
                     log_info("congestion_avoidance window incremented");
                     currPeer->window_size++;
                  }
                  
                  if (currPeer->rtt_val != (double)0)
                  {
                     modify_timer(currPeer->ipPort, 2 * currPeer->rtt_val, CONGESTION_SEQ_NUM);
                  }
                  else
                  {
                     log_info("here");
                     modify_timer(currPeer->ipPort, (double)TIMEOUT_DURATION, CONGESTION_SEQ_NUM);
                  } 
                  currPeer->detected_loss_or_ACK = NOTHING_DETECTED;
               }        
            }
            else
            {
               log_fatal("A TIMER WENT OFF WHOSE TYPE IS UNKNOWN %d", current_timer->type);
            }
         }
      }
   }
   return EXIT_SUCCESS;
}

/**
 * @brief Removes all the elements in the timer table i.e. DATA,GET, Congestion
 * and TCP reliability timers.
 * 
 */
void clear_timer_table()
{
    TimerTable *current_timer, *tmp;
    HASH_ITER(hh, timer_table, current_timer, tmp) 
    {
        HASH_DEL(timer_table, current_timer);
        free(current_timer);
    }
}