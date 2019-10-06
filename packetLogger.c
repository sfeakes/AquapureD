
#include <stdio.h>
#include <stdarg.h>

#include "packetLogger.h"
#include "aq_serial.h"
#include "utils.h"

static FILE *_packetLogFile = NULL;
static bool _log2file = false;
//static bool _includePentair = false;

void _logPacket(unsigned char *packet_buffer, int packet_length, bool error);
const char* get_jandy_packet_type(unsigned char* packet, int length);
const char* get_pentair_packet_type(unsigned char* packet, int length);


void startPacketLogger(bool debug_RSProtocol_packets) {
  _log2file = debug_RSProtocol_packets;
  //_includePentair = read_pentair_packets;
}

void stopPacketLogger() {
  if (_packetLogFile != NULL)
    fclose(_packetLogFile);
}


void writePacketLog(char *buffer) {
  if (_packetLogFile == NULL)
    _packetLogFile = fopen(RS485LOGFILE, "a");

  if (_packetLogFile != NULL) {
    fputs(buffer, _packetLogFile);
  } 
}

void logPacket(unsigned char *packet_buffer, int packet_length) {
  _logPacket(packet_buffer, packet_length, false);
}

void logPacketError(unsigned char *packet_buffer, int packet_length) {
  _logPacket(packet_buffer, packet_length, true);
}

void _logPacket(unsigned char *packet_buffer, int packet_length, bool error)
{
  // No point in continuing if loglevel is < debug_serial and not writing to file
  if ( error == false && getLogLevel() < LOG_DEBUG_SERIAL && _log2file == false)
    return;

  char buff[1000];
  int i = 0;
  int cnt = 0;

  // if first byte is 0x00 then we are sending as we pad all packets, received have already had the padding removed
  // String  "Pentair Received Message | Hex"
  /*if (error) {
    cnt = sprintf(buff, "%s%8.8s Packet | HEX: ",(error?"BAD PACKET ":""),getProtocolType(packet_buffer)==PCOL_JANDY?"Jandy":"Pentair");
  }else*/
  if (packet_buffer[0] == 0x00) {
    cnt = sprintf(buff, "%s%-7s %-8s %-10s| HEX: ",(error?"BAD PACKET ":""),getProtocolType(&packet_buffer[1])==PCOL_JANDY?"Jandy":"Pentair","Sent", get_packet_type(&packet_buffer[1], packet_length-2));
  } else {
    cnt = sprintf(buff, "%s%-7s %-8s %-10s| HEX: ",(error?"BAD PACKET ":""),getProtocolType(packet_buffer)==PCOL_JANDY?"Jandy":"Pentair","Received", get_packet_type(packet_buffer, packet_length));
  }


  //if (_includePentair) {
  //  cnt = sprintf(buff, "%s%8.8s Packet | HEX: ",(error?"BAD PACKET ":""),getProtocolType(packet_buffer)==PCOL_JANDY?"Jandy":"Pentair");
  //} else {
  //  cnt = sprintf(buff, "%sTo 0x%02hhx of type %8.8s | HEX: ",(error?"BAD PACKET ":""), packet_buffer[PKT_DEST], get_packet_type(packet_buffer, packet_length));
  //}

  for (i = 0; i < packet_length; i++)
    cnt += sprintf(buff + cnt, "0x%02hhx|", packet_buffer[i]);

  cnt += sprintf(buff + cnt, "\n");

  if (_log2file)
    writePacketLog(buff);
  
  if (error == true)
    logMessage(LOG_WARNING, "%s", buff);
  else
    logMessage(LOG_DEBUG_SERIAL, "%s", buff);
}


const char* get_packet_type(unsigned char* packet, int length)
{
  if (getProtocolType(packet) == PCOL_JANDY)
    return get_jandy_packet_type(packet, length);
  else if (getProtocolType(packet) == PCOL_PENTAIR)
    return get_pentair_packet_type(packet, length);
  
  return "Unknown";
}


const char* get_jandy_packet_type(unsigned char* packet, int length)
{
  static char buf[15];

  if (length <= 0 )
    return "";

  switch (packet[PKT_CMD]) {
    case CMD_ACK:
      return "Ack";
    break;
    case CMD_STATUS:
      return "Status";
    break;
    case CMD_MSG:
    case CMD_MSG_LONG:
      return "Message";
    break;
    case CMD_PROBE:
      return "Probe";
    break;
    case CMD_GETID:
      return "GetID";
    break;
    case CMD_PERCENT:
      return "AR %%";
    break;
    case CMD_PPM:
      return "AR PPM";
    break;
    default:
      sprintf(buf, "Unknown '0x%02hhx'", packet[PKT_CMD]);
      return buf;
    break;
  }
}

const char* get_pentair_packet_type(unsigned char* packet, int length)
{
  return "Unknown";
}