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

#include "aquarited.h"
#include "ar_config.h"
#include "config.h"

//#include "aq_programmer.h"
#include "ar_net_services.h"
#include "domoticz.h"
#include "json_messages.h"
#include "utils.h"
//#include "aq_mqtt.h"

#include "aqualink.h"

typedef enum {mqttstarting, mqttrunning, mqttstopped, mqttdisabled} mqttstatus;
static mqttstatus _mqtt_status = mqttstopped;

static struct aqconfig *_config;
static struct arconfig *_ar_prms;

//static int _mqtt_exit_flag = false;

void mqtt_broadcast_aquaritestate(struct mg_connection *nc);
void start_mqtt(struct mg_mgr *mgr);

static sig_atomic_t s_signal_received = 0;
// static const char *s_http_port = "8080";
// static struct mg_serve_http_opts s_http_server_opts;

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

  logMessage(LOG_INFO, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
}

void send_domoticz_mqtt_status_message(struct mg_connection *nc, int idx, int value, char *svalue) {
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_message_JSON(mqtt_msg, JSON_MQTT_MSG_SIZE, idx, value, svalue);

  send_mqtt(nc, _config->mqtt_dz_pub_topic, mqtt_msg);
}

void send_domoticz_mqtt_state_msg(struct mg_connection *nc, int idx, int value) {
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg, JSON_MQTT_MSG_SIZE, idx, value, TEMP_UNKNOWN);
  send_mqtt(nc, _config->mqtt_dz_pub_topic, mqtt_msg);
}

void send_domoticz_mqtt_numeric_msg(struct mg_connection *nc, int idx, int value) {
  if (idx <= 0)
    return;

  char mqtt_msg[JSON_MQTT_MSG_SIZE];
  build_mqtt_status_JSON(mqtt_msg, JSON_MQTT_MSG_SIZE, idx, 0, value);
  send_mqtt(nc, _config->mqtt_dz_pub_topic, mqtt_msg);
}

/*
void send_mqtt_state_msg(struct mg_connection *nc, char *dev_name, aqledstate state)
{
  //static char mqtt_pub_topic[250];

  //sprintf(mqtt_pub_topic, "%s/%s",_aqualink_config->mqtt_aq_topic, dev_name);
  //send_mqtt(nc, mqtt_pub_topic, (state==OFF?"0":"1"));
}
*/
/*

void send_mqtt_setpoint_msg(struct mg_connection *nc, char *dev_name, long value)
{
  static char mqtt_pub_topic[250];
  static char degC[5];

  sprintf(degC, "%.2f", (_aqualink_data->temp_units==FAHRENHEIT)?degFtoC(value):value );
  sprintf(mqtt_pub_topic, "%s/%s/setpoint", _aqualink_config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, degC);
}
*/
void send_mqtt_int_msg(struct mg_connection *nc, char *dev_name, int value) {
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s", _config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}
void send_mqtt_float_msg(struct mg_connection *nc, char *dev_name, float value) {
  static char mqtt_pub_topic[250];
  static char msg[10];

  sprintf(msg, "%f", value);
  sprintf(mqtt_pub_topic, "%s/%s", _config->mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}
/*
void check_mqtt(struct mg_mgr *mgr) {
  static int mqtt_count = 0;
  // struct mg_connection *c;
  // char data[JSON_STATUS_SIZE];
  // build_aqualink_status_JSON(_aqualink_data, data, JSON_STATUS_SIZE);

  if (_mqtt_exit_flag == true) {
    mqtt_count++;
    if (mqtt_count >= 10) {
      start_mqtt(mgr);
      mqtt_count = 0;
    }
  }
}
*/
void check_net_services(struct mg_mgr *mgr) {

  if (_mqtt_status == mqttstopped)
    logMessage (LOG_NOTICE, "MQTT client stopped\n");

  if (_mqtt_status == mqttstopped && _mqtt_status != mqttdisabled) 
  {
    start_mqtt(mgr);
  }
}

bool broadcast_aquaritestate(struct mg_connection *nc) {
  struct mg_connection *c;

  // logMessage(LOG_INFO, "broadcast_aquaritestate:\n");

  //check_mqtt(nc->mgr);
  check_net_services(nc->mgr);

  if (_mqtt_status == mqttrunning){
    for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
      mqtt_broadcast_aquaritestate(c);
    }
  } else {
    return false;
  }

  return true;
}

void mqtt_broadcast_aquaritestate(struct mg_connection *nc) {
  send_mqtt_int_msg(nc, "SWG/Percent", _ar_prms->Percent);
  if (_ar_prms->Percent == 101)
    send_mqtt_float_msg(nc, "SWG/Percent_f", 38.4);
  else
    send_mqtt_float_msg(nc, "SWG/Percent_f", roundf(degFtoC(_ar_prms->Percent)));

  if (_ar_prms->PPM != TEMP_UNKNOWN)
    send_mqtt_int_msg(nc, "SWG/PPM", _ar_prms->PPM);

  send_mqtt_int_msg(nc, "SWG", _ar_prms->generating);

  // if (_config->mqtt_dz_pub_topic != NULL) {
  if (_config->dzidx_swg_percent > 0)
    send_domoticz_mqtt_numeric_msg(nc, _config->dzidx_swg_percent, _ar_prms->Percent);
  if (_config->dzidx_swg_ppm > 0 && _ar_prms->PPM != TEMP_UNKNOWN)
    send_domoticz_mqtt_numeric_msg(nc, _config->dzidx_swg_ppm, _ar_prms->PPM);

  if (_config->dzidx_swg_status > 0) {
    if (_ar_prms->ar_connected == false) {
      send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 0, "OFF");
       _ar_prms->last_status_published = 0xFF;
    } else if (_ar_prms->status != _ar_prms->last_status_published) {
//    } else {
      switch (_ar_prms->status) {
      // Level = (0=gray, 1=green, 2=yellow, 3=orange, 4=red)
      case 0x00:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 1, "GENERATING CHLORINE");
        break;
      case 0x01:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 2, "NO FLOW");
        break;
      case 0x02:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 2, "LOW SALT");
        break;
      case 0x04:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 3, "VERY LOW SALT");
        break;
      case 0x08:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 4, "HIGH CURRENT");
        break;
      case 0x09:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 0, "TURNING OFF");
        break;
      case 0x10:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 2, "CLEAN CELL");
        break;
      case 0x20:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 3, "LOW VOLTAGE");
        break;
      case 0x40:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 2, "WATER TEMP LOW");
        break;
      case 0x80:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 4, "CHECK PCB");
        break;
      default:
        send_domoticz_mqtt_status_message(nc, _config->dzidx_swg_status, 4, "Unknown");
        break;
      }
      _ar_prms->last_status_published = _ar_prms->status;
    }
  }
  //}
  /*
    if (_aqualink_data->air_temp != TEMP_UNKNOWN && _aqualink_data->air_temp != _last_mqtt_aqualinkdata.air_temp) {
      _last_mqtt_aqualinkdata.air_temp = _aqualink_data->air_temp;
      send_mqtt_temp_msg(nc, AIR_TEMP_TOPIC, _aqualink_data->air_temp);
      send_domoticz_mqtt_temp_msg(nc, _aqualink_config->dzidx_air_temp, _aqualink_data->air_temp);
    }
  */
}

void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  // int i;
  static char tmp[40];
  // logMessage(LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  logMessage(LOG_DEBUG, "MQTT: topic %.*s %.*s\n", msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);

  // Need to do this in a better manor, but for present it's ok.
  strncpy(tmp, msg->payload.p, msg->payload.len);
  tmp[msg->payload.len] = '\0';

  float value = atof(tmp);

  // logMessage(LOG_DEBUG, "MQTT: topic %.*s %.2f\n",msg->topic.len, msg->topic.p, atof(msg->payload.p));
  // only care about topics with set at the end.
  // aquarite/Percent/set // Only thing we care about
  // aquarite/SWG/Percent    // post only
  // aquarite/SWG      // post only
  // aquarite/SWG/PPM        // post only

  int pt = strlen(_config->mqtt_aq_topic) + 1;

  if (strncmp(&msg->topic.p[pt], "SWG/Percent/set", 15) == 0) {
    _ar_prms->Percent = round(value);
    printf("SET PERCENT TO %d", _ar_prms->Percent);
    broadcast_aquaritestate(nc);
    write_cache(_ar_prms);
  } else if (strncmp(&msg->topic.p[pt], "SWG/Percent_f/set", 17) == 0) {
    _ar_prms->Percent = round(degCtoF(value));
    printf("SET PERCENT TO %d", _ar_prms->Percent);
    broadcast_aquaritestate(nc);
    write_cache(_ar_prms);
  } else if (strncmp(&msg->topic.p[pt], "SWG/set", 7) == 0) {
    logMessage(LOG_NOTICE, "MQTT Trying to set no settable value %.*s to %.*s\n", msg->topic.len, msg->topic.p, msg->payload.len, msg->payload.p);
    broadcast_aquaritestate(nc);
  }
}

void action_domoticz_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) {
  // int idx = -1;
  // int nvalue = -1;
  // int i;
  // char svalue[DZ_SVALUE_LEN];

  logMessage(LOG_INFO, "action_domoticz_mqtt_message: NOT IMPLIMENTED\n");
  /*
    if (parseJSONmqttrequest(msg->payload.p, msg->payload.len, &idx, &nvalue, svalue)) {
      logMessage(LOG_DEBUG, "MQTT: DZ: Received message IDX=%d nValue=%d sValue=%s\n", idx, nvalue, svalue);
      for (i=0; i < TOTAL_BUTTONS; i++) {
        if (_aqualink_data->aqbuttons[i].dz_idx == idx){
          //NSF, should try to simplify this if statment, but not easy since AQ ON and DZ ON are different, and AQ has other states.
          if ( (_aqualink_data->aqbuttons[i].led->state == OFF && nvalue==DZ_OFF) ||
             (nvalue == DZ_ON && (_aqualink_data->aqbuttons[i].led->state == ON ||
                                  _aqualink_data->aqbuttons[i].led->state == FLASH ||
                                  _aqualink_data->aqbuttons[i].led->state == ENABLE))) {
            logMessage(LOG_INFO, "MQTT: DZ: received '%s' for '%s', already '%s', Ignoring\n", (nvalue==DZ_OFF?"OFF":"ON"), _aqualink_data->aqbuttons[i].name,
    (nvalue==DZ_OFF?"OFF":"ON")); } else {
            // NSF Below if needs to check that the button pressed is actually a light. Add this later
            if (_aqualink_data->active_thread.ptype == AQ_SET_COLORMODE ) {
              logMessage(LOG_NOTICE, "MQTT: DZ: received '%s' for '%s', IGNORING as we are programming light mode\n", (nvalue==DZ_OFF?"OFF":"ON"),
    _aqualink_data->aqbuttons[i].name); } else { logMessage(LOG_INFO, "MQTT: DZ: received '%s' for '%s', turning '%s'\n", (nvalue==DZ_OFF?"OFF":"ON"),
    _aqualink_data->aqbuttons[i].name,(nvalue==DZ_OFF?"OFF":"ON")); aq_programmer(AQ_SEND_CMD, (char *)&_aqualink_data->aqbuttons[i].code, _aqualink_data);
            }
          }
          break; // no need to continue in for loop, we found button.
        }
      }
      */
  /* removed until domoticz has a better virtual thermostat
  if (idx == _aqualink_config->dzidx_pool_thermostat) {
    float degC = atof(svalue);
    int degF = (int)degCtoF(degC);
    if (degC > 0.0 && 1 < (degF - _aqualink_data->pool_htr_set_point)) {
      logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'pool heater setpoint' old value %d(f) setting to %f(c) %d(f)\n",svalue,
  _aqualink_data->pool_htr_set_point, degC, degF);
      //aq_programmer(AQ_SET_POOL_HEATER_TEMP, (int)degCtoF(degl), _aqualink_data);
    } else {
      logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'pool heater setpoint' matched current setting (or was zero), ignoring!\n", svalue);
    }
  } else if (idx == _aqualink_config->dzidx_spa_thermostat) {
    float degC = atof(svalue);
    int degF = (int)degCtoF(degC);
    if (degC > 0.0 && 1 < (degF - _aqualink_data->spa_htr_set_point)) {
      logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'spa heater setpoint' old value %d(f) setting to %f(c) %d(f)\n",svalue,
  _aqualink_data->spa_htr_set_point, degC, degF);
      //aq_programmer(AQ_SET_POOL_HEATER_TEMP, (int)degCtoF(degl), _aqualink_data);
    } else {
      logMessage(LOG_INFO, "MQTT: DZ: received temp setting '%s' for 'spa heater setpoint' matched current setting (or was zero), ignoring!\n", svalue);
    }
  }*/
  // printf("Finished checking IDX for setpoint\n");
  //}

  // NSF Need to check idx against ours and decide if we are interested in it.
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct mg_mqtt_message *mqtt_msg;
  /// struct http_message *http_msg;
  // struct websocket_message *ws_msg;
  // static double last_control_time;

  // logMessage (LOG_DEBUG, "Event\n");
  switch (ev) {
  case MG_EV_CLOSE:
    logMessage(LOG_WARNING, "MQTT Connection closed\n");
    //_mqtt_exit_flag = true;
    _mqtt_status = mqttstopped;
    break;

  case MG_EV_CONNECT: {
    // nc->user_data = MQTT;
    // nc->flags |= MG_F_USER_1; // NFS Need to readup on this
    // set_mqtt(nc);
    //_mqtt_exit_flag = false;
    // char *MQTT_id = "AQUALINK_MQTT_TEST_ID";
    struct mg_send_mqtt_handshake_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.user_name = _config->mqtt_user;
    opts.password = _config->mqtt_passwd;
    opts.keep_alive = 5;
    opts.flags |= MG_MQTT_CLEAN_SESSION; // NFS Need to readup on this
    mg_set_protocol_mqtt(nc);
    mg_send_mqtt_handshake_opt(nc, _config->mqtt_ID, opts);
    logMessage(LOG_INFO, "MQTT: Subscribing mqtt with id of: %s\n", _config->mqtt_ID);
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

    snprintf(aq_topic, 29, "%s/#", _config->mqtt_aq_topic);
    if (_config->mqtt_aq_topic != NULL && _config->mqtt_dz_sub_topic != NULL) {
      topics[0].topic = aq_topic;
      topics[0].qos = qos;
      topics[1].topic = _config->mqtt_dz_sub_topic;
      topics[1].qos = qos;
      mg_mqtt_subscribe(nc, topics, 2, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", _config->mqtt_dz_sub_topic);
    } else if (_config->mqtt_aq_topic != NULL) {
      topics[0].topic = aq_topic;
      topics[0].qos = qos;
      mg_mqtt_subscribe(nc, topics, 1, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
    } else if (_config->mqtt_dz_sub_topic != NULL) {
      topics[0].topic = _config->mqtt_dz_sub_topic;
      ;
      topics[0].qos = qos;
      mg_mqtt_subscribe(nc, topics, 1, 42);
      logMessage(LOG_INFO, "MQTT: Subscribing to '%s'\n", _config->mqtt_dz_sub_topic);
    }
  } break;
  case MG_EV_MQTT_PUBACK:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    logMessage(LOG_INFO, "MQTT: Message publishing acknowledged (msg_id: %d)\n", mqtt_msg->message_id);
    break;
  case MG_EV_MQTT_SUBACK:
    logMessage(LOG_INFO, "MQTT: Subscription(s) acknowledged\n");
    broadcast_aquaritestate(nc);
    break;
  case MG_EV_MQTT_PUBLISH:
    mqtt_msg = (struct mg_mqtt_message *)ev_data;

    if (mqtt_msg->message_id != 0) {
      logMessage(LOG_DEBUG, "MQTT: received (msg_id: %d), looks like my own message, ignoring\n", mqtt_msg->message_id);
      break;
    }

    // NSF Need to change strlen to a global so it's not executed every time we check a topic
    if (_config->mqtt_aq_topic != NULL && strncmp(mqtt_msg->topic.p, _config->mqtt_aq_topic, strlen(_config->mqtt_aq_topic)) == 0) {
      action_mqtt_message(nc, mqtt_msg);
    }
    if (_config->mqtt_dz_sub_topic != NULL && strncmp(mqtt_msg->topic.p, _config->mqtt_dz_sub_topic, strlen(_config->mqtt_dz_sub_topic)) == 0) {
      action_domoticz_mqtt_message(nc, mqtt_msg);
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
  logMessage(LOG_NOTICE, "Starting MQTT client to %s\n", _config->mqtt_server);
  if (_config->mqtt_server == NULL || (_config->mqtt_aq_topic == NULL && _config->mqtt_dz_pub_topic == NULL && _config->mqtt_dz_sub_topic == NULL))
    return;

  if (mg_connect(mgr, _config->mqtt_server, ev_handler) == NULL) {
    logMessage(LOG_ERR, "Failed to create MQTT listener to %s\n", _config->mqtt_server);
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
bool start_net_services(struct mg_mgr *mgr, struct aqconfig *aqconfig, struct arconfig *ar_prms) {
  // struct mg_connection *nc;
  //_aqualink_data = aqdata;

  _config = aqconfig;
  _ar_prms = ar_prms;
  //_config = arconf;

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);

  mg_mgr_init(mgr, NULL);
  /*
  logMessage (LOG_NOTICE, "Starting web server on port %s\n", _aqualink_config->socket_port);
  nc = mg_bind(mgr, _aqualink_config->socket_port, ev_handler);
  if (nc == NULL) {
    logMessage (LOG_ERR, "Failed to create listener\n");
    return false;
  }
  */

  start_mqtt(mgr);

  return true;
}

