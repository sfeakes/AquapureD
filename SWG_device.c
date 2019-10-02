#define SWG_DEVICE_C_

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>


#include "SWG_device.h"
#include "utils.h"
#include "aq_serial.h"
#include "aqualink.h"

#define CACHE_FILE "/tmp/aquapure.cache"
#define CMD_NONE 0xFF

struct apdata _apdata_;
unsigned char _next_msg = CMD_PROBE;
unsigned char _last_msg = CMD_NONE;
bool _forceConnection = false;



void debugStatusPrint();


void init_swg_device(bool forceConnection) {
  _apdata_.PPM = TEMP_UNKNOWN;
  _apdata_.Percent = 50;
  _apdata_.default_percent = 50;
  _apdata_.boost = false;
  _apdata_.cache_file = CACHE_FILE;
  _apdata_.changed = true;
  _apdata_.connected = false;

  _forceConnection = forceConnection;
  read_swg_cache();
}

int _prepare_swg_message(unsigned char type, unsigned char *packet_buffer, int packet_length)
{
  int length = 0;
  packet_buffer[length++] = PCOL_JANDY;
  packet_buffer[length++] = AR_ID;

  switch (type) {
    case CMD_PROBE:
      packet_buffer[length++] = CMD_PROBE;
    break;
    case CMD_GETID:
      packet_buffer[length++] = CMD_GETID;
      packet_buffer[length++] = 0x01;
    break;
    case CMD_PERCENT:
      packet_buffer[length++] = CMD_PERCENT;
      packet_buffer[length++] = (unsigned char)_apdata_.Percent;
      //packet_buffer[length++] = NUL;
    break;
    default:
      length = 0;
    break;
  }

  return length;
}

int prepare_swg_message(unsigned char *packet_buffer, int packet_length)
{
  
  static struct timeval last , now;
  static int blank_calls=0;
  int rtn = 0;
	gettimeofday(&now , NULL);

  //logMessage(LOG_DEBUG, "prepare_swg_Message() time diff = %lf\n",timval_diff(last, now));
  
  // Only send command a maximum of every 1.5 second.
  if (timval_diff(last, now) < 1.0) {
    //logMessage(LOG_DEBUG, "prepare_swg_Message() interval too short\n");
    return 0;
  }

  switch (_next_msg) {
    case CMD_PROBE:
    case CMD_GETID:
    case CMD_PERCENT:
      rtn = _prepare_swg_message(_next_msg, packet_buffer, packet_length);
    break;
    case CMD_NONE:
      if (++blank_calls >= 5) {
        logMessage(LOG_ERR, "Too many no-reply from SWG, calling connection dead\n");
        if (_apdata_.connected == true ||  _apdata_.status != SWG_STATUS_OFFLINE) {
          _apdata_.connected = false;
          _apdata_.status = SWG_STATUS_OFFLINE;
          _apdata_.changed = true;
        }
        _next_msg = CMD_PROBE;
        blank_calls = 0;
        rtn=0;
      } else {
        // Resend the last message.
        logMessage(LOG_DEBUG, "SWG resend last message\n");
        rtn = _prepare_swg_message(_last_msg, packet_buffer, packet_length);
        rtn = _prepare_swg_message(CMD_PERCENT, packet_buffer, packet_length);
      }
      //length = 0;
      // We didn't receive reply from last message.
    break;
    default:
     logMessage(LOG_ERR, "Not sure how we got here\n");
      rtn = 0;
    break;
  }

  if (_next_msg != CMD_NONE) {
    blank_calls = 0;
    _last_msg = _next_msg;
    _next_msg = CMD_NONE;
  }

  if (gettimeofday(&last , NULL) ) {
    logMessage(LOG_ERR, "SWG Timer reset failed\n");
  }

  return rtn;
}

void action_swg_message(unsigned char *packet_buffer, int packet_length)
{
  unsigned char corrected_status;

  _apdata_.connected = true;
  
  switch (packet_buffer[PKT_CMD])
  {
    case CMD_ACK:
      if (_forceConnection == true)
        _apdata_.connected = true;

      if (_apdata_.connected == false)
        _next_msg = CMD_GETID;
      else
        _next_msg = CMD_PERCENT;
      break;
    case CMD_PPM:
      logMessage(LOG_DEBUG_SERIAL, "Received PPM %d\n", (packet_buffer[4] * 100));

      if (_apdata_.connected != true)
      {
        _apdata_.connected = true;
        _apdata_.changed = true;
      }

      // If ON status but % = 0 change status to off, since off is not supported on RS485 protocol.
      corrected_status = (packet_buffer[5] == SWG_STATUS_ON && _apdata_.Percent < 0) ? SWG_STATUS_OFF : packet_buffer[5];

      // Has status changed.
      if (_apdata_.status != corrected_status)
      {
        _apdata_.status = corrected_status;
        _apdata_.changed = true;
      }

      if (_apdata_.PPM != packet_buffer[4] * 100)
      {
        _apdata_.PPM = packet_buffer[4] * 100;
        _apdata_.changed = true;
      }

      if (getLogLevel() >= LOG_DEBUG_SERIAL && _apdata_.status != 0x00)
        debugStatusPrint();

    case CMD_MSG: // Want to fall through
      //send_command(rs_fd, AR_ID, CMD_PERCENT, (unsigned char)_ar_prms.Percent, NUL);
      //send_3byte_command(rs_fd, AR_ID, CMD_PERCENT, (unsigned char)_apdata_.Percent, NUL);
      _next_msg = CMD_PERCENT;
      _apdata_.connected = true;
      break;
    // case 0x16:
    // break;
    default:
      printf("do nothing, didn't understand return\n");
      break;
  }
}

void set_swg_uptodate() {
   _apdata_.changed = false;
}

void set_swg_req_percent(char *sval, bool f2c) {
  float value = atof(sval);

  if (f2c) {
    value = degCtoF(value);
  }

  set_swg_percent(round(value));
}

void set_swg_percent(int percent) {

  if (_apdata_.connected != true) {
    logMessage(LOG_ERR, "Can't set Percent %d, SWG not connected\n",percent);
     _apdata_.changed = true;
    return;
  } else if (_apdata_.boost == true) {
    logMessage(LOG_ERR, "Can't set Percent %d, SWG Boost is active\n",percent);
     _apdata_.changed = true;
    return;
  }

  if (percent > 100)
      percent = 100;
  else if (percent < 0)
      percent = 0;

  _apdata_.Percent = percent;

  if (percent > 0 && percent < 101)
    _apdata_.last_generating_percent = percent;
   //_apdata_.Percent = round(degCtoF(value));
  logMessage(LOG_INFO, "Setting SWG percent to %d", _apdata_.Percent);
    //broadcast_aquapurestate(nc);
  write_swg_cache();
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
        set_swg_percent(_apdata_.last_generating_percent);
      else
        set_swg_percent(0);
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
      set_swg_percent(_apdata_.last_generating_percent);
    else
      set_swg_percent(_apdata_.default_percent);
  }

  if (val==false) {
    if (_apdata_.Percent == 0) {
      logMessage(LOG_NOTICE, "Can't turn SWG %s, SWG is already %s\n",(val==true?"on":"off"),(_apdata_.Percent>0?"on":"off"));
      return;
    }
    set_swg_percent(0);
  }

  _apdata_.changed = true;
}


void write_swg_cache() {
  FILE *fp;

  fp = fopen(_apdata_.cache_file, "w");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", _apdata_.cache_file);
    return;
  }

  fprintf(fp, "%d\n", _apdata_.Percent);
  fprintf(fp, "%d\n", _apdata_.PPM);
  fclose(fp);
}

void read_swg_cache() {
  FILE *fp;
  int i;

  fp = fopen(_apdata_.cache_file, "r");
  if (fp == NULL) {
    logMessage(LOG_ERR, "Open file failed '%s'\n", _apdata_.cache_file);
    return;
  }

  fscanf (fp, "%d", &i);
  logMessage(LOG_DEBUG, "Read Percent '%d' from cache\n", i);
  if (i > -1 && i< 102)
    _apdata_.Percent = i;

  fscanf (fp, "%d", &i);
  logMessage(LOG_DEBUG, "Read PPM '%d' from cache\n", i);
  if (i > -1 && i< 5000)
    _apdata_.PPM = i;

  fclose (fp);
}



void debugStatusPrint() {
  if (_apdata_.status == SWG_STATUS_ON) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - OK ***\n");
  } else if (_apdata_.status == SWG_STATUS_NO_FLOW) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - NO FLOW ***\n");
  } else if (_apdata_.status == SWG_STATUS_LOW_SALT) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - LOW SALT ***\n");
  } else if (_apdata_.status == SWG_STATUS_HI_SALT) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - HIGH SALT ***\n");
  } else if (_apdata_.status == SWG_STATUS_HIGH_CURRENT) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - HIGH CURRENT ***\n");
  } else if (_apdata_.status == SWG_STATUS_TURNING_OFF) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - TURNING OFF ***\n");
  } else if (_apdata_.status == SWG_STATUS_CLEAN_CELL) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - CLEAN CELL ***\n");
  } else if (_apdata_.status == SWG_STATUS_LOW_VOLTS) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - LOW VOLTAGE ***\n");
  } else if (_apdata_.status == SWG_STATUS_LOW_TEMP) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - WATER TEMP LOW ***\n");
  } else if (_apdata_.status == SWG_STATUS_CHECK_PCB) {
    logMessage(LOG_DEBUG_SERIAL, "*** Received status - CHECK PCB ***\n");
  }
}

