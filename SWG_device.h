#ifndef SWG_DEVICE_H_
#define SWG_DEVICE_H_

#include <stdbool.h>


#ifndef SWG_DEVICE_C_
const extern struct apdata _apdata_;
#endif

#define DISMSGLEN 30

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
  char display_message[DISMSGLEN+1]; // Need to move this to a generic data, not device data
};


//bool set_SWG_percent(int percent);
void init_swg_device(bool forceConnection);
void set_swg_uptodate();
int prepare_swg_message(unsigned char *packet_buffer, int packet_length);
void action_swg_message(unsigned char *packet_buffer, int packet_length);
void set_swg_req_percent(char *sval, bool f2c);
void set_swg_percent(int percent);
void set_swg_boost(bool val);
void set_swg_on(bool val);
bool action_boost_request(char *value);

void write_swg_cache();
void read_swg_cache();
void set_display_message(char *msg);

#endif
