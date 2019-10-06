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
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <string.h>

#include "aq_serial.h"
#include "utils.h"
#include "packetLogger.h"

//#define BLOCKING_MODE
#define PENTAIR_LENGTH_FIX
static struct termios _oldtio;

void send_packet(int fd, unsigned char *packet, int length);
unsigned char getProtocolType(unsigned char* packet);

// Generate and return checksum of packet.
int generate_checksum(unsigned char* packet, int length)
{
  int i, sum, n;

  n = length - 3;
  sum = 0;
  for (i = 0; i < n; i++)
  sum += (int) packet[i];
  return(sum & 0x0ff);
}

bool check_jandy_checksum(unsigned char* packet, int length)
{
  if (generate_checksum(packet, length) == packet[length-3])
    return true;

  return false;
}

bool check_pentair_checksum(unsigned char* packet, int length)
{
  printf("check_pentair_checksum \n");
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 8;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  //printf("Check High 0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n], packet[length-2],((sum >> 8) & 0xFF) );
  //printf("Check Low  0x%02hhx = 0x%02hhx = 0x%02hhx\n",packet[n + 1], packet[length-1], (sum & 0xFF) ); 

  // Check against caculated length
  if (sum == (packet[length-2] * 256 + packet[length-1]))
    return true;

  // Check against actual # length
  if (sum == (packet[n] * 256 + packet[n+1])) {
    logMessage(LOG_ERR, "Pentair checksum is accurate but length is not\n");
    return true;
  }
  
  return false;
}


void generate_pentair_checksum(unsigned char* packet, int length)
{
  int i, sum, n;
  n = packet[8] + 9;
  //n = packet[8] + 6;
  sum = 0;
  for (i = 3; i < n; i++) {
    //printf("Sum 0x%02hhx\n",packet[i]);
    sum += (int) packet[i];
  }

  packet[n+1] = (unsigned char) (sum & 0xFF);        // Low Byte
  packet[n] = (unsigned char) ((sum >> 8) & 0xFF); // High Byte

}


unsigned char getProtocolType(unsigned char* packet) {
  if (packet[0] == DLE)
    return PCOL_JANDY;
  else if (packet[0] == PP1)
    return PCOL_PENTAIR;

  return PCOL_UNKNOWN; 
}


#ifndef PLAYBACK_MODE
/*
Open and Initialize the serial communications port to the Aqualink RS8 device.
Arg is tty or port designation string
returns the file descriptor
*/
int init_serial_port(const char* tty)
{
  long BAUD = B9600;
  long DATABITS = CS8;
  long STOPBITS = 0;
  long PARITYON = 0;
  long PARITY = 0;

  struct termios newtio;       //place for old and new port settings for serial port

  //int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK);
  int fd = open(tty, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
  if (fd < 0)  {
    logMessage(LOG_ERR, "Unable to open port: %s\n", tty);
    return -1;
  }

  logMessage(LOG_DEBUG_SERIAL, "Openeded serial port %s\n",tty);
  
#ifdef BLOCKING_MODE
  fcntl(fd, F_SETFL, 0);
  newtio.c_cc[VMIN]= 1;
  newtio.c_cc[VTIME]= 0;
  logMessage(LOG_DEBUG_SERIAL, "Set serial port %s to blocking mode\n",tty);
#else
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK | O_NDELAY);
  newtio.c_cc[VMIN]= 0;
  newtio.c_cc[VTIME]= 1;
  logMessage(LOG_DEBUG_SERIAL, "Set serial port %s to non blocking mode\n",tty);
#endif

  tcgetattr(fd, &_oldtio); // save current port settings
    // set new port settings for canonical input processing
  newtio.c_cflag = BAUD | DATABITS | STOPBITS | PARITYON | PARITY | CLOCAL | CREAD;
  newtio.c_iflag = IGNPAR;
  newtio.c_lflag = 0;       // ICANON;  
  newtio.c_oflag = 0;
    
  tcflush(fd, TCIFLUSH);
  tcsetattr(fd, TCSANOW, &newtio);
  
  logMessage(LOG_DEBUG_SERIAL, "Set serial port %s io attributes\n",tty);

  return fd;
}

/* close tty port */
void close_serial_port(int fd)
{
  tcsetattr(fd, TCSANOW, &_oldtio);
  close(fd);
  logMessage(LOG_DEBUG_SERIAL, "Closed serial port\n");
}







// Send an ack packet to the Aqualink RS8 master device.
// fd: the file descriptor of the serial port connected to the device
// command: the command byte to send to the master device, NUL if no command
// 
// NUL = '\x00'
// DLE = '\x10'
// STX = '\x02'
// ETX = '\x03'
// 
// masterAddr = '\x00'          # address of Aqualink controller
// 
//msg = DLE+STX+dest+cmd+args
//msg = msg+self.checksum(msg)+DLE+ETX
//      DLE+STX+DEST+CMD+ARGS+CHECKSUM+DLE+ETX


void print_hex(char *pk, int length)
{
  int i=0;
  for (i=0;i<length;i++)
  {
    printf("0x%02hhx|",pk[i]);
  }
  printf("\n");
}

/*
void test_cmd()
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //send_cmd(fd, CMD_ACK, command);
  
  print_hex((char *)ackPacket, length);
  
  ackPacket[7] = generate_checksum(ackPacket, length-1);
  print_hex((char *)ackPacket, length);
  
  ackPacket[6] = 0x02;
  ackPacket[7] = generate_checksum(ackPacket, length-1);
  print_hex((char *)ackPacket, length);
}
*/
/*
void send_test_cmd(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3)
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  ackPacket[3] = destination;
  ackPacket[4] = b1;
  ackPacket[5] = b2;
  ackPacket[6] = b3;
  ackPacket[7] = generate_checksum(ackPacket, length-1);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      logMessage(LOG_ERR, "write to serial port failed\n");
  }
  //logMessage(LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
  //logMessage(LOG_DEBUG, "Send '0x%02hhx' to '0x%02hhx'\n", command, destination);
#endif  

  log_packet("Sent ", ackPacket, length);
}
*/
void send_1byte_command(int fd, unsigned char destination, unsigned char b1)
{
  int length = 9;
  unsigned char packet[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, 0x13, DLE, ETX, NUL };
  packet[3] = destination;
  packet[4] = b1;
  packet[5] = generate_checksum(packet, length-1);

  send_packet(fd, packet, length);
}

void send_2byte_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2)
{
  int length = 10;
  unsigned char packet[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, 0x13, DLE, ETX, NUL };
  packet[3] = destination;
  packet[4] = b1;
  packet[5] = b2;
  packet[6] = generate_checksum(packet, length-1);

  send_packet(fd, packet, length);
}

void send_3byte_command(int fd, unsigned char destination, unsigned char b1, unsigned char b2, unsigned char b3)
{
  int length = 11;
  unsigned char packet[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  packet[3] = destination;
  packet[4] = b1;
  packet[5] = b2;
  packet[6] = b3;
  packet[7] = generate_checksum(packet, length-1);

  send_packet(fd, packet, length);
}

/*
 unsigned char tp[] = {PCOL_PENTAIR, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00};
 send_command(0, tp, 19);
 Should produce
{0xFF, 0x00, 0xFF, 0xA5, 0x07, 0x0F, 0x10, 0x08, 0x0D, 0x55, 0x55, 0x5B, 0x2A, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x02, 0x9E};
 <-------  headder ----> <-- type to from type-> <len> <------------------------------ data ----------------------------------------> <checksum>
*/

void send_pentair_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;

  packet[0] = NUL;
  packet[1] = PP1;
  packet[2] = PP2;
  packet[3] = PP3;
  packet[4] = PP4;

  //packet[i++] = 0x00; // from
  //packet[i++] = // to
  for (i=5; i-4 < size; i++) {
    //printf("added 0x%02hhx at position %d\n",packet_buffer[i-4],i);
    if (i==6) {
      // Replace source
      packet[i] = 0x00;
    } else if (i==9) {
      // Replace length
      //packet[i] = 0xFF;
      packet[i] = (unsigned char)size-6;
    } else {
      packet[i] = packet_buffer[i-4];
    }

    //packet[i] = packet_buffer[i-4];    
  }

  packet[++i] = NUL;  // Checksum
  packet[++i] = NUL;  // Checksum
  generate_pentair_checksum(&packet[1], i);
  packet[++i] = NUL;


  //logPacket(packet, i);
  send_packet(fd,packet,i);
}

void send_command(int fd, unsigned char *packet_buffer, int size)
{
  unsigned char packet[AQ_MAXPKTLEN];
  int i=0;
  
  if (packet_buffer[0] != PCOL_JANDY) {
    logMessage(LOG_ERR, "Only Jandy protocol supported at present!\n");
    send_pentair_command(fd, packet_buffer, size);
    return;
  }

  packet[0] = NUL;
  packet[1] = DLE;
  packet[2] = STX;

  for (i=3; i-2 < size; i++) {
    //printf("added 0x%02hhx at position %d\n",packet_buffer[i-2],i);
    packet[i] = packet_buffer[i-2];
  }

  packet[++i] = DLE;
  packet[++i] = ETX;
  packet[++i] = NUL;

  packet[i-3] = generate_checksum(packet, i);

  send_packet(fd,packet,++i);
}

void send_packet(int fd, unsigned char *packet, int length)
{

  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, packet + i, length - i);
    if (nwrite < 0) 
      logMessage(LOG_ERR, "write to serial port failed\n");
  }

  if ( getLogLevel() >= LOG_DEBUG_SERIAL) {
    //char buf[30];
    //sprintf(buf, "Sent     %8.8s ", get_packet_type(packet+1, length));
    //log_packet(buf, packet, length);
    logPacket(packet, length);
  }
}


void send_ack(int fd, unsigned char command)
{
  const int length = 11;
  unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, CMD_ACK, NUL, NUL, 0x13, DLE, ETX, NUL };
  //unsigned char ackPacket[] = { NUL, DLE, STX, DEV_MASTER, NUL, NUL, NUL, 0x13, DLE, ETX, NUL };

  // Update the packet and checksum if command argument is not NUL.
  if(command != NUL) {
    ackPacket[6] = command;
    ackPacket[7] = generate_checksum(ackPacket, length-1);
  }

  send_packet(fd, ackPacket, length);
/*
  // Send the packet to the master device.
  //write(fd, ackPacket, length);
  //logMessage(LOG_DEBUG, "Send '0x%02hhx' to controller\n", command);

#ifdef BLOCKING_MODE
  write(fd, ackPacket, length);
#else
  int nwrite, i;
  for (i=0; i<length; i += nwrite) {        
    nwrite = write(fd, ackPacket + i, length - i);
    if (nwrite < 0) 
      logMessage(LOG_ERR, "write to serial port failed\n");
  }
  logMessage(LOG_DEBUG_SERIAL, "Send %d bytes to serial\n",length);
  //tcdrain(fd);
#endif  
  
  //log_packet("Sent ", ackPacket, length);
  logPacket( ackPacket, length);
  */
}


int get_packet(int fd, unsigned char* packet)
{
  unsigned char byte;
  int bytesRead;
  int index = 0;
  bool endOfPacket = false;
  //bool packetStarted = FALSE;
  bool lastByteDLE = false;
  int retry = 0;
  bool jandyPacketStarted = false;
  bool pentairPacketStarted = false;
  //bool lastByteDLE = false;
  int PentairPreCnt = 0;
  int PentairDataCnt = -1;

  // Read packet in byte order below
  // DLE STX ........ ETX DLE
  // sometimes we get ETX DLE and no start, so for now just ignoring that.  Seem to be more applicable when busy RS485 traffic

  while (!endOfPacket) {
    bytesRead = read(fd, &byte, 1);
    //if (bytesRead < 0 && errno == EAGAIN && packetStarted == FALSE && lastByteDLE == FALSE) {
    if (bytesRead < 0 && errno == EAGAIN && 
        jandyPacketStarted == false && 
        pentairPacketStarted == false && 
        lastByteDLE == false) {
      // We just have nothing to read
      return 0;
    } else if (bytesRead < 0 && errno == EAGAIN) {
      // If we are in the middle of reading a packet, keep going
      if (retry > 20) {
        logMessage(LOG_WARNING, "Serial read timeout\n");
        //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
        logPacketError(packet, index);
        return 0;
      }
      retry++;
      delay(10);
 #ifdef TESTING
    } else if (bytesRead == 0 && jandyPacketStarted == false && pentairPacketStarted == false) {
      // Probably set port to /dev/null for testing.
      //printf("Read loop return\n");
      return 0;
  #endif
    } else if (bytesRead == 1) {

      if (lastByteDLE == true && byte == NUL)
      {
        // Check for DLE | NULL (that's escape DLE so delete the NULL)
        //printf("IGNORE THIS PACKET\n");
        lastByteDLE = false;
      }
      else if (lastByteDLE == true)
      {
        if (index == 0)
          index++;

        packet[index] = byte;
        index++;
        if (byte == STX && jandyPacketStarted == false)
        {
          jandyPacketStarted = true;
          pentairPacketStarted = false;
        }
        else if (byte == ETX && jandyPacketStarted == true)
        {
          endOfPacket = true;
        }
      }
      else if (jandyPacketStarted || pentairPacketStarted)
      {
        packet[index] = byte;
        index++;
        if (pentairPacketStarted == true && index == 9)
        {
          //printf("Read 0x%02hhx %d pentair\n", byte, byte);
          PentairDataCnt = byte;
        }
        if (PentairDataCnt >= 0 && index - 11 >= PentairDataCnt && pentairPacketStarted == true)
        {
          endOfPacket = true;
          PentairPreCnt = -1;
        }
      }
      else if (byte == DLE && jandyPacketStarted == false)
      {
        packet[index] = byte;
      }

      // // reset index incase we have EOP before start
      if (jandyPacketStarted == false && pentairPacketStarted == false)
      {
        index = 0;
      }

      if (byte == DLE && pentairPacketStarted == false)
      {
        lastByteDLE = true;
        PentairPreCnt = -1;
      }
      else
      {
        lastByteDLE = false;
        if (byte == PP1 && PentairPreCnt == 0)
          PentairPreCnt = 1;
        else if (byte == PP2 && PentairPreCnt == 1)
          PentairPreCnt = 2;
        else if (byte == PP3 && PentairPreCnt == 2)
          PentairPreCnt = 3;
        else if (byte == PP4 && PentairPreCnt == 3)
        {
          pentairPacketStarted = true;
          jandyPacketStarted = false;
          PentairDataCnt = -1;
          packet[0] = PP1;
          packet[1] = PP2;
          packet[2] = PP3;
          packet[3] = byte;
          index = 4;
        }
        else if (byte != PP1) // Don't reset counter if multiple PP1's
          PentairPreCnt = 0;
      }
    } else if(bytesRead < 0) {
      // Got a read error. Wait one millisecond for the next byte to
      // arrive.
      logMessage(LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }

    // Break out of the loop if we exceed maximum packet
    // length.
    if (index >= AQ_MAXPKTLEN) {
      logPacketError(packet, index);
      logMessage(LOG_WARNING, "Serial packet too large\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
      break;
    }
  }

  //logMessage(LOG_DEBUG, "Serial checksum, length %d got 0x%02hhx expected 0x%02hhx\n", index, packet[index-3], generate_checksum(packet, index));
  if (jandyPacketStarted) {
    if (check_jandy_checksum(packet, index) != true){
      logPacketError(packet, index);
      logMessage(LOG_WARNING, "Serial read bad Jandy checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  } else if (pentairPacketStarted) {
    if (check_pentair_checksum(packet, index) != true){
      logPacketError(packet, index);
      logMessage(LOG_WARNING, "Serial read bad Pentair checksum, ignoring\n");
      //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
      return 0;
    }
  }
/* 
  if (generate_checksum(packet, index) != packet[index-3]){
    logMessage(LOG_WARNING, "Serial read bad checksum, ignoring\n");
    log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  } else*/ if (index < AQ_MINPKTLEN && (jandyPacketStarted || pentairPacketStarted) ) { //NSF. Sometimes we get END sequence only, so just ignore.
    logPacketError(packet, index);
    logMessage(LOG_WARNING, "Serial read too small\n");
    //log_packet(LOG_WARNING, "Bad receive packet ", packet, index);
    return 0;
  }

  logMessage(LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  logPacket(packet, index);
  // Return the packet length.
  return index;
}
































// Reads the bytes of the next incoming packet, and
// returns when a good packet is available in packet
// fd: the file descriptor to read the bytes from
// packet: the unsigned char buffer to store the bytes in
// returns the length of the packet
int get_packet_OLD(int fd, unsigned char* packet)
{
  unsigned char byte;
  int bytesRead;
  int index = 0;
  int endOfPacket = FALSE;
  int packetStarted = FALSE;
  int foundDLE = FALSE;
  bool started = FALSE;
  int retry=0;

  while (!endOfPacket) {
    //printf("Read loop %d\n",i);
    //if (i++ > 10)
    //  return 0;

    bytesRead = read(fd, &byte, 1);

    //printf("Read loop size %d rtn %d\n",bytesRead, errno);

    if (bytesRead < 0 && errno == EAGAIN && started == FALSE) {
      // We just have nothing to read
      return 0;
  #ifdef TESTING
    } else if (bytesRead == 0 && started == FALSE) {
      // Probably set port to /dev/null for testing.
      //printf("Read loop return\n");
      return 0;
  #endif
    } else if (bytesRead < 0 && errno == EAGAIN) {
      // If we are in the middle of reading a packet, keep going
      if (retry > 10)
        return 0;
        
      retry++;
      delay(10);
    } else if (bytesRead == 1) {
      started = TRUE;
      //if (bytesRead == 1) {
      if (byte == DLE) {
        // Found a DLE byte. Set the flag, and record the byte.
        foundDLE = TRUE;
        packet[index] = byte;
      }
      else if (byte == STX && foundDLE == TRUE) {
        // Found the DLE STX byte sequence. Start of packet detected.
        // Reset the DLE flag, and record the byte.
        foundDLE = FALSE;
        packetStarted = TRUE;
        packet[index] = byte;
      }
      else if (byte == NUL && foundDLE == TRUE) {
        // Found the DLE NUL byte sequence. Detected a delimited data byte.
        // Reset the DLE flag, and decrement the packet index to offset the
        // index increment at the end of the loop. The delimiter, [NUL], byte
        // is not recorded.
        foundDLE = FALSE;
        //trimmed = true;
        index--;
      }
      else if (byte == ETX && foundDLE == TRUE) {
        // Found the DLE ETX byte sequence. End of packet detected.
        // Reset the DLE flag, set the end of packet flag, and record
        // the byte.
        foundDLE = FALSE;
        packetStarted = FALSE;
        endOfPacket = TRUE;
        packet[index] = byte;
      }
      else if (packetStarted == TRUE) {
        // Found a data byte. Reset the DLE flag just in case it is set
        // to prevent anomalous detections, and record the byte.
        foundDLE = FALSE;
        packet[index] = byte;
      }
      else {
        // Found an extraneous byte. Probably a NUL between packets.
        // Ignore it, and decrement the packet index to offset the
        // index increment at the end of the loop.
        index--;
      }

      // Finished processing the byte. Increment the packet index for the
      // next byte.
      index++;

      // Break out of the loop if we exceed maximum packet
      // length.
      if (index >= AQ_MAXPKTLEN-1) {
        break;
      }
    }
    else if(bytesRead < 0) {
      // Got a read error. Wait one millisecond for the next byte to
      // arrive.
      logMessage(LOG_WARNING, "Read error: %d - %s\n", errno, strerror(errno));
      if(errno == 9) {
        // Bad file descriptor. Port has been disconnected for some reason.
        // Return a -1.
        return -1;
      }
      delay(100);
    }
  }
  
  if (generate_checksum(packet, index) != packet[index-3]){
    logMessage(LOG_WARNING, "Serial read bad checksum, ignoring\n");
    //log_packet("Bad packet ", packet, index);
    logPacketError(packet, index);
    return 0;
  } else if (index < AQ_MINPKTLEN) {
    logMessage(LOG_WARNING, "Serial read too small\n");
    //log_packet("Bad packet ", packet, index);
    logPacketError(packet, index);
    return 0;
  }

  logMessage(LOG_DEBUG_SERIAL, "Serial read %d bytes\n",index);
  // Return the packet length.
  return index;
}


#else // PLAYBACK_MODE

#endif // PLAYBACK_MODE
