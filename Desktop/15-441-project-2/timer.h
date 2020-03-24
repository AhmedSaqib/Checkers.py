#include <stdio.h>
#include <time.h>
#include "uthash.h"

#define IP_PORT 25
#define TIMER_ID_LEN 30


#ifndef timer_h
#define timer_h

typedef struct Timers
{
   char id[TIMER_ID_LEN];
   int type;
   int seq_num;
   double duration;
   int succ_timeout_count;
   struct timeval utime;
   time_t timestamp;
   UT_hash_handle hh;
} TimerTable;

int check_if_timer_expired();
int handle_timers();
int set_timer(double duration, int type, char* peer, u_int seq_num);
int remove_timer(char* peer, u_int seq_num);
int modify_timer(char* peer, double duration, u_int seq_num);
void remove_reliability_timers(char *peer, UT_array* seq_num_array);
double timer_duration(char* peer, u_int seq_num);
void clear_timer_table();

#endif