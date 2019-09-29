#ifndef AR_CONFIG_H_
#define AR_CONFIG_H_

#include <stdbool.h>
/*
struct arconfig
{
  int PPM;
  int Percent;
  bool generating;
  unsigned char status;
  unsigned char last_status_published;
  bool ar_connected;
  char *cache_file;
};
*/
struct apdata
{
  int PPM;
  int Percent;
  bool generating;
  unsigned char status;
  unsigned char last_status_published;
  bool ar_connected;
  char *cache_file;
  bool changed;
};



void write_cache(struct apdata *ar_prms);
void read_cache(struct apdata *ar_prms);

#endif
