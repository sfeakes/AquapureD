/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the 
 * Free Software Foundation. For the terms of this license, 
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "aqualink.h"
#include "config.h"
//#include "aq_programmer.h"
#include "utils.h"
//#include "web_server.h"
#include "json_messages.h"
//#include "domoticz.h"
#include "aq_mqtt.h"
#include "ap_config.h"
#include "version.h"


//#define test_message "{\"type\": \"status\",\"version\": \"8157 REV MMM\",\"date\": \"09/01/16 THU\",\"time\": \"1:16 PM\",\"temp_units\": \"F\",\"air_temp\": \"96\",\"pool_temp\": \"86\",\"spa_temp\": \" \",\"battery\": \"ok\",\"pool_htr_set_pnt\": \"85\",\"spa_htr_set_pnt\": \"99\",\"freeze_protection\": \"off\",\"frz_protect_set_pnt\": \"0\",\"leds\": {\"pump\": \"on\",\"spa\": \"off\",\"aux1\": \"off\",\"aux2\": \"off\",\"aux3\": \"off\",\"aux4\": \"off\",\"aux5\": \"off\",\"aux6\": \"off\",\"aux7\": \"off\",\"pool_heater\": \"off\",\"spa_heater\": \"off\",\"solar_heater\": \"off\"}}"
//#define test_labels "{\"type\": \"aux_labels\",\"aux1_label\": \"Cleaner\",\"aux2_label\": \"Waterfall\",\"aux3_label\": \"Spa Blower\",\"aux4_label\": \"Pool Light\",\"aux5_label\": \"Spa Light\",\"aux6_label\": \"Unassigned\",\"aux7_label\": \"Unassigned\"}"

//#define test_message "{\"type\": \"status\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"on\",\"Spa_Mode\": \"on\",\"Aux_1\": \"on\",\"Aux_2\": \"on\",\"Aux_3\": \"on\",\"Aux_4\": \"on\",\"Aux_5\": \"on\",\"Aux_6\": \"on\",\"Aux_7\": \"on\",\"Pool_Heater\": \"on\",\"Spa_Heater\": \"on\",\"Solar_Heater\": \"on\"}}"

//{"type": "aux_labels","Pool Pump": "Pool Pump","Spa Mode": "Spa Mode","Cleaner": "Aux 1","Waterfall": "Aux 2","Spa Blower": "Aux 2","Pool Light": "Aux 4","Spa Light ": "Aux 5","Aux 6": "Aux 6","Aux 7": "Aux 7","Heater": "Heater","Heater": "Heater","Solar Heater": "Solar Heater","(null)": "(null)"}

const char* SWGstatus2test(unsigned char status)
{
  switch (status) {
    case SWG_STATUS_OFF:
      return "Off";
    break;
    case SWG_STATUS_OFFLINE:
      return "Offline";
    break;
    case SWG_STATUS_ON:
      return "Generating Salt";
    break;
    case SWG_STATUS_NO_FLOW:
      return "No flow";
    break;
    case SWG_STATUS_LOW_SALT:
      return "Low Salt";
    break;
    case SWG_STATUS_HI_SALT:
      return "High Salt";
    break;
    case SWG_STATUS_CLEAN_CELL:
      return "Clean Cell";
    break;
    case SWG_STATUS_TURNING_OFF:
      return "Turning Off";
    break;
    case SWG_STATUS_HIGH_CURRENT:
      return "High Current";
    break;
    case SWG_STATUS_LOW_VOLTS:
      return "Low Volts";
    break;
    case SWG_STATUS_LOW_TEMP:
      return "Low Temp";
    break;
    case SWG_STATUS_CHECK_PCB:
      return "Check PCB";
    break;
  }
  return "";
}

const char* getStatus(struct aqualinkdata *aqdata)
{
  if (aqdata->active_thread.thread_id != 0) {
    return JSON_PROGRAMMING;
  }
 
  if (aqdata->last_message != NULL && stristr(aqdata->last_message, "SERVICE") != NULL ) {
    return JSON_SERVICE;
  }

  return JSON_READY; 
}

/*
int build_mqtt_status_JSON(char* buffer, int size, int idx, int nvalue, float tvalue)
{
  memset(&buffer[0], 0, size);
  int length = 0;

  if (tvalue == TEMP_UNKNOWN) {
    length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"\"}", idx, nvalue);
  } else {
    length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"stype\":\"SetPoint\",\"svalue\":\"%.2f\"}", idx, nvalue, tvalue);
  }

  buffer[length] = '\0';
  return strlen(buffer);
}

int build_mqtt_status_message_JSON(char* buffer, int size, int idx, int nvalue, char *svalue)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  //json.htm?type=command&param=udevice&idx=IDX&nvalue=LEVEL&svalue=TEXT

  length = sprintf(buffer, "{\"idx\":%d,\"nvalue\":%d,\"svalue\":\"%s\"}", idx, nvalue, svalue);

  buffer[length] = '\0';
  return strlen(buffer);
}
*/
int build_aqualink_error_status_JSON(char* buffer, int size, char *msg)
{
  //return snprintf(buffer, size, "{\"type\": \"error\",\"status\":\"%s\"}", msg);
  return snprintf(buffer, size, "{\"type\": \"status\",\"status\":\"%s\",\"version\":\"xx\",\"time\":\"xx\",\"air_temp\":\"0\",\"pool_temp\":\"0\",\"spa_temp\":\"0\",\"pool_htr_set_pnt\":\"0\",\"spa_htr_set_pnt\":\"0\",\"frz_protect_set_pnt\":\"0\",\"temp_units\":\"f\",\"battery\":\"ok\",\"leds\":{\"Filter_Pump\": \"off\",\"Spa_Mode\": \"off\",\"Aux_1\": \"off\",\"Aux_2\": \"off\",\"Aux_3\": \"off\",\"Aux_4\": \"off\",\"Aux_5\": \"off\",\"Aux_6\": \"off\",\"Aux_7\": \"off\",\"Pool_Heater\": \"off\",\"Spa_Heater\": \"off\",\"Solar_Heater\": \"off\"}}", msg);

}


//int build_status_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
int build_device_JSON(struct apdata *aqdata, char* buffer, int size, bool homekit)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  //int i;

  bool homekit_f = (homekit && _apconfig_.temp_units==FAHRENHEIT);

  length += sprintf(buffer+length, "{\"type\": \"devices\"");
  length += sprintf(buffer+length, ",\"version\":\"%s\"",AQUAPURED_VERSION );
  length += sprintf(buffer+length, ",\"name\":\"%s\"",AQUAPURED_NAME );
  length += sprintf(buffer+length, ",\"fullstatus\":\"%s\"",SWGstatus2test(aqdata->status));
  length += sprintf(buffer+length,  ", \"devices\": [");
  
  //if ( aqdata->Percent != TEMP_UNKNOWN ) {
    length += sprintf(buffer+length, "{\"type\": \"setpoint_swg\", \"id\": \"%s\", \"setpoint_id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"status\": \"%s\", \"spvalue\": \"%.*f\", \"value\": \"%.*f\", \"extended_status\": \"%d\" },",
                                    SWG_TOPIC,
                                    ((homekit_f)?SWG_PERCENT_F_TOPIC:SWG_PERCENT_TOPIC),
                                    "Salt Water Generator",
                                    aqdata->status == SWG_STATUS_ON?JSON_ON:JSON_OFF,
                                    aqdata->status == SWG_STATUS_ON?JSON_ON:JSON_OFF,
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->Percent):aqdata->Percent),
                                    ((homekit)?2:0),
                                    ((homekit_f)?degFtoC(aqdata->Percent):aqdata->Percent),
                                    aqdata->status);

    //length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
    length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   ((homekit_f)?SWG_PERCENT_F_TOPIC:SWG_PERCENT_TOPIC),
                                   "Salt Water Generator Percent",
                                   "on",
                                   ((homekit_f)?2:0),
                                   ((homekit_f)?degFtoC(aqdata->Percent):aqdata->Percent));
  //}

  if ( aqdata->PPM != TEMP_UNKNOWN ) {

    length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%.*f\" },",
                                   ((homekit_f)?SWG_PPM_F_TOPIC:SWG_PPM_TOPIC),
                                   "Salt Level PPM",
                                   "on",
                                   ((homekit)?2:0),
                                   ((homekit_f)?roundf(degFtoC(aqdata->PPM)):aqdata->PPM)); 
                                   
                                  /*
   length += sprintf(buffer+length, "{\"type\": \"value\", \"id\": \"%s\", \"name\": \"%s\", \"state\": \"%s\", \"value\": \"%d\" },",
                                   SWG_PPM_TOPIC,
                                   "Salt Level PPM",
                                   "on",
                                   aqdata->swg_ppm);
                                   */
  }

  if (buffer[length-1] == ',')
    length--;

  length += sprintf(buffer+length, "]}");

  logMessage(LOG_DEBUG, "build_device_JSON %d of %d", length, size);
 

  buffer[length] = '\0';

  logMessage(LOG_DEBUG, "-->%s<--", buffer);

  return strlen(buffer);

}

/*
int build_aquapure_status_JSON(struct apdata *aqdata, char* buffer, int size)
{
  //strncpy(buffer, test_message, strlen(test_message)+1);
  //return strlen(test_message);
  //char buffer2[600];
  
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;

  length += sprintf(buffer+length, "{\"type\": \"status\"");
  length += sprintf(buffer+length, ",\"version\":\"%s\"",AQUAPURED_VERSION );
  length += sprintf(buffer+length, ",\"name\":\"%s\"",AQUAPURED_NAME );
  length += sprintf(buffer+length, ",\"fullstatus\":\"%s\"",SWGstatus2test(aqdata->status));

  if ( aqdata->Percent != TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"swg_percent\":\"%d\"",aqdata->Percent );
  
  if ( aqdata->PPM != TEMP_UNKNOWN )
    length += sprintf(buffer+length, ",\"swg_ppm\":\"%d\"",aqdata->PPM );

 
  length += sprintf(buffer+length, ",\"leds\":{\"%s\": \"%s\"}", SWG_TOPIC, aqdata->status == 0x00?JSON_ON:JSON_OFF);

  length += sprintf(buffer+length, "}" );
  
  buffer[length] = '\0';

  
  return strlen(buffer);
}
*/
/*
int build_aux_labels_JSON(struct aqualinkdata *aqdata, char* buffer, int size)
{
  memset(&buffer[0], 0, size);
  int length = 0;
  int i;
  
  length += sprintf(buffer+length, "{\"type\": \"aux_labels\"");
  
  for (i=0; i < TOTAL_BUTTONS; i++) 
  {
    length += sprintf(buffer+length, ",\"%s\": \"%s\"", aqdata->aqbuttons[i].name, aqdata->aqbuttons[i].label);
  }
  
  length += sprintf(buffer+length, "}");
  
  return length;
  
//printf("%s\n",buffer);
  
  //return strlen(buffer);
}

// WS Received '{"parameter":"SPA_HTR","value":99}'
// WS Received '{"command":"KEY_HTR_POOL"}'
// WS Received '{"command":"GET_AUX_LABELS"}'

bool parseJSONwebrequest(char *buffer, struct JSONwebrequest *request)
{
  int i=0;
  int found=0;
  bool reading = false;

  request->first.key    = NULL;
  request->first.value  = NULL;
  request->second.key   = NULL;
  request->second.value = NULL;

  int length = strlen(buffer);

  while ( i < length )
  {
//printf ("Reading %c",buffer[i]);
    switch (buffer[i]) {
    case '{':
    case '"':
    case '}':
    case ':':
    case ',':
    case ' ':
      // Ignore space , : if reading a string
      if (reading == true && buffer[i] != ' ' && buffer[i] != ',' && buffer[i] != ':'){
 //printf (" <-  END");
        reading = false;
        buffer[i] = '\0';
        found++;
      }
      break;
      
    default:
      if (reading == false) {
//printf (" <-  START");
        reading = true;
        switch(found) {
        case 0:
          request->first.key = &buffer[i];
          break;
        case 1:
          request->first.value = &buffer[i];
          break;
        case 2:
          request->second.key = &buffer[i];
          break;
        case 3:
          request->second.value = &buffer[i];
          break;
        }
      }
      break;
    }
//printf ("\n");
    if (found >= 4)
    break;
    
    i++;
  }
  
  return true;
}
*/


bool parseJSONwebrequest(char *buffer, struct JSONwebrequest *request)
{
  int i=0;
  int found=0;
  bool reading = false;

  request->first.key    = NULL;
  request->first.value  = NULL;
  request->second.key   = NULL;
  request->second.value = NULL;
  request->third.key   = NULL;
  request->third.value = NULL;

  int length = strlen(buffer);

  while ( i < length )
  {
//printf ("Reading %c",buffer[i]);
    switch (buffer[i]) {
    case '{':
    case '"':
    case '}':
    case ':':
    case ',':
    case ' ':
      // Ignore space , : if reading a string
      if (reading == true && buffer[i] != ' ' && buffer[i] != ',' && buffer[i] != ':'){
 //printf (" <-  END");
        reading = false;
        buffer[i] = '\0';
        found++;
      }
      break;
      
    default:
      if (reading == false) {
//printf (" <-  START");
        reading = true;
        switch(found) {
        case 0:
          request->first.key = &buffer[i];
          break;
        case 1:
          request->first.value = &buffer[i];
          break;
        case 2:
          request->second.key = &buffer[i];
          break;
        case 3:
          request->second.value = &buffer[i];
          break;
        case 4:
          request->third.key = &buffer[i];
          break;
        case 5:
          request->third.value = &buffer[i];
          break;
        }
      }
      break;
    }
//printf ("\n");
//    if (found >= 4)
    if (found >= 6)
    break;
    
    i++;
  }
  
  return true;
}

char *jsontok(jsontoken *jt) {
  //int i = jt->position;
  int j;
  jt->key_ptr = NULL;
  jt->val_ptr = NULL;
  jt->key_len = 0;
  jt->val_len = 0;
  bool subset = 0;
  bool inquotes = false;

  while(jt->json[jt->pos] == ' ' || jt->json[jt->pos] == '{' || jt->json[jt->pos] == '"' || jt->json[jt->pos] <= 31) {
    if(jt->json[jt->pos] == '"')
      inquotes = !inquotes;
    jt->pos++;
  }

  jt->key_ptr = (char *)&jt->json[jt->pos];

  while (jt->pos < jt->json_len ) {
    switch (jt->json[jt->pos]){
      case '{':
      case '[':
        subset++;
      break;
      case '}':
      case ']':
        subset--;
      break;
      case '"':
        inquotes = !inquotes;
      break;

      case ':':
        // force move to value and error if no key
        if (subset==0 && !inquotes && jt->key_ptr != NULL && jt->key_len == 0) {
          j=jt->pos-1;
          while(jt->json[j] == ' ' || jt->json[j] == '"' || jt->json[j] <= 31){j--;}
          jt->key_len =  &jt->json[j+1] - jt->key_ptr;

          j=jt->pos+1;
          while(jt->json[j] == ' ' || jt->json[j] == '"' || jt->json[j] <= 31) {j++;}
          jt->val_ptr = (char *)&jt->json[j];
          
        } else if (subset==0 && !inquotes) {
          printf("Don't know what to do, received ':' out of sequance\n");
        }
      break;
      case ',':
        // We need to end if value is started, error if not
        if (subset==0 && !inquotes && jt->key_len != 0 && jt->val_len == 0 && jt->val_ptr != NULL) {
          j=jt->pos-1;
          while(jt->json[j] == ' ' || jt->json[j] == '"' || jt->json[j] <= 31){j--;}
          jt->val_len =  &jt->json[j+1] - jt->val_ptr;

          jt->pos++;
          // Handle blank value
          if (jt->val_len < 0) jt->val_len=0;
          return (char *)&jt->json[jt->pos+1];
        } else if (subset==0 && !inquotes) {
          printf("Don't know what to do, received ','\n");
        }
      break;
    }
    jt->pos++;
  }
 
  if (jt->val_ptr != NULL) {
    j=jt->pos-1;
    while(jt->json[j] == '}' || jt->json[j] == ' ' || jt->json[j] == '"' || jt->json[j] <= 31){j--;}
    jt->val_len =  &jt->json[j+1] - jt->val_ptr;
    // Handle blank value
    if (jt->val_len < 0) jt->val_len=0;
    return (char *)&jt->json[jt->pos+1];
  }

  return NULL;
}