#ifndef JSON_MESSAGES_H_
#define JSON_MESSAGES_H_

#include "SWG_device.h"
#include "GPIO_device.h"
//FUNCTION PROTOTYPES

#define JSON_LABEL_SIZE 300
#define JSON_STATUS_SIZE 1024
#define JSON_MQTT_MSG_SIZE 100

#define JSON_ON      "on"
#define JSON_OFF     "off"
#define JSON_FLASH   "flash"
#define JSON_ENABLED "enabled"

#define JSON_FAHRENHEIT "f"
#define JSON_CELSIUS    "c"
#define JSON_UNKNOWN    "u"

#define JSON_OK      "ok"
#define JSON_LOW     "low"

#define JSON_PROGRAMMING "Programming"
#define JSON_SERVICE     "Service"
#define JSON_READY       "Ready"

struct JSONkeyvalue{
  char *key;
  char *value;
};
struct JSONwebrequest {
  struct JSONkeyvalue first;
  struct JSONkeyvalue second;
  struct JSONkeyvalue third;
};

typedef struct jsontoken {
  const char *json;
  int json_len;
  char *key_ptr;
  char *val_ptr;
  int key_len; 
  int val_len;
  int pos;
} jsontoken;

/*
int build_aqualink_status_JSON(struct aqualinkdata *aqdata, char* buffer, int size);
int build_aux_labels_JSON(struct aqualinkdata *aqdata, char* buffer, int size);
bool parseJSONwebrequest(char *buffer, struct JSONwebrequest *request);
int build_mqtt_status_JSON(char* buffer, int size, int idx, int nvalue, float setpoint);
bool parseJSONmqttrequest(const char *str, size_t len, int *idx, int *nvalue, char *svalue);
int build_aqualink_error_status_JSON(char* buffer, int size, char *msg);
int build_mqtt_status_message_JSON(char* buffer, int size, int idx, int nvalue, char *svalue);
int build_homebridge_JSON(struct aqualinkdata *aqdata, char* buffer, int size);
*/
bool parseJSONwebrequest(char *buffer, struct JSONwebrequest *request);
char *jsontok(jsontoken *jt);
//int build_device_JSON(const struct apdata *aqdata, char* buffer, int size, bool homekit);
int build_device_JSON(const struct apdata *aqdata, const struct gpiodata *gpiodata, char* buffer, int size, bool homekit);
int build_swg_device_JSON(const struct apdata *aqdata, char* buffer, int size, bool homekit);
int build_gpio_device_JSON(const struct gpiodata *gpiodata, char* buffer, int size, bool homekit);
int build_dz_mqtt_status_message_JSON(char* buffer, int size, int idx, int nvalue, char *svalue);
//int build_aquapure_status_JSON(struct apdata *aqdata, char* buffer, int size);

#endif /* JSON_MESSAGES_H_ */

/*
{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}

{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}


{"type": "status","version": "8157 REV MMM","date": "09/01/16 THU","time": "1:16 PM","temp_units": "F","air_temp": "96","pool_temp": "86","spa_temp": " ","battery": "ok","pool_htr_set_pnt": "85","spa_htr_set_pnt": "99","freeze_protection": "off","frz_protect_set_pnt": "0","leds": {"pump": "on","spa": "off","aux1": "off","aux2": "off","aux3": "off","aux4": "off","aux5": "off","aux6": "off","aux7": "off","pool_heater": "off","spa_heater": "off","solar_heater": "off"}}

{"type": "aux_labels","aux1_label": "Cleaner","aux2_label": "Waterfall","aux3_label": "Spa Blower","aux4_label": "Pool Light","aux5_label": "Spa Light","aux6_label": "Unassigned","aux7_label": "Unassigned"}


{  
  "type":"aux_labels",
  "aux1_label":"Cleaner",
  "aux2_label":"Waterfall",
  "aux3_label":"Spa Blower",
  "aux4_label":"Pool Light",
  "aux5_label":"Spa Light",
  "aux6_label":"Unassigned",
  "aux7_label":"Unassigned"
}


{  
  "type":"status",
  "version":"8157 REV MMM",
  "date":"09/01/16 THU",
  "time":"1:16 PM",
  "temp_units":"F",
  "air_temp":"96",
  "pool_temp":"86",
  "spa_temp":" ",
  "battery":"ok",
  "pool_htr_set_pnt":"85",
  "spa_htr_set_pnt":"99",
  "freeze_protection":"off",
  "frz_protect_set_pnt":"0",
  "leds":{  
    "pump":"on",
    "spa":"off",
    "aux1":"off",
    "aux2":"off",
    "aux3":"off",
    "aux4":"off",
    "aux5":"off",
    "aux6":"off",
    "aux7":"off",
    "pool_heater":"off",
    "spa_heater":"off",
    "solar_heater":"off"
  }
}



*/
