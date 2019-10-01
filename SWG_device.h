#ifndef AR_CONFIG_H_
#define AR_CONFIG_H_

#include <stdbool.h>


// connected, receiving ACK
// status, status from RS485  (on & generating are same status)
//             Percent > 0 & status = 0 to generating.
//             Percent = 0 & status = 0x00 change status to 0xFF (off)

struct apdata
{
  int PPM;
  int Percent;
  int last_generating_percent;
  int default_percent;
  //bool generating;
  bool boost;
  unsigned char status;
  //unsigned char last_status_published;
  bool connected;
  char *cache_file;
  bool changed;
};


bool set_SWG_percent(int percent);

void write_cache(struct apdata *ar_prms);
void read_cache(struct apdata *ar_prms);

#endif
