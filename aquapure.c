#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include "mongoose.h"
#include "aq_serial.h"
#include "config.h"
#include "SWG_device.h"
#include "GPIO_device.h"

#include "utils.h"
#include "ap_net_services.h"

#include "version.h"
//#define SLOG_MAX 80
//#define PACKET_MAX 10000


// TIMEOUT_TRIES needs to be divisable by RETRY_NUL_READ.  
// Number of RETRY_NUL_READ null reads = re send last message.   ie re-send every 5 poll cycles of nothing read.
// Number of TIMEOUT_TRIES / RETRY_NUL_READ null reads set to connection faulure.  ie 3 re-sends, and nothing read call SWG dead. 
//#define RETRY_NUL_READ 5  
//#define TIMEOUT_TRIES 15  




//extern apconfig _apconfig_;

//struct aqconfig _config;
//struct apdata _ar_prms;

bool _keepRunning = true;
//int _percentsalt_ = 50;
//bool _forceConnection = false;

void main_loop();

void intHandler(int dummy) {
  _keepRunning = false;
  logMessage(LOG_NOTICE, "Stopping!");
}

/*
void printHex(char *pk, int length) {
  int i = 0;
  for (i = 0; i < length; i++) {
    printf("0x%02hhx|", pk[i]);
  }
}

void printPacket(unsigned char ID, unsigned char *packet_buffer, int packet_length) {
  // if (packet_buffer[PKT_DEST] != 0x00)
  // printf("\n");

  printf("    Received  |");
  printf(" HEX: ");
  printHex((char *)packet_buffer, packet_length);

  if (packet_buffer[PKT_CMD] == CMD_MSG || packet_buffer[PKT_CMD] == CMD_MSG_LONG) {
    printf("  Message : ");
    // fwrite(packet_buffer + 4, 1, AQ_MSGLEN+1, stdout);
    fwrite(packet_buffer + 4, 1, packet_length - 7, stdout);
  }

  printf("\n");
}
*/
/*
void debugPacketPrint(unsigned char ID, unsigned char *packet_buffer, int packet_length)
{
  char buff[1000];
  int i=0;
  int cnt = 0;

  //cnt = sprintf(buff, "%4.4s 0x%02hhx of type %8.8s", (packet_buffer[PKT_DEST]==0x00?"From":"To"), ID, get_packet_type(packet_buffer, packet_length));
  cnt += sprintf(buff+cnt, "Received %8.8s | HEX: ", get_packet_type(packet_buffer, packet_length));
  //printHex(packet_buffer, packet_length);
  for (i=0;i<packet_length;i++)
    cnt += sprintf(buff+cnt, "0x%02hhx|",packet_buffer[i]);
  
  if (packet_buffer[PKT_CMD] == CMD_MSG) {
    cnt += sprintf(buff+cnt, "  Message : ");
    //fwrite(packet_buffer + 4, 1, packet_length - 4, stdout);
    strncpy(buff+cnt, (char*)packet_buffer+PKT_DATA+1, AQ_MSGLEN);
    cnt += AQ_MSGLEN;
  }
  
  cnt += sprintf(buff+cnt,"\n");

  logMessage(LOG_DEBUG_SERIAL, "%s", buff);
}
*/

int main(int argc, char *argv[]) {
  //int rs_fd;
  //int packet_length;
  //unsigned char packet_buffer[AQ_MAXPKTLEN];
  //unsigned char lastID;
  //int received_packets = 0;
  //bool ar_connected = false;
  //int no_reply = 0;
  //struct mg_mgr mgr;
  // int sent = 0;
  int i;
  bool forceConnection = false;
  //int
  //struct config arconf;
  //struct aqconfig config;
  //struct arconfig ar_prms;
  bool deamonize;

  printf("%s %s\n",AQUAPURED_NAME,AQUAPURED_VERSION);

  char *cfgFile = "aquapured.conf";

  if (getuid() != 0) {
    fprintf(stderr, "ERROR %s Can only be run as root\n", argv[0]);
    return EXIT_FAILURE;
  }

/*
  _ar_prms.PPM = TEMP_UNKNOWN;
  _ar_prms.Percent = 50;
  _ar_prms.default_percent = 50;
  _ar_prms.boost = false;
  _ar_prms.cache_file = CACHE_FILE;
  _ar_prms.changed = true;
  _ar_prms.connected = false;
*/
  // Initialize the daemon's parameters.
  //init_parameters();

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      deamonize = false;
    } else if (strcmp(argv[i], "-c") == 0) {
      cfgFile = argv[++i];
    } else if (strcmp(argv[i], "-f") == 0) {
      forceConnection = true;
    }
  }

#ifdef TESTING
  deamonize = false;
  cfgFile = "./release/aquapured.test.conf";
#endif
  init_parameters(deamonize);
  readCfg(cfgFile);

  //setLoggingPrms(_apconfig_.log_level, _apconfig_.deamonize, _apconfig_.log_file, _apconfig_.last_display_message);
  setLoggingPrms(_apconfig_.log_level, _apconfig_.deamonize, _apconfig_.log_file, NULL);

  init_swg_device(forceConnection);
  init_gpio_device();

  //read_cache(&_ar_prms);
  
//printf("MQTT = %s\n",_apconfig_.mqtt_server);

  if (_apconfig_.deamonize == true) {
    char pidfile[256];
    // sprintf(pidfile, "%s/%s.pid",PIDLOCATION, basename(argv[0]));
    sprintf(pidfile, "%s/%s.pid", "/run", basename(argv[0]));
    daemonise(pidfile, main_loop);
  } else {
    main_loop();
  }

  exit(EXIT_SUCCESS);
}

void main_loop() {
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  //int no_reply = 0;
  struct mg_mgr mgr;
  //bool sendUpdate = false;
  bool broadcast = true;
  int cnt=0;
  int size=0;
  bool wait_for_reply = false;
  int sent_loop_cnt = 0;
  
  unsigned char last_packet_sent_to = 0x00;

  logMessage(LOG_DEBUG, "Starting aquapured!\n");

  // NEED TO CHANGE THIS TO NOT PASS _apdata_
  if (!start_net_services(&mgr)) {
    logMessage(LOG_ERR, "Can not start mqtt on port\n");
    exit(EXIT_FAILURE);
  }

  rs_fd = init_serial_port(_apconfig_.serial_port);

  if (rs_fd < 0) {
    logMessage(LOG_ERR, "Can not open serial port '%s'\n",_apconfig_.serial_port);
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  mg_mgr_poll(&mgr, 500);

#ifdef TESTING
  unsigned char test_packet[] = {0x10,0x02,0x00,0x16,0x03,0x03,0x02,0x00,0x49,0x41,0x47,0x4a,0x4b,0x10,0x03};
  action_swg_message(test_packet, 14);
#endif

  //send_1byte_command(rs_fd, AR_ID, CMD_PROBE);

  //logMessage(LOG_DEBUG_SERIAL,"Send Probe\n");
  
  while (_keepRunning == true) {
//printf("AR_CONNECTED = %d\n", _ar_prms.ar_connected);
    if (rs_fd < 0) {
      logMessage(LOG_DEBUG, "ERROR, serial port disconnect\n");
      _keepRunning = false;
    }
    
    if (broadcast == false)
      broadcast = broadcast_aquapurestate(mgr.active_connections);

    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length == -1) {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_DEBUG, "ERROR, on serial port\n");
      _keepRunning = false;
    } else if (packet_length == 0 || wait_for_reply == false) {

      size = prepare_swg_message(packet_buffer, AQ_MAXPKTLEN-1);
      if (size > 0) {
        send_command(rs_fd, packet_buffer, size);
        last_packet_sent_to = packet_buffer[1];
        sent_loop_cnt = 0;
        wait_for_reply = true;
      } else {
        last_packet_sent_to = 0x00;
      }

    } else if (packet_length > 0) {
      wait_for_reply = false;
      sent_loop_cnt = 0;
      //if (getLogLevel() >= LOG_DEBUG_SERIAL)
      //  debugPacketPrint(AR_ID, packet_buffer, packet_length);

      if (packet_buffer[PKT_DEST] == 0x00) {
        if (last_packet_sent_to == AR_ID)
          action_swg_message(packet_buffer, packet_length);
      }
    } else if (sent_loop_cnt >= 3) {
      // Not waiting any longer for reply.
      sent_loop_cnt = 0;
      wait_for_reply = false;
    }

    //sleep(1);
    check_net_services(&mgr);
    
    if ( _apdata_.changed == true || _gpiodata_.changed == true ) {
      broadcast = broadcast_aquapurestate(mgr.active_connections);
      //_ar_prms.changed = false;
      set_swg_uptodate();
      set_gpio_uptodate();
    }

    mg_mgr_poll(&mgr, 500);
    
    if (cnt >= 600) {
      cnt = 0;
      if (_apdata_.connected == true)
        broadcast = false;  
    }
    cnt++;

    if (wait_for_reply)
      sent_loop_cnt++;
  }

  return;
}







/*
void main_loop() {
  int rs_fd;
  int packet_length;
  unsigned char packet_buffer[AQ_MAXPKTLEN];
  int no_reply = 0;
  struct mg_mgr mgr;
  //bool sendUpdate = false;
  bool broadcast = true;
  int cnt=0;
  unsigned char corrected_status;

  logMessage(LOG_DEBUG, "Starting aquapured!\n");

  if (!start_net_services(&mgr, &_ar_prms)) {
    logMessage(LOG_ERR, "Can not start mqtt on port\n");
    exit(EXIT_FAILURE);
  }

  rs_fd = init_serial_port(_apconfig_.serial_port);

  if (rs_fd < 0) {
    logMessage(LOG_ERR, "Can not open serial port '%s'\n",_apconfig_.serial_port);
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);

  mg_mgr_poll(&mgr, 500);

  send_1byte_command(rs_fd, AR_ID, CMD_PROBE);

  logMessage(LOG_DEBUG_SERIAL,"Send Probe\n");
  
  while (_keepRunning == true) {
//printf("AR_CONNECTED = %d\n", _ar_prms.ar_connected);
    if (rs_fd < 0) {
      logMessage(LOG_DEBUG, "ERROR, serial port disconnect\n");
      _keepRunning = false;
    }
    
    if (broadcast == false)
      broadcast = broadcast_aquapurestate(mgr.active_connections);

    packet_length = get_packet(rs_fd, packet_buffer);

    if (packet_length == -1) {
      // Unrecoverable read error. Force an attempt to reconnect.
      logMessage(LOG_DEBUG, "ERROR, on serial port\n");
      _keepRunning = false;
    } else if (packet_length == 0) {
      // Nothing read
      no_reply++;
      logMessage(LOG_DEBUG_SERIAL,"Nothing read try %d\n",no_reply);
     
      // Resend last command after X blank reads
      if ( (no_reply % RETRY_NUL_READ) == 0 ) {
        if ( _ar_prms.connected == true)
          send_3byte_command(rs_fd, AR_ID, CMD_PERCENT, (unsigned char)_ar_prms.Percent, NUL);
        else
          send_1byte_command(rs_fd, AR_ID, CMD_PROBE);
      }

      if (no_reply >= TIMEOUT_TRIES) {
        logMessage(LOG_ERR,"%d BLANK READS, Calling SWG connection dead\n",no_reply);
        if (_ar_prms.connected == true || _ar_prms.status != SWG_STATUS_OFFLINE) {
            //sendUpdate = true;
          _ar_prms.changed = true;
        }
        _ar_prms.connected = false;
        _ar_prms.status = SWG_STATUS_OFFLINE;
          
        no_reply = 0;
      }
    } else if (packet_length > 0) {
      no_reply = 0;
      if (packet_buffer[PKT_DEST] == 0x00) {
        
        _ar_prms.connected = true;
        if (getLogLevel() >= LOG_DEBUG_SERIAL)
          debugPacketPrint(AR_ID, packet_buffer, packet_length);

        switch (packet_buffer[PKT_CMD]) {
        case CMD_ACK:
          if (_forceConnection == true)
            _ar_prms.connected = true;

          if (_ar_prms.connected == false) {
            send_2byte_command(rs_fd, AR_ID, CMD_GETID, 0x01);
          } else {
            send_3byte_command(rs_fd, AR_ID, CMD_PERCENT, (unsigned char)_ar_prms.Percent, NUL);
          }
          break;
        case CMD_PPM:
          logMessage(LOG_DEBUG_SERIAL,"Received PPM %d\n", (packet_buffer[4] * 100));

          if (_ar_prms.connected != true) {
            _ar_prms.connected = true;
            _ar_prms.changed = true;
          }
          
          // If ON status but % = 0 change status to off, since off is not supported on RS485 protocol.
          corrected_status = (packet_buffer[5] == SWG_STATUS_ON && _ar_prms.Percent < 0)?SWG_STATUS_OFF:packet_buffer[5];
 
          // Has status changed.
          if (_ar_prms.status != corrected_status ) {
            _ar_prms.status = corrected_status;
            _ar_prms.changed = true;
          }

          if (_ar_prms.PPM != packet_buffer[4] * 100) {
            _ar_prms.PPM = packet_buffer[4] * 100;
            _ar_prms.changed = true;
          }

          if (getLogLevel() >= LOG_DEBUG_SERIAL && _ar_prms.status != 0x00)
            debugStatusPrint();

        case CMD_MSG: // Want to fall through
          //send_command(rs_fd, AR_ID, CMD_PERCENT, (unsigned char)_ar_prms.Percent, NUL);
          send_3byte_command(rs_fd, AR_ID, CMD_PERCENT, (unsigned char)_ar_prms.Percent, NUL);
          _ar_prms.connected = true;
          break;
        // case 0x16:
        // break;
        default:
          printf("do nothing, didn't understand return\n");
          break;
        }
      }

      //printf("Packets received %d\n", received_packets++);
    }

    //sleep(1);
    check_net_services(&mgr);
    
    if ( _ar_prms.changed == true ) {
      broadcast = broadcast_aquapurestate(mgr.active_connections);
      _ar_prms.changed = false;
    }

    mg_mgr_poll(&mgr, 500);
    
    if (cnt >= 600) {
      cnt = 0;
      if (_ar_prms.connected == true)
        broadcast = false;  
    }
    cnt ++;
  }

  return;
}
*/
