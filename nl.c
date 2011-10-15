#include "nl.h"

NetStat *get_stat(size_t *count) {
  FILE *stat = fopen("/proc/net/dev", "r");
  if ( stat == NULL )
    return NULL;

  char *buf = (char *)calloc(BUFSIZE, sizeof(char));
  if ( buf == NULL ) {
    return NULL;
  }
  
  int num_of_iface = 0;
  int fscanf_result = 0;

  fseek(stat, 0, SEEK_SET);

  // skip 2 garbage lines in /proc/net/dev
  fgets(buf, BUFSIZE-1, stat);
  fgets(buf, BUFSIZE-1, stat);

  NetStat *netstat = NULL;
  size_t founded_ifacs = 0;

  while ( fgets(buf, BUFSIZE-1, stat) != NULL ) {
    char *ptr = buf;
    while ( *ptr == ' ' )
      ptr++;
    char *devname = ptr;
    while ( *ptr != ':' )
      ptr++;
    *ptr = '\0';
    ptr++;

    if ( strcmp(devname, "lo") == 0 ) // skip local loopback
      continue;

    double received_bytes, transmit_bytes;
    int dump;

    int scaned_count = sscanf(ptr, "%lg %lu %lu %d %d %d %d %d %lg %lu %lu %d %d %d %d %d",
	   &received_bytes, &dump, &dump, &dump, &dump, &dump, &dump, &dump,
	   &transmit_bytes, &dump, &dump, &dump, &dump, &dump, &dump, &dump );

    if ( scaned_count != 16 )
      continue;

    founded_ifacs++;
    netstat = (NetStat *)realloc(netstat, founded_ifacs * sizeof(NetStat));
    if ( netstat == NULL ) {
      founded_ifacs--;
      return NULL;
    }

    netstat[founded_ifacs-1].net_dev_name = (char *)calloc(strlen(devname)+1, sizeof(char));
    if ( netstat[founded_ifacs-1].net_dev_name == NULL ) {
      return NULL;
    }
    
    char *res = strcpy(netstat[founded_ifacs-1].net_dev_name, devname);
    if ( res == NULL ) {
      return NULL;
    }
    netstat[founded_ifacs-1].received_bytes = received_bytes;
    netstat[founded_ifacs-1].transmit_bytes = transmit_bytes;
  }

  fclose(stat);
  free(buf);
  *count = founded_ifacs;

  return netstat;
}

double summary_bytes(NetStat *stat, size_t stat_count) {
  int i;
  double sum_bytes = 0;
  for ( i=0; i < stat_count; i++ )
    sum_bytes += stat[i].received_bytes + stat[i].transmit_bytes;

  return sum_bytes;
}

void free_stat( NetStat *stat, size_t stat_count ) {
  int i;
  for ( i=0; i < stat_count; i++ ) {
    free(stat[i].net_dev_name);
  }
  free(stat);
}
 
/*int main(int argc, char **argv){
  size_t stats_count = 0;
  NetStat *stats = get_stat(&stats_count);

  int i;
  for ( i=0; i < stats_count; i++ ){
    printf("Network interface [%s] Received: [%lg] Transmited: [%lg]\n",
	   stats[i].net_dev_name, stats[i].received_bytes, stats[i].transmit_bytes);
  }
  double summary = summary_bytes(stats, stats_count);
  double total100 = (100 / 8) * (1024 * 1024); // * stats_count;
  printf("Percentage: %7.4f\n", summary / total100 * 100);

  free_stat(stats, stats_count);
  return 0;
  }*/
