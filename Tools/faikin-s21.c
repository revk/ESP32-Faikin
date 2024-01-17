/* Daikin conditioner simulator for S21 protocol testing */

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ctype.h>
#include <popt.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#include "main/daikin_s21.h"

#ifdef WIN32
#include <windows.h>
#else
#include <termios.h>
#endif

int   debug = 0,
	  ump = 0;
int   dump = 0;
int   power = 0;
int   mode = 3;
int   comp = 1;
float temp = 22.5;
int   fan = 3;
int   swing = 0;
int   powerful = 0;
int   eco = 0;
int   home = 245; // Multiplied by 10
int   outside = 205;
int   inlet = 185;

static void hexdump_raw(const unsigned char *buf, unsigned int len)
{
   for (int i = 0; i < len; i++)
      printf(" %02X", buf[i]);
   printf("\n");
}

static void hexdump(const char *header, const unsigned char *buf, unsigned int len)
{
   if (dump) {
      printf("%s:", header);
      hexdump_raw(buf, len);
   }
}

static void serial_write(int p, const unsigned char *response, unsigned int pkt_len)
{
   int l;

   hexdump("Tx", response, pkt_len);

   l = write(p, response, pkt_len);

   if (l < 0) {
	  perror("Serial write failed");
	  exit(255);
   }
   if (l != pkt_len) {
	  fprintf(stderr, "Serial write failed; %d bytes instead of %d\n", l, pkt_len);
	  exit(255);
   }
}

static void s21_nak(int p, unsigned char *buf)
{
   static unsigned char response = NAK;

   printf(" -> Unknown command, sending NAK\n");
   serial_write(p, &response, 1);
   
   buf[0] = 0; // Clear read buffer
}

static void s21_ack(int p)
{
   static unsigned char response = ACK;

   serial_write(p, &response, 1);
}

static void s21_reply(int p, unsigned char *response, const unsigned char *cmd, int payload_len)
{
   int pkt_len = S21_MIN_PKT_LEN + payload_len;
   int l;

   s21_ack(p); // Send ACK before the reply

   response[S21_STX_OFFSET]  = STX;
   response[S21_CMD0_OFFSET] = cmd[S21_CMD0_OFFSET] + 1;
   response[S21_CMD1_OFFSET] = cmd[S21_CMD1_OFFSET];
   response[S21_PAYLOAD_OFFSET + payload_len] = s21_checksum(response, pkt_len);
   response[S21_PAYLOAD_OFFSET + payload_len + 1] = ETX;

   serial_write(p, response, pkt_len);
}

static void send_temp(int p, unsigned char *response, const unsigned char *cmd, int value)
{
	char buf[5];
	
	snprintf(buf, sizeof(buf), "%+d", value);
	if (debug)
	   printf(" -> '%c' sensor = %s\n", cmd[2], buf);

    // A decimal value from sensor is sent as ASCII value with sign,
	// spelled backwards for some reason. One decimal place is assumed.
	response[3] = buf[3];
	response[4] = buf[2];
	response[5] = buf[1];
	response[6] = buf[0];
	
	s21_reply(p, response, cmd, S21_PAYLOAD_LEN);
}

int
main(int argc, const char *argv[])
{
   const char     *port = NULL;
   poptContext     optCon;
   const struct poptOption optionsTable[] = {
	  {"port", 'p', POPT_ARG_STRING, &port, 0, "Port", "/dev/cu.usbserial..."},
	  {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
	  {"on", 0, POPT_ARG_NONE, &power, 0, "Power on"},
	  {"mode", 0, POPT_ARG_INT, &mode, 0, "Mode", "0=F,1=H,2=C,3=A,7=D"},
	  {"fan", 0, POPT_ARG_INT, &fan, 0, "Fan", "0 = auto, 1-5 = set speed, 6 = quiet"},
	  {"temp", 0, POPT_ARG_FLOAT, &temp, 0, "Temp", "C"},
	  {"comp", 0, POPT_ARG_INT, &comp, 0, "Comp", "1=H,2=C"},
	  {"powerful", 0, POPT_ARG_NONE, &powerful, 0, "Debug"},
	  {"dump", 'V', POPT_ARG_NONE, &dump, 0, "Dump"},
	  POPT_AUTOHELP {}
   };

   optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
   //poptSetOtherOptionHelp(optCon, "");

   int             c;
   if ((c = poptGetNextOpt(optCon)) < -1) {
      fprintf(stderr, "%s: %s\n", poptBadOption(optCon, POPT_BADOPTION_NOALIAS), poptStrerror(c));
      exit(255);
   }

   if (poptPeekArg(optCon) || !port)
   {
      poptPrintUsage(optCon, stderr, 0);
      return -1;
   }
   poptFreeContext(optCon);

   int p = open(port, O_RDWR);

   if (p < 0) {
      fprintf(stderr, "Cannot open %s: %s", port, strerror(errno));
	  exit(255);
   }
#ifdef WIN32
   DCB dcb = {0};
   
   dcb.DCBlength = sizeof(dcb);
   dcb.BaudRate  = CBR_2400;
   dcb.fBinary   = TRUE;
   dcb.fParity   = TRUE;
   dcb.ByteSize  = 8;
   dcb.Parity    = EVENPARITY;
   dcb.StopBits  = TWOSTOPBITS;
   
   if (!SetCommState((HANDLE)_get_osfhandle(p), &dcb)) {
      fprintf(stderr, "Failed to set port parameters\n");
	  exit(255);
   }
#else
   struct termios  t;
   if (tcgetattr(p, &t) < 0)
      err(1, "Cannot get termios");
   cfsetspeed(&t, 2400);
   t.c_cflag = CREAD | CS8 | PARENB | CSTOPB;
   if (tcsetattr(p, TCSANOW, &t) < 0)
      err(1, "Cannot set termios");
   usleep(100000);
   tcflush(p, TCIOFLUSH);
#endif

   unsigned char buf[256];
   unsigned char response[256];
   unsigned char chksum;

   buf[0] = 0;

   while (1)
   {
	  // Carry over STX from the previous iteration
	  int len = buf[0] == STX ? 1 : 0;

      while (len < sizeof(buf))
      {
         int l = read(p, buf + len, 1);

		 if (l < 0) {
		    perror("Error reading from serial port");
			exit(255);
		 }
		 if (l == 0)
		    continue;
		 if (len == 0 && *buf != STX) {
			printf("Garbage byte received: 0x%02X\n", *buf);
			continue;
		 }
         len += l;
		 if (buf[len - 1] == ETX)
			break;
      }
      if (!len)
         continue;
	 
	  hexdump("Rx", buf, len);

      chksum = s21_checksum(buf, len);
      if (chksum != buf[len - 2]) {
		 printf("Bad checksum: 0x%02X vs 0x%02X\n", chksum, buf[len - 2]);
		 buf[0] = 0; // Just silently drop the packet. My FTXF20D does this.
		 continue;
	  }

      if (debug)
         printf("Got command: %c%c\n", buf[1], buf[2]);

	  if (buf[1] == 'D') {
		 // Set value. No response expected, just ACK.
		 s21_ack(p);

		 switch (buf[2]) {
	     case '1':
		    power = buf[S21_PAYLOAD_OFFSET + 0] - '0'; // ASCII char
			mode  = buf[S21_PAYLOAD_OFFSET + 1] - '0'; // See AC_MODE_*
			temp  = s21_decode_target_temp(buf[S21_PAYLOAD_OFFSET + 2]);
			fan   = s21_decode_fan(buf[S21_PAYLOAD_OFFSET + 3]);

			printf(" Set power %d mode %d temp %.1f fan %d\n", power, mode, temp, fan);
			break;
		 case '5':
		    swing = buf[S21_PAYLOAD_OFFSET + 0] - '0'; // ASCII char
			// Payload offset 1 equals to '?' for "on" and '0' for "off
			// Payload offset 2 and 3 are always '0', seem unused

			printf(" Set swing %d spare bytes", swing);
			hexdump_raw(&buf[S21_PAYLOAD_OFFSET + 1], S21_PAYLOAD_LEN - 1);
			break;
		 case '6':
		    powerful = buf[S21_PAYLOAD_OFFSET + 0] == '2'; // '2' or '0'
			// My Daichi controller always sends 'D6 0 0 0 0' for 'Eco',
			// both on and off. Bug or feature ?

			printf(" Set powerful %d spare bytes", powerful);
			hexdump_raw(&buf[S21_PAYLOAD_OFFSET + 1], S21_PAYLOAD_LEN - 1);
			break;
		 default:
            printf(" Set unknown:");
		    hexdump_raw(buf, len);
		    break;
		 }

	     buf[0] = 0;
		 continue;
	  }

      if (buf[1] == 'F') {
		 // Query control settings
		 switch (buf[2]) {
	     case '1':
		    if (debug)
		       printf(" -> power %d mode %d temp %.1f\n", power, mode, temp);
		    response[3] = power + '0'; // sent as ASCII
			response[4] = mode + '0';
			// 18.0 + 0.5 * (signed) (payload[2] - '@')
			response[5] = s21_encode_target_temp(temp);
			response[6] = s21_encode_fan(fan);

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '4':
		    if (debug)
		       printf(" -> unknown ('F4')\n");
		    response[3] = 0x30; // No idea what this is, taken from my FTXF20D
			response[4] = 0x00;
			response[5] = 0xA0;
			response[6] = 0x30;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '5':
		    if (debug)
		       printf(" -> swing %d\n", swing);
		    response[3] = swing;
			response[4] = 0;
			response[5] = 0;
			response[6] = 0;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '6':
		    if (debug)
		       printf(" -> powerful %d\n", powerful);
		    response[3] = powerful ? '2' : '0';
			response[4] = 0;
			response[5] = 0;
			response[6] = 0;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '7':
		    if (debug)
		       printf(" -> eco %d\n", eco);
		    response[3] = 0;
			response[4] = eco ? '2' : '0';
			response[5] = 0;
			response[6] = 0;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
 /*
  * I also tried the following commands on my FTXF20D and got
  * responses as listed. It is currently unknown what they report.
  * 'F2' 06 02 47 32 34 3A 00 80 67 03
  * 'F3' 06 02 47 33 30 30 30 00 0A 03
  * 'F8' 06 02 47 38 30 32 30 30 41 03
  * 'F9' 06 02 47 39 B4 FF FF 30 62 03
  * 'RI' 06 02 53 49 35 36 32 2B 64 03 - the same data as for 'RH', probably firmware bug
  */
		 default:
		    // Respond NAK to an unknown command. My FTXF20D does the same.
		    s21_nak(p, buf);
		    continue;
		 }
	  } else if (buf[1] == 'R') {
		 // Query temperature sensors
		 switch (buf[2]) {
	     case 'H':
		    send_temp(p, response, buf, home);
		    break;
	     case 'I':
		    send_temp(p, response, buf, inlet);
		    break;
	     case 'a':
		    send_temp(p, response, buf, outside);
		    break;
	     case 'L':
		    // No idea what this is, comments in Faikin code say it's fan speed
			// This sample value was grabbed from my FTXF20D; when turned off,
			// it reports '000'. Let's try. It should most likely read 052, because
			// after some time it reported '350', should have been 053. This also
			// follows logic of reporting sensor temperatures in inverse order
			// (see send_temp())
		    response[S21_PAYLOAD_OFFSET + 0] = '2';
			response[S21_PAYLOAD_OFFSET + 1] = '5';
			response[S21_PAYLOAD_OFFSET + 2] = '0';
			
			s21_reply(p, response, buf, 3); // Nontypical response, 3 bytes, not 4!
		    break;
		 default:
		    s21_nak(p, buf);
		    continue;
		 }
	  } else {
		  s21_nak(p, buf);
		  continue;
	  }

      // We are here if we just have sent a reply. The controller must ACK it.

	  do {
         len = read(p, buf, 1);

         if (len < 0) {
		    perror("Error reading from serial port");
		    exit(255);
	     }
	  } while (len != 1);

      hexdump("Rx", buf, 1);

	  if (debug && buf[0] != ACK) {
		 printf("Protocol error: expected ACK, got 0x%02X\n", buf[0]);
	  }
	  // My Daichi cloud controller doesn't send this ACK.
	  // After a small delay it simply sends a next packet
	  if (buf[0] == STX) {
		 if (debug)
		    printf("The controller didn't ACK our response, next frame started!\n");
	  } else {
		 buf[0] = 0;
	  }
   }
   return 0;
}
