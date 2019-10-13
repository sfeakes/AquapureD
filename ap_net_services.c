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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include "mongoose.h"

#include "aquapured.h"
#include "SWG_device.h"
#include "GPIO_device.h"
#include "config.h"

//#include "aq_programmer.h"
#include "ap_net_services.h"
//#include "domoticz.h"
#include "json_messages.h"
#include "utils.h"
#include "aq_mqtt.h"

#include "aqualink.h"

static struct mg_serve_http_opts s_http_server_opts;
static char *_web_root;

typedef enum {mqttstarting, mqttrunning, mqttstopped, mqttdisabled} mqttstatus;
static mqttstatus _mqtt_status = mqttstopped;

//static struct aqconfig *_config;
//static struct apdata *_ar_prms;

//static int _mqtt_exit_flag = false;

void mqtt_broadcast_aquapurestate(struct mg_connection *nc);
void ws_broadcast_aquapurestate(struct mg_connection *nc);
void mqtt_broadcast_gpiostate(struct mg_connection *nc);
void ws_broadcast_gpiostate(struct mg_connection *nc);
void start_mqtt(struct mg_mgr *mgr);

static sig_atomic_t s_signal_received = 0;
// static const char *s_http_port = "8080";
// static struct mg_serve_http_opts s_http_server_opts;

static int is_websocket(const struct mg_connection *nc) {
  return nc->flags & MG_F_IS_WEBSOCKET;
}
static int is_mqtt(const struct mg_connection *nc) {
  return nc->flags & MG_F_USER_1;
}
static void set_mqtt(struct mg_connection *nc) {
  nc->flags |= MG_F_USER_1; 
}

static void ws_send(struct mg_connection *nc, char *msg)
{
  int size = strlen(msg);
  
  //logMessage (LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);

  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg, size);
  
  //logMessage (LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);
}

static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler); // Reinstantiate signal handler
  s_signal_received = sig_num;
}

void send_mqtt(struct mg_connection *nc, const char *toppic, char *message) {
  static uint16_t msg_id = 0;

  if (toppic == NULL)
    return;

  if (msg_id >= 65535) {
    msg_id = 1;
  } else {
    msg_id++;
  }

  // mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_QOS(0), message, strlen(message));
  mg_mqtt_publish(nc, toppic, msg_id, MG_MQTT_RETAIN | MG_MQTT_QOS(1), message, strlen(message));

  logMessage(LOG_DEBUG, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
}


void send_mqtt_int_msg(struct mg_connection *nc, char *dev_name, int value) {
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s",  _apconfig_.mqtt_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_mqtt_float_msg(struct mg_connection *nc, char *dev_name, float value) {
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%0.2f", value);
  sprintf(mqtt_pub_topic, "%s/%s",  _apconfig_.mqtt_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_domoticz_mqtt_status_message(struct mg_connection *nc, int idx, int value, char *svalue) {
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_dz_mqtt_status_message_JSON(mqtt_msg, JSON_MQTT_MSG_SIZE, idx, value, svalue);

  send_mqtt(nc, _apconfig_.mqtt_dz_pub_topic, mqtt_msg);
}




void check_net_services(struct mg_mgr *mgr) {

  if (_mqtt_status == mqttstopped)
    logMessage (LOG_NOTICE, "MQTT client stopped\n");

  if (_mqtt_status == mqttstopped && _mqtt_status != mqttdisabled) 
  {
    start_mqtt(mgr);
  }
}

bool broadcast_aquapurestate(struct mg_connection *nc) {
  struct mg_connection *c;
  //char data[JSON_STATUS_SIZE];
  //build_aqualink_status_JSON(_aqualink_data, data, JSON_STATUS_SIZE);


  

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    if (is_websocket(c)) {
      if (_apdata_.changed)
        ws_broadcast_aquapurestate(c);
      if (_gpiodata_.changed)
        ws_broadcast_gpiostate(c);
    } else if (is_mqtt(c)) {
      if (_apdata_.changed)
        mqtt_broadcast_aquapurestate(c);
      if (_gpiodata_.changed)
        mqtt_broadcast_gpiostate(c);
    }
    //printf("Connection is WS %d | MQTT %d\n",is_websocket(c),is_mqtt(c));
  }

/*
  if (_mqtt_status == mqttdisabled) 
    return false;
  // logMessage(LOG_INFO, "broadcast_aquapurestate:\n");

  //check_mqtt(nc->mgr);
  check_net_services(nc->mgr);

  if (_mqtt_status == mqttrunning){
    for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
      if (is_websocket(c))
        //ws_send(c, data);
      if (is_mqtt(c))
        mqtt_broadcast_aquapurestate(c);
    }
  } else {
    return false;
  }
*/
  return true;
}

void ws_broadcast_aquapurestate(struct mg_connection *nc) {
  char data[JSON_STATUS_SIZE];
  //int size = build_aquapure_status_JSON(_ar_prms, data, JSON_STATUS_SIZE);

  build_swg_device_JSON(&_apdata_, data, JSON_STATUS_SIZE, false);

  ws_send(nc, data);
}

void ws_broadcast_gpiostate(struct mg_connection *nc) {
  char data[JSON_STATUS_SIZE];
  //int size = build_aquapure_status_JSON(_ar_prms, data, JSON_STATUS_SIZE);

  //build_device_JSON(&_apdata_, data, JSON_STATUS_SIZE, false);
  build_gpio_device_JSON(&_gpiodata_, data, JSON_STATUS_SIZE, false);

  ws_send(nc, data);
}

void mqtt_broadcast_aquapurestate(struct mg_connection *nc) {
  send_mqtt_int_msg(nc, SWG_PERCENT_TOPIC, _apdata_.Percent);
  if (_apdata_.Percent == 101)
    send_mqtt_float_msg(nc, SWG_PERCENT_F_TOPIC, 38.4);
  else
    send_mqtt_float_msg(nc, SWG_PERCENT_F_TOPIC, roundf(degFtoC(_apdata_.Percent)));

  if (_apdata_.PPM != TEMP_UNKNOWN) {
    send_mqtt_int_msg(nc, SWG_PPM_TOPIC, _apdata_.PPM);
    send_mqtt_float_msg(nc, SWG_PPM_F_TOPIC, roundf(degFtoC(_apdata_.PPM)));
  }

  //send_mqtt_int_msg(nc, SWG_TOPIC, ((_apdata_.connected && (_apdata_.Percent > 0))?2:0) );
  send_mqtt_int_msg(nc, SWG_ENABELED_TOPIC, (_apdata_.connected?2:0));
  send_mqtt_int_msg(nc, SWG_EXTENDED_TOPIC, (int)_apdata_.status);
  send_mqtt_int_msg(nc, SWG_BOOST_TOPIC, _apdata_.boost);

  //if (_apconfig_.dzidx_swg_status > 0 && strlen(_apconfig_.mqtt_dz_pub_topic) > 0){
  switch (_apdata_.status) {
      // Level = (0=gray, 1=green, 2=yellow, 3=orange, 4=red)
      case SWG_STATUS_ON:
        set_display_message( "AquaPure ON");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 1, "GENERATING CHLORINE");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
      case SWG_STATUS_NO_FLOW:   
        set_display_message( "AquaPure No Flow");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 2, "NO FLOW");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_OFF);
        break;
      case SWG_STATUS_LOW_SALT:    
        set_display_message( "AquaPure Low Salt");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 2, "LOW SALT");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
      case SWG_STATUS_HI_SALT:       
        set_display_message( "AquaPure High Salt");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 3, "HIGH SALT");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
      case SWG_STATUS_HIGH_CURRENT:      
        set_display_message( "AquaPure High Current");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 4, "HIGH CURRENT");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
      case SWG_STATUS_TURNING_OFF:     
        set_display_message( "AquaPure Turning Off");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 0, "TURNING OFF");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_OFF);
        break;
      case SWG_STATUS_CLEAN_CELL:     
        set_display_message( "AquaPure Clean Cell");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 2, "CLEAN CELL");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
      case SWG_STATUS_LOW_VOLTS:       
        set_display_message( "AquaPure Low Voltage");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 3, "LOW VOLTAGE");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
      case SWG_STATUS_LOW_TEMP:      
        set_display_message( "AquaPure Water Temp Low");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 2, "WATER TEMP LOW");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_OFF);
        break;
      case SWG_STATUS_CHECK_PCB:       
        set_display_message( "AquaPure Check PCB");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 4, "CHECK PCB");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_OFF);
        break;
      case SWG_STATUS_OFF: // THIS IS OUR OFF STATUS, NOT AQUAPURE      
        set_display_message( "AquaPure OFF");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 0, "OFF");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_OFF);
        break;
      case SWG_STATUS_OFFLINE:
        set_display_message( "AquaPure OFFLINE");
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 4, "OFFLINE");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_OFF);
        break;
      default:
        send_domoticz_mqtt_status_message(nc, _apconfig_.dzidx_swg_status_msg, 4, "Unknown");
        send_mqtt_int_msg(nc, SWG_TOPIC, SWG_ON);
        break;
  }
/*
  if (_apdata_.status != _apdata_.last_status_published) {

  } else {
   _apdata_.last_status_published = _apdata_.status;
  }
*/
 
}

void mqtt_broadcast_gpiostate(struct mg_connection *nc) {
  int i;
  char topic[20];

  for (i=0; i < _gpiodata_.num_devices; i++) {
    //if (_gpiodata_.devices[i].changed) {
      sprintf(topic, "%s%d",GPIO_TOPIC,_gpiodata_.devices[i].pin);
      //send_mqtt_int_msg(nc, topic, (digitalRead(_gpiodata_.devices[i].pin)==_gpiodata_.devices[i].on_state?MQTT_ON:MQTT_OFF) );
      send_mqtt_int_msg(nc, topic, is_gpiodevice_on(&_gpiodata_.devices[i])?MQTT_ON:MQTT_OFF);

    //}
  }
}
/*
void set_swg_percent(char *sval, bool f2c) {
  float value = atof(sval);

  if (f2c) {
    value = degCtoF(value);
  }

  if (_apdata_.connected != true) {
    logMessage(LOG_ERR, "Can't set Percent %d, SWG not connected\n",value);
     _apdata_.changed = true;
    return;
  } else if (_apdata_.boost == true) {
    logMessage(LOG_ERR, "Can't set Percent %d, SWG Boost is active\n",value);
     _apdata_.changed = true;
    return;
  }

  int ival = round(value);

    //if (0 != ( ival % 5) )
    //  ival = ((ival + 5) / 10) * 10;

  if (ival > 100)
      ival = 100;
  else if (ival < 0)
      ival = 0;

  _apdata_.Percent = ival;

  if (ival > 0 && ival < 101)
    _apdata_.last_generating_percent = ival;
   //_apdata_.Percent = round(degCtoF(value));
  logMessage(LOG_INFO, "Setting SWG percent to %d", _apdata_.Percent);
    //broadcast_aquapurestate(nc);
  write_cache(_ar_prms);
  _apdata_.changed = true;
}

void set_swg_boost(bool val) {
  if (_apdata_.connected != true) {
    logMessage(LOG_ERR, "Can't boost, SWG not connected\n");
     _apdata_.changed = true;
    return;
  }

  if (_apdata_.boost != val) {
    _apdata_.boost = val;
    if (val == true) { // turning boost on
      if (_apdata_.Percent > 0)
        _apdata_.last_generating_percent = _apdata_.Percent;

      _apdata_.Percent = 101;
    } else { // Turning boost off
      if (_apdata_.last_generating_percent > 0)
        _apdata_.Percent = _apdata_.last_generating_percent;
      else
        set_swg_percent("0", false);
    }
  }

  _apdata_.changed = true;
}

void set_swg_on(bool val) {
  if (_apdata_.connected != true) {
    logMessage(LOG_ERR, "Can't turn SWG %s, SWG not connected\n",(val==true?"on":"off"));
     _apdata_.changed = true;
    return;
  }

  if (val==true) {
    if (_apdata_.Percent > 0) {
      logMessage(LOG_NOTICE, "Can't turn SWG %s, SWG is already %s\n",(val==true?"on":"off"),(_apdata_.Percent>0?"on":"off"));
      return;
    }
    if (_apdata_.last_generating_percent > 0 && _apdata_.last_generating_percent < 101)
      _apdata_.Percent = _apdata_.last_generating_percent;
    else
      _apdata_.Percent = _apdata_.default_percent;
  }

  if (val==false) {
    if (_apdata_.Percent == 0) {
      logMessage(LOG_NOTICE, "Can't turn SWG %s, SWG is already %s\n",(val==true?"on":"off"),(_apdata_.Percent>0?"on":"off"));
      return;
    }
    set_swg_percent("0", false);
  }

  _apdata_.changed = true;
}
*/
void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  // int i;
  static char tmp[40];
  // logMessage(LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  logMessage(LOG_DEBUG, "MQTT: topic %.*s %.*s\n", msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);

  // Need to do this in a better manor, but for present it's ok.
  strncpy(tmp, msg->payload.p, msg->payload.len);
  tmp[msg->payload.len] = '\0';

  //float value = atof(tmp);

  // logMessage(LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  // only care about topics with set at the end.
  // aquapure/Percent/set // Only thing we care about
  // aquapure/SWG/Percent    // post only
  // aquapure/SWG      // post only
  // aquapure/SWG/PPM        // post only

  int pt = strlen( _apconfig_.mqtt_topic) + 1;

  if (strncmp(&msg->topic.p[pt], "SWG/Percent/set", 15) == 0) {
    set_swg_req_percent(tmp, false);
  } else if (strncmp(&msg->topic.p[pt], "SWG/Percent_f/set", 17) == 0) {
    set_swg_req_percent(tmp, true);
  } else if (strncmp(&msg->topic.p[pt], "SWG/Boost/set", 13) == 0) {
    if (atoi(tmp) == 1)
      set_swg_boost(true);
    else
      set_swg_boost(false);
  } else if (strncmp(&msg->topic.p[pt], "SWG/set", 7) == 0) {
    if (atoi(tmp) == 1)
      set_swg_on(true);
    else
      set_swg_on(false);
    //logMessage(LOG_NOTICE, "MQTT Trying to set no settable value %.*s to %.*s\n", msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);
    //broadcast_aquapurestate(nc);
    //_apdata_.changed = true;
  } else if (strncmp(&msg->topic.p[pt], GPIO_TOPIC, 5) == 0 &&
             ( strncmp(&msg->topic.p[pt]+7, "/set", 4) == 0 || strncmp(&msg->topic.p[pt]+6, "/set", 3) == 0)) {
     action_gpio_request(&msg->topic.p[pt], tmp);
  } else {
    logMessage(LOG_DEBUG, "MQTT: Didn't understand topic %.*s %.*s\n", msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);
  }
}

void action_web_request(struct mg_connection *nc, struct http_message *http_msg) {
  // struct http_message *http_msg = (struct http_message *)ev_data;
  if (getLogLevel() >= LOG_INFO) { // Simply for log message, check we are at
                                   // this log level before running all this
                                   // junk
    char *uri = (char *)malloc(http_msg->uri.len + http_msg->query_string.len + 2);
    strncpy(uri, http_msg->uri.p, http_msg->uri.len + http_msg->query_string.len + 1);
    uri[http_msg->uri.len + http_msg->query_string.len + 1] = '\0';
    logMessage(LOG_INFO, "URI request: '%s'\n", uri);
    free(uri);
  }
  // If we have a get request, pass it
  if (strstr(http_msg->method.p, "GET") && http_msg->query_string.len > 0) {
    char command[20];

    mg_get_http_var(&http_msg->query_string, "command", command, sizeof(command));
    logMessage(LOG_INFO, "WEB: Message command='%s'\n", command);

    if (strcmp(command, "status") == 0) {
      char data[JSON_STATUS_SIZE];
      int size = build_device_JSON(&_apdata_, &_gpiodata_, data, JSON_STATUS_SIZE, false);
      mg_send_head(nc, 200, size, "Content-Type: application/json");
      mg_send(nc, data, size);
      return;
    } else if (strcmp(command, "homebridge") == 0) {
      char data[JSON_STATUS_SIZE];
      int size = build_device_JSON(&_apdata_, &_gpiodata_, data, JSON_STATUS_SIZE, true);
      mg_send_head(nc, 200, size, "Content-Type: application/json");
      mg_send(nc, data, size);
      return;
    } else if (strcmp(command, SWG_PERCENT_TOPIC) == 0 ||
               strcmp(command, "swg_percent") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      set_swg_req_percent(value, false);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));
      return;
    } else if (strcmp(command, SWG_TOPIC) == 0 || 
               strcmp(command, "swg") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      if (strcmp(value, "off") == 0)
        set_swg_on(false);
      else
        set_swg_on(true);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));
      return;
    } else if (strcmp(command, SWG_BOOST_TOPIC) == 0 ) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      if (strcmp(value, "off") == 0)
        set_swg_boost(false);
      else
        set_swg_boost(true);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));
      return;
    } else if (strncmp(command, GPIO_TOPIC, 4) == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      action_gpio_request(command, value);
    }
    
    mg_send_head(nc, 200, strlen(GET_RTN_UNKNOWN), "Content-Type: text/plain");
    mg_send(nc, GET_RTN_UNKNOWN, strlen(GET_RTN_UNKNOWN));
    
    
  } else {
    struct mg_serve_http_opts opts;
    memset(&opts, 0, sizeof(opts)); // Reset all options to defaults
    opts.document_root = _web_root; // Serve files from the current directory
    // logMessage (LOG_DEBUG, "Doc root=%s\n",opts.document_root);
    mg_serve_http(nc, http_msg, s_http_server_opts);
  }
}

void action_websocket_request(struct mg_connection *nc, struct websocket_message *wm) {
  char buffer[50];
  char data[JSON_STATUS_SIZE];
  struct JSONwebrequest request;
  
  // Any websocket request means UI is active, so don't let AqualinkD go to sleep if in PDA mode

  strncpy(buffer, (char *)wm->data, wm->size);
  buffer[wm->size] = '\0';
  // logMessage (LOG_DEBUG, "buffer '%s'\n", buffer);

  parseJSONwebrequest(buffer, &request);
  logMessage(LOG_INFO, "WS: Message - Key '%s' Value '%s' | Key2 '%s' Value2 '%s'\n", request.first.key, request.first.value, request.second.key,
             request.second.value);
  
  if (strcmp(request.first.value, "GET_DEVICES") == 0) {
      //char message[JSON_LABEL_SIZE*10];
      //build_device_JSON(_aqualink_data, _apconfig_.light_programming_button_pool, _apconfig_.light_programming_button_spa, message, JSON_LABEL_SIZE*10, false);
      build_device_JSON(&_apdata_, &_gpiodata_, data, JSON_STATUS_SIZE, false);
      //logMessage(LOG_DEBUG, "-->%s<--", data);
      ws_send(nc, data);
  } else if (strcmp(request.first.key, "command") == 0) {
    if (strcmp(request.first.value, SWG_BOOST_TOPIC) == 0) {
printf("***** BOOST ****\n");
      action_boost_request(request.second.value);
    } else if (strcmp(request.first.value, SWG_TOPIC) == 0) {
      //printf("Turn SWG on/off NOT IMPLIMENTED YET!\n");
      //_apdata_.changed = true;
      if (strcmp(request.second.value, "on") == 0)
        set_swg_on(true);
      else
        set_swg_on(false);
    } else if (strncmp(request.first.value, GPIO_TOPIC, 4) == 0) {
      action_gpio_request(request.first.value, request.second.value);
    }
  } else if (strcmp(request.first.key, "parameter") == 0) {
    if (strcmp(request.first.value, SWG_TOPIC) == 0) { // Should delete this soon
      set_swg_req_percent(request.second.value, false);
    } else if (strcmp(request.first.value, SWG_PERCENT_TOPIC) == 0) {
      set_swg_req_percent(request.second.value, false);
    } else if (strcmp(request.first.value, SWG_PERCENT_F_TOPIC) == 0) {
      set_swg_req_percent(request.second.value, true);
    }
  }
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct mg_mqtt_message *mqtt_msg;
  struct http_message *http_msg;
  struct websocket_message *ws_msg;
  // static double last_control_time;

  //logMessage (LOG_DEBUG, "Event %d\n",ev);
  switch (ev) {
  case MG_EV_HTTP_REQUEST:
    //nc->user_data = WEB;
    logMessage(LOG_DEBUG, "Start WEB request\n");
    http_msg = (struct http_message *)ev_data;
    action_web_request(nc, http_msg);
    logMessage(LOG_DEBUG, "Finish WEB request\n");
    break;
    /*
  case MG_EV_CLOSE:
    logMessage(LOG_WARNING, "MQTT Connection closed\n");
    //_mqtt_exit_flag = true;
    _mqtt_status = mqttstopped;
    break;
*/
  case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
    //nc->user_data = WS;
    //_aqualink_data->open_websockets++;
    logMessage(LOG_DEBUG, "++ Websocket joined\n");
    break;
  
  case MG_EV_WEBSOCKET_FRAME: 
    ws_msg = (struct websocket_message *)ev_data;
    action_websocket_request(nc, ws_msg);
    break;
  
  case MG_EV_CLOSE: 
    if (is_websocket(nc)) {
      //_aqualink_data->open_websockets--;
      logMessage(LOG_DEBUG, "-- Websocket left\n");
    } else if (is_mqtt(nc)) {
      logMessage(LOG_WARNING, "MQTT Connection closed\n");
      _mqtt_status = mqttstopped;
    }
    break;

  case MG_EV_CONNECT: {
    // nc->user_data = MQTT;
    // nc->flags |= MG_F_USER_1; // NFS Need to readup on this
    set_mqtt(nc);
    //_mqtt_exit_flag = false;
    // char *MQTT_id = "AQUALINK_MQTT_TEST_ID";
    struct mg_send_mqtt_handshake_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.user_name =  _apconfig_.mqtt_user;
    opts.password =  _apconfig_.mqtt_passwd;
    //opts.keep_alive = 5;
    opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this
    mg_set_protocol_mqtt(nc);
    mg_send_mqtt_handshake_opt(nc,  _apconfig_.mqtt_ID, opts);
    logMessage(LOG_INFO, "MQTT: Subscribing mqtt with id of: %s\n",  _apconfig_.mqtt_ID);
    _mqtt_status = mqttrunning;
    // last_control_time = mg_time();
  } break;

  case MG_EV_MQTT_CONNACK: {
    struct mg_mqtt_topic_expression topics[2];
    char aq_topic[30];
    int qos = 0; // can't be bothered with ack, so set to 0

    logMessage(LOG_INFO, "MQTT: Connection acknowledged\n");
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    if (mqtt_msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED) {
      logMessage(LOG_WARNING, "Got mqtt connection error: %d\n", mqtt_msg->connack_ret_code);
      //_mqtt_exit_flag = true;
      _mqtt_status = mqttstopped;
    }

    snprintf(aq_topic, 29, "%s/#",  _apconfig_.mqtt_topic);
    if ( _apconfig_.mqtt_topic != NULL) {
      topics[0].topic = aq_topic;
      topics[0].qos = qos;
      mg_mqtt_subscribe(nc, topics, 1, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
    } 
  } break;
  case MG_EV_MQTT_PUBACK:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    logMessage(LOG_DEBUG, "MQTT: Message publishing acknowledged (msg_id: %d)\n", mqtt_msg->message_id);
    break;
  case MG_EV_MQTT_SUBACK:
    logMessage(LOG_INFO, "MQTT: Subscription(s) acknowledged\n");
    broadcast_aquapurestate(nc);
    break;
  case MG_EV_MQTT_PUBLISH:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;

    if (mqtt_msg->message_id != 0) {
      logMessage(LOG_DEBUG, "MQTT: received (msg_id: %d), looks like my own message, ignoring\n", mqtt_msg->message_id);
      break;
    }

    // NSF Need to change strlen to a global so it's not executed every time we check a topic
    if ( _apconfig_.mqtt_topic != NULL && strncmp(mqtt_msg->topic.p,  _apconfig_.mqtt_topic, strlen( _apconfig_.mqtt_topic)) == 0) {
      action_mqtt_message(nc, mqtt_msg);
    }

    break;
    /*
    // MQTT ping wasn't working in previous versions.
  case MG_EV_POLL: {
    struct mg_mqtt_proto_data *pd = (struct mg_mqtt_proto_data *)nc->proto_data;
    double now = mg_time();

    if (pd->keep_alive > 0 && last_control_time > 0 && (now - last_control_time) > pd->keep_alive) {
      logMessage(LOG_INFO, "MQTT: Sending MQTT ping\n");
      mg_mqtt_ping(nc);
      last_control_time = now;
    }
  }
    break;*/
  }
}

void start_mqtt(struct mg_mgr *mgr) {

  
  if ( _apconfig_.mqtt_server == NULL ||  _apconfig_.mqtt_topic == NULL ) {
    if (_mqtt_status != mqttdisabled)
      logMessage(LOG_NOTICE, "Disabling MQTT client, no config!\n",  _apconfig_.mqtt_server);

    _mqtt_status = mqttdisabled;
    return;
  }
  
  logMessage(LOG_NOTICE, "Starting MQTT client to %s\n",  _apconfig_.mqtt_server);

  if (mg_connect(mgr,  _apconfig_.mqtt_server, ev_handler) == NULL) {
    logMessage(LOG_ERR, "Failed to create MQTT listener to %s\n",  _apconfig_.mqtt_server);
  } else {
    /*
    int i;
    for (i=0; i < TOTAL_BUTTONS; i++) {
      _last_mqtt_aqualinkdata.aqualinkleds[i].state = LED_S_UNKNOWN;
    }*/
    //_mqtt_exit_flag = false; // set here to stop multiple connects, if it fails truley fails it will get set to false.
    _mqtt_status = mqttstarting;
  }
}

// bool start_web_server(struct mg_mgr *mgr, struct aqualinkdata *aqdata, char *port, char* web_root) {
// bool start_net_services(struct mg_mgr *mgr, struct aqualinkdata *aqdata, struct aqconfig *aqconfig) {
bool start_net_services(struct mg_mgr *mgr) {
  struct mg_connection *nc;
  //_aqualink_data = aqdata;

  //_config = aqconfig;
  //_ar_prms = ar_prms;
  //_config = arconf;

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);

  mg_mgr_init(mgr, NULL);
  
  logMessage (LOG_NOTICE, "Starting WEB Server on port %s\n",  _apconfig_.socket_port);
  nc = mg_bind(mgr, _apconfig_.socket_port, ev_handler);
  if (nc == NULL) {
    logMessage (LOG_ERR, "Failed to create listener\n");
    return false;
  }
  

  start_mqtt(mgr);

  mg_set_protocol_http_websocket(nc);
  s_http_server_opts.document_root = _apconfig_.web_directory;  // Serve current directory
  s_http_server_opts.enable_directory_listing = "yes";

  return true;
}

