#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include <ctype.h>
#include "uthash.h"
#include "utlist.h"
#include "utarray.h"
#include "chunk.h"

#include "discovery.h"
#include "common.h"
#include "log.h"
#include "timer.h"



#define HEADER_LEN 16
#define MAX_CHUNKS 74
#define INITIAL_BUF_POS 4
#define MAGIC_NUM 15441
#define VERSION 1
#define DATA_SIZE 1024


extern chunkOwners* chunkOwnersHash;
extern master_chunk_table *masterChunkTableHash;
extern char *globalPath;
extern Peer *peerHash;
extern int my_sock;
extern get_file_chunk_table* getChunksHash;
extern hasChunks *hasChunksHash; 


/**
 * @brief Makes data packets.
 * 
 * @param hash_value 
 * @param seq_num_array 
 * @return packet_list* 
 */
packet_list* make_data_packets(uint8_t* hash_value, UT_array* seq_num_array){
    uint8_t hash_value_temp[SHA_HASH_LEN];
    bzero(hash_value_temp, SHA_HASH_LEN);

    master_chunk_table* find_result; 
    packet_list* final_list = NULL;
    packet_list* list_pkt = NULL;
    int* p;
    
    data_packet_t packet;
    memset(&packet, 0, sizeof(packet));

    memcpy(hash_value_temp, hash_value, SHA_HASH_LEN);
    HASH_FIND(hh, masterChunkTableHash, hash_value_temp, SHA_HASH_LEN, find_result);

    if (find_result == NULL){
        log_fatal("Coundnt find chunk in masterchunksTable");
        return NULL;
    }
    FILE* fp = fopen(globalPath, "r");
    if (fp == NULL)
    {
        log_fatal("Coundnt open masterchunks file");
        return NULL;
    }

  for(p=(int*)utarray_front(seq_num_array); p != NULL; p=(int*)utarray_next(seq_num_array,p)) 
    { 
        if (fseek(fp, (BT_CHUNK_SIZE * find_result->offset) + (DATA_SIZE * (u_int)((*p) - 1)), SEEK_SET))
        {
            log_fatal("File seek failed to seek to chunk index");
        }
        
        packet.header.magicnum = MAGIC_NUM; /** Magic Number **/
        packet.header.version = VERSION;      /** Version **/
        packet.header.packet_type = DATA;  /** ACK Packet Type **/
        packet.header.header_len = HEADER_LEN;  /** Header Length **/
        packet.header.packet_len = HEADER_LEN + DATA_SIZE;  /** Have to update this packet length as we add chunks**/
        packet.header.seq_num = (u_int)*p;      
        packet.header.ack_num = 0;      /** Ack Number doesent matter **/
        memset(packet.data, 0, DATA_LENGTH);

        if (fread(packet.data, sizeof(uint8_t), DATA_SIZE, fp) != DATA_SIZE){
            log_fatal("An error happened while reading file.");
        }

        list_pkt = (packet_list*)malloc(sizeof(packet_list));
        memset(&(list_pkt->packet), 0, sizeof(list_pkt->packet));
        memcpy((void*)(&list_pkt->packet), (void*)&packet, sizeof(packet));
        DL_APPEND(final_list, list_pkt);
        memset(&packet, 0, sizeof(packet));

    }
    fclose(fp);
    return final_list;
   }


/**
 * @brief Makes an ack or denied packet.
 * 
 * @param ack 
 * @param type 
 * @return packet_list* 
 */
packet_list* make_ack_or_denied(u_int ack, int type)
{
    data_packet_t return_packet;
    packet_list* final_list = NULL;
    packet_list* list_pkt = NULL;

    memset(&return_packet, 0, sizeof(data_packet_t));

    /** Making Headers **/
    return_packet.header.magicnum = MAGIC_NUM; /** Magic Number **/
    return_packet.header.version = VERSION;      /** Version **/
    return_packet.header.header_len = HEADER_LEN;  /** Header Length **/
    return_packet.header.packet_len = HEADER_LEN;  /** Have to update this packet length as we add chunks**/
    return_packet.header.seq_num = 0;      /** Sequence Number doesent matter **/ 
    
    if (type == ACK)
    {
        return_packet.header.packet_type = ACK;  /** ACK Packet Type **/
        return_packet.header.ack_num = ack;      
    }
    else
    {
        return_packet.header.packet_type = DENIED;  /** ACK Packet Type **/
        return_packet.header.ack_num = 0;      
    }
    memset(return_packet.data, 0, DATA_LENGTH);

    /**Add To List**/
    list_pkt = (packet_list*)malloc(sizeof(packet_list));

    memset(&(list_pkt->packet), 0, sizeof(list_pkt->packet));
    memcpy((void*)(&list_pkt->packet), (void*)&return_packet, sizeof(return_packet));
    DL_APPEND(final_list, list_pkt);

    return final_list;
}


/**
 * @brief Makes WHOHAS, IHAVE, and GET requests based on type parameter,
 *        Expects a chunklist as argument.
 * 
 * @param chunkList 
 * @param type 
 * @return packet_list* 
 */ 
packet_list* make_whohas_ihave_get(chunk_list* chunkList, int type)
{
    int num_chunks = 0;
    int buf_pos = 0;
    char* data_ptr = NULL;
    chunk_list* element;
    packet_list* final_list = NULL;
    packet_list* list_pkt = NULL;
    data_packet_t packet;
    bool all_sent = false;

    
    memset(&packet, 0, sizeof(packet));
    
    /** Making Headers **/
    packet.header.magicnum = MAGIC_NUM; /** Magic Number **/
    packet.header.version = VERSION;      /** Version **/
    packet.header.packet_type = type;  /** Packet Type **/
    packet.header.header_len = HEADER_LEN;  /** Header Length **/
    packet.header.packet_len = HEADER_LEN;  /** Have to update this packet length as we add chunks**/
    packet.header.seq_num = 0;      /** Sequence Number doesent matter **/ 
    packet.header.ack_num = 0;      /** Ack Number doesent matter **/

    data_ptr = packet.data;
    buf_pos = INITIAL_BUF_POS;

    if (type == GET){
        buf_pos = 0;
    }
    memset(data_ptr, 0, DATA_LENGTH);
    
    DL_FOREACH(chunkList, element){
        all_sent = false;
        memcpy(data_ptr + buf_pos, element->hash_value, SHA_HASH_LEN);
        buf_pos = buf_pos + SHA_HASH_LEN;
        num_chunks++;

        if (num_chunks == MAX_CHUNKS && type != GET){
            num_chunks = 0;
            buf_pos = INITIAL_BUF_POS;
            data_ptr[0] = MAX_CHUNKS;
            packet.header.packet_len = HEADER_LEN + 4 + (MAX_CHUNKS * SHA_HASH_LEN);
            
            /**Add To List**/
            list_pkt = (packet_list*)malloc(sizeof(packet_list));

            memset(&(list_pkt->packet), 0, sizeof(list_pkt->packet));
            memcpy((void*)(&list_pkt->packet), (void*)&packet, sizeof(packet));
            DL_APPEND(final_list, list_pkt);

            memset(data_ptr, 0, DATA_LENGTH);
            all_sent = true;
        } 
    }

    if (all_sent == false)
    {
        if (type != GET)
        {
            data_ptr[0] = num_chunks;
            packet.header.packet_len = HEADER_LEN + 4 + (num_chunks * SHA_HASH_LEN);
        }
        else
        {
            packet.header.packet_len = HEADER_LEN +  SHA_HASH_LEN;
        }

        /**Add To List**/
        list_pkt = (packet_list*)malloc(sizeof(packet_list));
        memset(&(list_pkt->packet), 0, sizeof(list_pkt->packet));
        memcpy((void*)(&list_pkt->packet), (void*)&packet, sizeof(packet));
        DL_APPEND(final_list, list_pkt);

        memset(data_ptr, 0, DATA_LENGTH);
        all_sent = true;
    }
    return final_list;
}


/**
 * @brief Prints hash value of chunkhash
 * 
 * @param hash_value 
 */
void print_hash_value(uint8_t* hash_value){
    size_t i;
    for (i = 0; i < SHA_HASH_LEN; i++)
    {
       fprintf(stdout, "%02x", hash_value[i]);
    }
    fprintf(stdout, "\n");
}

/**
 * @brief Prints hash value of chunkhash into buffer passed in
 * 
 * @param hash_value 
 */
void print_hash_value_into_buf(uint8_t* hash_value, char* buf){
    char temp_bfr[5];
    bzero(temp_bfr, 5);
    size_t i;
    for (i = 0; i < SHA_HASH_LEN; i++)
    {
       sprintf(temp_bfr, "%02x", hash_value[i]);
       strcat(buf, temp_bfr);
       bzero(temp_bfr, 5);
    }
}




/**
 * @brief Converts packet from host to network byte ordering
 * 
 * @param packet 
 */
void to_network_order(data_packet_t* packet){
    packet->header.magicnum = htons(packet->header.magicnum);
    packet->header.header_len = htons(packet->header.header_len);
    packet->header.packet_len = htons(packet->header.packet_len);
    packet->header.seq_num = htonl(packet->header.seq_num);
    packet->header.ack_num = htonl(packet->header.ack_num);

}

/**
 * @brief Converts packet from network to host byte ordering
 * 
 * @param packet 
 */
void to_normal_order(data_packet_t* packet) {
    packet->header.magicnum = ntohs(packet->header.magicnum);
    packet->header.header_len = ntohs(packet->header.header_len);
    packet->header.packet_len = ntohs(packet->header.packet_len);
    packet->header.seq_num = ntohl(packet->header.seq_num);
    packet->header.ack_num = ntohl(packet->header.ack_num);
}


/**
 * @brief Find the type of packet just recieved.
 * 
 * @param type 
 * @param buf 
 */
void packet_type_store_in_buf(int type, char* buf)
{
    switch (type)
    {
    case 0:
        strcat(buf, "WHOHAS");
        break;
    case 1:
        strcat(buf, "IHAVE");
        break;
    case 2:
        strcat(buf, "GET");
        break;    
    case 3:
        strcat(buf, "DATA");
        break;    
    case 4:
        strcat(buf, "ACK");
        break;    
    case 5:
        strcat(buf, "DENIED");
        break;     
    case 6:
        strcat(buf, "PROCESSING");
        break;  
    case 7:
        strcat(buf, "TCP_RELIABILITY");
        break; 
    case 8:
        strcat(buf, "CONGESTION_AVOIDANCE_TIMER");
        break; 
    default:
        strcat(buf, "DONTKNOW");
        break;
    }
}

/**
 * @brief Sends all packets passed in the packetlist, to 
 *        all peers in the peerlist. No reliability
 * 
 * @param packets 
 * @param peer_list 
 * @param num_peers 
 * @param socket 
 */
void flood_all(packet_list* packets, char** peer_list, int num_peers, int socket)
{
    packet_list* elem;
    struct sockaddr_in toaddr;
    int length_peer_list;
    char *temp, *temp1;
    char temp_peer[IP_PORT];
    char packet_type[PACKET_NAME_SIZE];
    bzero(temp_peer, IP_PORT);
    bzero(packet_type, sizeof(packet_type));
    // int iterator = 0;
    
    if (peer_list == NULL){
        log_fatal("Peer list was empty, not expected to be empty");
        return;
    }

    length_peer_list = num_peers;

    /** Use this to test for UDP's shitty service: DL_REVFOREACH(packets, elem, iterator)**/
    DL_FOREACH(packets, elem)
    {
        to_network_order(&elem->packet);
        size_t i;
        for (i = 0; i < length_peer_list; i++)
        {
            packet_type_store_in_buf((int)elem->packet.header.packet_type, packet_type);
            strcat(temp_peer, peer_list[i]);
            temp = strtok(temp_peer, ":");
            temp1 = strtok(NULL, "");
            log_trace("Sending packet type %s Seq Num: %u Ack Num: %u to IP: %s PORT: %s", packet_type, ntohl(elem->packet.header.seq_num) , ntohl(elem->packet.header.ack_num) ,temp, temp1);
            inet_aton(temp, &toaddr.sin_addr);
            toaddr.sin_port = htons(atoi(temp1));
            toaddr.sin_family = AF_INET;
            spiffy_sendto(socket, &elem->packet, sizeof(elem->packet), 0, (struct sockaddr *) &toaddr, sizeof(toaddr));
            bzero(temp_peer, IP_PORT);
            bzero(packet_type, sizeof(packet_type));

        }
        to_normal_order(&elem->packet);
    }
}

/**
 * @brief Frees the packet list returned by make_whohas_ihave_get
 * 
 * @param head 
 */
void free_packet_list(packet_list* head){
    packet_list* delPkt = NULL;
    packet_list* temp_chunk;
    DL_FOREACH_SAFE(head,delPkt,temp_chunk) 
    {
      DL_DELETE(head,delPkt);
      free(delPkt);
    }
    if (head != NULL){
        free(head);
    }
}

/**
 * @brief Frees chunk list used as input to make_whohas_ihave_get
 * 
 * @param head 
 */
void free_chunk_list(chunk_list* head){
    chunk_list* delPkt = NULL;
    chunk_list* temp_chunk;
    DL_FOREACH_SAFE(head,delPkt,temp_chunk) 
    {
      DL_DELETE(head,delPkt);
      free(delPkt);
    }

    if (head != NULL){
        free(head);
    }
}

/**
 * @brief Frees the list of peers who own a chunk in the chunkOwnersHash Table
 * 
 * @param head 
 */
void free_peer_list(peer_list* head){
    peer_list* delPkt = NULL;
    peer_list* temp_chunk;
    DL_FOREACH_SAFE(head,delPkt,temp_chunk) 
    {
      DL_DELETE(head,delPkt);
      free(delPkt);
    }

    if (head != NULL){
        free(head);
    }
}

/**
 * @brief Clears the seqNumHash table for a peer
 * 
 * @param hash_tb 
 */
void clean_seqNumHash(seq_num * hash_tb)
{
    seq_num *current, *tmp;
    HASH_ITER(hh, hash_tb, current, tmp) 
    {
        HASH_DEL(hash_tb, current);
        free(current); 
    }
}

/**
 * @brief clears the hasChunksHash Table.
 * 
 */
void clean_hasChunksHash()
{
    hasChunks *current, *tmp;
    HASH_ITER(hh, hasChunksHash, current, tmp) 
    {
        HASH_DEL(hasChunksHash, current);
        free(current); 
    }
}

/**
 * @brief Clears the mastChunkTableHash table.
 * 
 */
void clean_masterChunkTableHash()
{
    master_chunk_table *current, *tmp;
    HASH_ITER(hh, masterChunkTableHash, current, tmp) 
    {
        HASH_DEL(masterChunkTableHash, current);
        free(current); 
    }  
}



/**
 * @brief Clears the receiving peer's part of state
 * 
 * @param currPeer 
 */
void clean_peer_state_recieving(Peer* currPeer){
    seq_num *current, *tmp;
    currPeer->busy = PEER_NOT_BUSY;
    currPeer->LPAcked_sent = 0;
    currPeer->NextExpectedPacket = 1;
    currPeer->maxSeq = 0;
    currPeer->num_bytes_recieved = 0;
    bzero(currPeer->chunkProcessing, HASH_SIZE);
    HASH_ITER(hh, currPeer->seqNumHash, current, tmp) 
    {
        HASH_DEL(currPeer->seqNumHash, current);
        free(current); 
    }
    currPeer->seqNumHash = NULL;
    return;
}

/**
 * @brief makes the chunk owners hash table where key is a hash and value is
 * the list of peers who own that chunk. 
 * 
 * @param owned_chunks 
 * @param owner 
 */
void make_chunk_owners(chunk_list *owned_chunks, char *owner)
{
    chunk_list *elem;
    peer_list *owner_peers = NULL;
    struct Owners *singleChunk = NULL;
    struct Owners *found = NULL;
    peer_list *newList = NULL;
    char temp_hash_buffer[100];
    bzero(temp_hash_buffer, sizeof(temp_hash_buffer));

    DL_FOREACH(owned_chunks, elem)
    {
        singleChunk = (struct Owners *)malloc(sizeof(struct Owners));
        
        memcpy((void *)&singleChunk->id, (void *)&elem->hash_value, 
               sizeof(elem->hash_value));
        owner_peers = (peer_list *)malloc(sizeof(peer_list));
        
        memset(&(owner_peers->hash_value), 0, sizeof(owner_peers->hash_value));
        memcpy((void*)(&owner_peers->hash_value), (void*)owner, strlen(owner));
        
        print_hash_value_into_buf(elem->hash_value, temp_hash_buffer);
        if(!chunkOwnersHash)
        {
            log_trace("Table is empty, adding hash: %s", temp_hash_buffer);
            DL_APPEND(newList, owner_peers);
            singleChunk->ownerList = newList;
            HASH_ADD(hh, chunkOwnersHash, id, sizeof(elem->hash_value), 
            singleChunk);
        }
        else
        {
            HASH_FIND(hh, chunkOwnersHash, elem->hash_value, 
                    sizeof(elem->hash_value), found);
            if(!found)
            {
                log_trace("Table was not empty. Didnt find hash: %s ADDED NOW", temp_hash_buffer);
                DL_APPEND(newList, owner_peers);
                singleChunk->ownerList = newList;
                HASH_ADD(hh, chunkOwnersHash, id, sizeof(elem->hash_value), 
                singleChunk);
            }
            else
            {
                DL_APPEND(found->ownerList, owner_peers);
            }        
        }
        singleChunk = NULL;
        owner_peers = NULL;   
        newList = NULL;
        bzero(temp_hash_buffer, sizeof(temp_hash_buffer));
    }
}

/**
 * @brief sends packets to a peer where num of packets is at most sliding window 
 * size
 * 
 * @param hash_value 
 * @param peer 
 * @return int 
 */
int sliding_window_send(uint8_t* hash_value, char* peer){
    UT_array *seq_nums;
    Peer* currPeer;
    packet_list* data_packets;
    char* temp_ptr;
    int iterator;

    utarray_new(seq_nums, &ut_int_icd);
    HASH_FIND_STR(peerHash, peer, currPeer);

    if (currPeer == NULL)
    {
        log_fatal("Could not find peer to send DATA packets to, NOT EXPECTED");
        return EXIT_FAILURE;
    }

    for(iterator=currPeer->LPSent+1; iterator < currPeer->LPAvailable+1; iterator++) 
    {
        utarray_push_back(seq_nums,&iterator);
        //sent_something = true;
        if (currPeer->rtt_val != (double)0)
        {
            set_timer(2 * currPeer->rtt_val, TCP_RELIABILITY, peer, (u_int)iterator);
        }
        else
        {
            set_timer((double)TIMEOUT_DURATION, TCP_RELIABILITY, peer, (u_int)iterator);
        }   
    }
    data_packets= make_data_packets(hash_value, seq_nums);
    temp_ptr = peer;

    if (data_packets == NULL)
    {
        log_warn("There were no packets sent in Sliding_Send. This is ok dw.");
    }
    else
    {
        flood_all(data_packets, &temp_ptr, 1, my_sock);
        currPeer->LPSent = currPeer->LPAvailable;
        free_packet_list(data_packets);
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Removes a chunks from the chunkOwnersHash table
 * 
 * @param hash_value 
 * @return int 
 */
int remove_from_chunkOwnersHash(uint8_t* hash_value)
{
    chunkOwners* found = NULL;
    HASH_FIND(hh, chunkOwnersHash, hash_value, HASH_SIZE, found);
    
    if (found == NULL)
    {
        return EXIT_FAILURE;
    }
    free_peer_list(found->ownerList);
    HASH_DEL(chunkOwnersHash, found);
    free(found);
    return EXIT_SUCCESS;    
}


/**
 * @brief Clears the peerHash Table
 * 
 */
void clean_peerHash()
{
    Peer *current, *tmp;
    HASH_ITER(hh, peerHash, current, tmp) 
    {
        clean_seqNumHash(current->seqNumHash);
        HASH_DEL(peerHash, current);
        free(current); 
    }      
}

/**
 * @brief Clears the chunkOwnersHash table
 * 
 */
void clear_chunkOwnersHash()
{
    chunkOwners *current_chunk, *tmp;
    HASH_ITER(hh, chunkOwnersHash, current_chunk, tmp) 
    {
        free_peer_list(current_chunk->ownerList);
        HASH_DEL(chunkOwnersHash, current_chunk);
        free(current_chunk);
    }
}

/**
 * @brief Clears the getChunksHash table
 * 
 */
void clear_getChunksHash()
{
    get_file_chunk_table *current_chunk, *tmp;
    
    HASH_ITER(hh, getChunksHash, current_chunk, tmp) 
    {
        HASH_DEL(getChunksHash, current_chunk);
        free(current_chunk);
    }
}

