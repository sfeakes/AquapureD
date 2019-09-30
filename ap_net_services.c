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
#include "ap_config.h"
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
static struct apdata *_ar_prms;

//static int _mqtt_exit_flag = false;

void mqtt_broadcast_aquapurestate(struct mg_connection *nc);
void ws_broadcast_aquapurestate(struct mg_connection *nc);
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
  
  logMessage (LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);

  mg_send_websocket_frame(nc, WEBSOCKET_OP_TEXT, msg, size);
  
  logMessage (LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);
}

static void signal_handler(int sig_num) {
  signal(sig_num, signal_handler); // Reinstantiate signal handler
  s_signal_received = sig_num;
}

void send_mqtt(struct mg_connection *nc, char *toppic, char *message) {
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
  sprintf(mqtt_pub_topic, "%s/%s",  _apconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_mqtt_float_msg(struct mg_connection *nc, char *dev_name, float value) {
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%0.2f", value);
  sprintf(mqtt_pub_topic, "%s/%s",  _apconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
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
    if (is_websocket(c))
      ws_broadcast_aquapurestate(c);
    else if (is_mqtt(c))
      mqtt_broadcast_aquapurestate(c);

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

  int size = build_device_JSON(_ar_prms, data, JSON_STATUS_SIZE, false);

  ws_send(nc, data);
}

void mqtt_broadcast_aquapurestate(struct mg_connection *nc) {
  send_mqtt_int_msg(nc, SWG_PERCENT_TOPIC, _ar_prms->Percent);
  if (_ar_prms->Percent == 101)
    send_mqtt_float_msg(nc, SWG_PERCENT_F_TOPIC, 38.4);
  else
    send_mqtt_float_msg(nc, SWG_PERCENT_F_TOPIC, roundf(degFtoC(_ar_prms->Percent)));

  if (_ar_prms->PPM != TEMP_UNKNOWN) {
    send_mqtt_int_msg(nc, SWG_PPM_TOPIC, _ar_prms->PPM);
    send_mqtt_float_msg(nc, SWG_PPM_F_TOPIC, roundf(degFtoC(_ar_prms->PPM)));
  }

  send_mqtt_int_msg(nc, SWG_TOPIC, _ar_prms->generating);

  send_mqtt_int_msg(nc, SWG_EXTENDED_TOPIC, (int)_ar_prms->status);

  if (_ar_prms->status != _ar_prms->last_status_published) {

  } else {
   _ar_prms->last_status_published = _ar_prms->status;
  }

 
}

void set_swg_percent(char *sval, bool f2c) {
  float value = atof(sval);

  if (f2c) {
    value = degCtoF(value);
  }

  int ival = round(value);

    //if (0 != ( ival % 5) )
    //  ival = ((ival + 5) / 10) * 10;

  if (ival > 100)
      ival = 100;
  else if (ival < 0)
      ival = 0;

  _ar_prms->Percent = ival;
   //_ar_prms->Percent = round(degCtoF(value));
  logMessage(LOG_INFO, "Setting SWG percent to %d", _ar_prms->Percent);
    //broadcast_aquapurestate(nc);
  write_cache(_ar_prms);
  _ar_prms->changed = true;
}


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

  int pt = strlen( _apconfig_.mqtt_aq_topic) + 1;

  if (strncmp(&msg->topic.p[pt], "SWG/Percent/set", 15) == 0) {
    //_ar_prms->Percent = round(value);
    //printf("SET PERCENT TO %d", _ar_prms->Percent);
    //broadcast_aquapurestate(nc);
    //write_cache(_ar_prms);
    //_ar_prms->changed = true;
    set_swg_percent(tmp, false);
  } else if (strncmp(&msg->topic.p[pt], "SWG/Percent_f/set", 17) == 0) {
    //_ar_prms->Percent = round(degCtoF(value));
    //printf("SET PERCENT TO %d", _ar_prms->Percent);
    //broadcast_aquapurestate(nc);
    //write_cache(_ar_prms);
    //_ar_prms->changed = true;
    set_swg_percent(tmp, true);
  } else if (strncmp(&msg->topic.p[pt], "SWG/set", 7) == 0) {
    logMessage(LOG_NOTICE, "MQTT Trying to set no settable value %.*s to %.*s\n", msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);
    //broadcast_aquapurestate(nc);
    _ar_prms->changed = true;
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
      int size = build_device_JSON(_ar_prms, data, JSON_STATUS_SIZE, false);
      mg_send_head(nc, 200, size, "Content-Type: application/json");
      mg_send(nc, data, size);
      return;
    } else if (strcmp(command, "homebridge") == 0) {
      char data[JSON_STATUS_SIZE];
      int size = build_device_JSON(_ar_prms, data, JSON_STATUS_SIZE, true);
      mg_send_head(nc, 200, size, "Content-Type: application/json");
      mg_send(nc, data, size);
      return;
    } else if (strcmp(command, "swg_percent") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      
      //_ar_prms->Percent = atoi(value);
      //printf("SET PERCENT TO %d\n", _ar_prms->Percent);
      //broadcast_aquapurestate(nc);
      //write_cache(_ar_prms);
      //_ar_prms->changed = true;
      set_swg_percent(value, false);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));
      return;
    } else if (strcmp(command, "swg") == 0) {
      char value[20];
      mg_get_http_var(&http_msg->query_string, "value", value, sizeof(value));
      if (strcmp(value, "off") == 0) {
        set_swg_percent("0", false);
      }
      //_ar_prms->Percent = 0;
      //printf("SET PERCENT TO %d\n", _ar_prms->Percent);
      //broadcast_aquapurestate(nc);
      //write_cache(_ar_prms);
      //_ar_prms->changed = true;
      //set_swg_percent(value, false);
      mg_send_head(nc, 200, strlen(GET_RTN_OK), "Content-Type: text/plain");
      mg_send(nc, GET_RTN_OK, strlen(GET_RTN_OK));
      return;
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
      //build_device_JSON(_aqualink_data, _aqualink_config->light_programming_button_pool, _aqualink_config->light_programming_button_spa, message, JSON_LABEL_SIZE*10, false);
      int size = build_device_JSON(_ar_prms, data, JSON_STATUS_SIZE, false);
      logMessage(LOG_DEBUG, "-->%s<--", data);
      ws_send(nc, data);
  } else if (strcmp(request.first.key, "command") == 0) {
    if (strcmp(request.first.value, SWG_TOPIC) == 0) {
      printf("Turn SWG on/off NOT IMPLIMENTED YET!\n");
      _ar_prms->changed = true;
    }
  } else if (strcmp(request.first.key, "parameter") == 0) {
    if (strcmp(request.first.value, SWG_TOPIC) == 0) { // Should delete this soon
      set_swg_percent(request.second.value, false);
    } else if (strcmp(request.first.value, SWG_PERCENT_TOPIC) == 0) {
      set_swg_percent(request.second.value, false);
    } else if (strcmp(request.first.value, SWG_PERCENT_F_TOPIC) == 0) {
      set_swg_percent(request.second.value, true);
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

    snprintf(aq_topic, 29, "%s/#",  _apconfig_.mqtt_aq_topic);
    if ( _apconfig_.mqtt_aq_topic != NULL) {
      topics[0].topic = aq_topic;
      topics[0].qos = qos;
      mg_mqtt_subscribe(nc, topics, 1, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
    } 
  } break;
  case MG_EV_MQTT_PUBACK:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    logMessage(LOG_INFO, "MQTT: Message publishing acknowledged (msg_id: %d)\n", mqtt_msg->message_id);
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
    if ( _apconfig_.mqtt_aq_topic != NULL && strncmp(mqtt_msg->topic.p,  _apconfig_.mqtt_aq_topic, strlen( _apconfig_.mqtt_aq_topic)) == 0) {
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

  
  if ( _apconfig_.mqtt_server == NULL ||  _apconfig_.mqtt_aq_topic == NULL ) {
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
bool start_net_services(struct mg_mgr *mgr, struct apdata *ar_prms) {
  struct mg_connection *nc;
  //_aqualink_data = aqdata;

  //_config = aqconfig;
  _ar_prms = ar_prms;
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

