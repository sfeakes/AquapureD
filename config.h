
#ifndef CONFIG_H_
#define CONFIG_H_

#include "utils.h"
#include "aq_serial.h"
#include "aqualink.h"


#define DEFAULT_LOG_LEVEL    10 
#define DEFAULT_WEBPORT      "80"
#define DEFAULT_WEBROOT      "./"
#define DEFAULT_SERIALPORT   "/dev/ttyUSB0"
#define DEFAULT_DEVICE_ID    "0x0a"
#define DEFAULT_MQTT_DZ_IN   NULL
#define DEFAULT_MQTT_DZ_OUT  NULL
#define DEFAULT_MQTT_AQ_TP   NULL
#define DEFAULT_MQTT_SERVER  NULL
#define DEFAULT_MQTT_USER    NULL
#define DEFAULT_MQTT_PASSWD  NULL

#define MQTT_ID_LEN 20


#ifndef CONFIG_C_
const extern struct apconfig _apconfig_;
#endif

struct apconfig
{
  char name[20];
  char web_directory[512];
  char log_file[512];
  char serial_port[50];
  char socket_port[6];

  char mqtt_server[20];
  char mqtt_user[50];
  char mqtt_passwd[50];
  char mqtt_topic[50];
  char mqtt_dz_sub_topic[50];
  char mqtt_dz_pub_topic[50];
  char mqtt_ID[MQTT_ID_LEN];

  int dzidx_swg_status_msg;

  //char *version;
  //char *name;
  //char *serial_port;
  unsigned int log_level;
  //char *socket_port;
  //char *web_directory;
  unsigned char device_id;
  bool deamonize;
  
  //char *mqtt_dz_sub_topic;
  //char *mqtt_dz_pub_topic;
  //char *mqtt_aq_topic;
  //char *mqtt_server;
  //char *mqtt_user;
  //char *mqtt_passwd;
  //char mqtt_ID[MQTT_ID_LEN+1];
  //char last_display_message[AQ_MSGLONGLEN+1];
  //int dzidx_air_temp;
  //int dzidx_pool_water_temp;
  //int dzidx_spa_water_temp;
  //int dzidx_swg_percent;
  //int dzidx_swg_ppm;
  //int dzidx_swg_status;
  //float light_programming_mode;
  //bool override_freeze_protect;
  //bool pda_mode;
  bool convert_mqtt_temp;
  int temp_units;
  //bool convert_dz_temp;
  //int dzidx_pool_thermostat; // Domoticz virtual thermostats are crap removed until better
  //int dzidx_spa_thermostat;  // Domoticz virtual thermostats are crap removed until better
  //char mqtt_pub_topic[250];
  //char *mqtt_pub_tp_ptr = mqtt_pub_topic[];
};

void init_parameters (bool deamonize);
//bool parse_config (struct aqconfig * parms, char *cfgfile);
//void readCfg (struct aqconfig *config_parameters, char *cfgFile);
void readCfg (char *cfgFile);

#endif