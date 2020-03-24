#ifndef COMMON_H
#define COMMON_H

#include "utlist.h"
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define DATA_LENGTH 1484
#define SHA_HASH_LEN 20
#define HASH_SIZE 20
#define IP_PORT 25
#define IP_ADDRESS 100
#define PORT 10
#define PACKET_NAME_SIZE 50
#define INVALID_SEQ_NUM 0
#define CONGESTION_SEQ_NUM -1
#define ONLY_ONE_PEER 1
#define TIMEOUT_DURATION 3
#define TCP_RELIABILITY_TIMEOUT 5
#define PEER_IS_BUSY 1
#define PEER_NOT_BUSY 0
#define SLOW_START_PHASE 1
#define CONGESTION_AVOIDANCE_PHASE 0
#define INITIAL_SSTHRESH 64
#define INITIAL_WINDOW_SIZE 1
#define TIMER_DOES_NOT_EXIST -1
#define max_num(a,b)            (((a) > (b)) ? (a) : (b))

#define PACKETLEN 1500
#define WHO_HAS 0
#define I_HAVE 1
#define GET 2
#define DATA 3
#define ACK 4
#define DENIED 5
#define PROCESSING 6
#define TCP_RELIABILITY 7
#define CONGESTION_AVOIDANCE_TIMER 8
#define DETECTED_LOSS -1
#define DETECTED_ACK 1
#define NOTHING_DETECTED 0


/**
 * @brief Defines a node in the doubly-linked list that 
 *        contains chunk hash information
 * 
 */
typedef struct ChunkList 
{
    uint8_t hash_value[20];
    struct ChunkList *prev; 
    struct ChunkList *next; 
} chunk_list;


typedef struct PeerList 
{
    char hash_value[25];
    struct PeerList *prev; 
    struct PeerList *next; 
} peer_list;


typedef struct Owners
{
    uint8_t id[HASH_SIZE];
    peer_list *ownerList;
    UT_hash_handle hh;
} chunkOwners;

/**
 * @brief Defines a UDP packet header
 * 
 */
typedef struct header_s 
{
  short magicnum;
  char version;
  char packet_type;
  short header_len;
  short packet_len; 
  u_int seq_num;
  u_int ack_num;
} header_t;  

/**
 * @brief Defines a complete UDP packet
 *        including header and data. Size is 1500.
 * 
 */
typedef struct data_packet 
{
  header_t header;
  char data[DATA_LENGTH]; 
} data_packet_t;

/**
 * @brief Defines a node in the doubly-linked list that 
 *        contains data packets.
 * 
 */
typedef struct packet 
{
    data_packet_t packet;
    struct packet *prev; 
    struct packet *next;
} packet_list;

typedef struct seqNum
{
  u_int seqNum;
  u_int data[PACKETLEN];
  int num_bytes;
  UT_hash_handle hh;
} seq_num;
 
typedef struct Peers 
{
   char ipPort[IP_PORT];
   char ipAddress[IP_ADDRESS];
   char port[PORT];
   int busy;
   int dup_acks_recieved;
   int num_bytes_recieved;
   int window_size;
   int slow_start;
   int ssthresh;
   double rtt_val;
   int num_rtt_samples;
   int detected_loss_or_ACK;
   uint8_t chunkProcessing[HASH_SIZE];
   uint8_t chunkSending[HASH_SIZE];
   u_int LPAcked_recieved;
   u_int LPAvailable;
   u_int LPSent;
   u_int LPAcked_sent;
   u_int maxSeq;
   u_int NextExpectedPacket;
   seq_num *seqNumHash;
   UT_hash_handle hh;
} Peer;

typedef struct MasterChunkTable
{
  uint8_t id[HASH_SIZE];
  int offset;
  UT_hash_handle hh;
} master_chunk_table;


typedef struct GetFileChunkTable
{
  uint8_t id[HASH_SIZE];
  int offset;
  int written;
  UT_hash_handle hh;
} get_file_chunk_table;

typedef struct Chunks
{
   uint8_t id[HASH_SIZE];
   int index;
   UT_hash_handle hh;
} hasChunks;
#endif // COMMON_H ///:~

