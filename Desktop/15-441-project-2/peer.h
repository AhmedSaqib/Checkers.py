#ifndef PEER_H
#define PEER_H

#include "uthash.h"

typedef struct Owners
{
    char id[20];
    char *ownerList;
    UT_hash_handle hh;
} chunkOwners;

typedef struct Chunks
{
   char id[20];
   int index;
   UT_hash_handle hh;
} hasChunks;


typedef struct Peers 
{
   char ipAddress[100];
   char port[10];
} Peer;


#endif // PEER_H ///:~