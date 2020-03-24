#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "common.h"
#include "utarray.h"

packet_list* make_whohas_ihave_get(chunk_list* chunkList, int type);
void print_hash_value(uint8_t* hash_value);
void print_hash_value_into_buf(uint8_t* hash_value, char* buf);
void flood_all(packet_list* packets, char** peer_list, int num_peers, int socket);
void free_packet_list(packet_list* head);
void free_chunk_list(chunk_list* head);
void make_chunk_owners(chunk_list *owned_chunks, char *owner);
void packet_type_store_in_buf(int type, char* buf);
packet_list* make_data_packets(uint8_t* hash_value, UT_array* seq_num_array);
packet_list* make_ack_or_denied(u_int ack, int type);
int sliding_window_send(uint8_t* hash_value, char* peer);
void clean_peer_state_recieving(Peer* currPeer);
int remove_from_chunkOwnersHash(uint8_t* hash_value);
void clear_chunkOwnersHash();
void clear_getChunksHash();
void clean_peerHash();
void clean_hasChunksHash();
void clean_masterChunkTableHash();
#endif /* DISCOVERY_H */