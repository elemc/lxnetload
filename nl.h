#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 256

typedef struct {
  char *net_dev_name;
  double received_bytes;
  double transmit_bytes;
} NetStat;

NetStat *get_stat(size_t *count);
double summary_bytes(NetStat *stat, size_t stat_count);
void free_stat( NetStat *stat, size_t stat_count); 
