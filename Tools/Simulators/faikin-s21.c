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
#include "faikin-s21.h"
#include "osal.h"

static int debug = 0; // Dump commands and responses (short form)
static int dump  = 0; // Raw dump

// Initial state of a simulated A/C. Defaults are chosen to be distinct;
// can be changed via command line.
static struct S21State init_state = {
   .power    = 0,    // Power on
   .mode     = 3,    // Mode
   .temp     = 22.5, // Set point
   .fan      = 3,    // Fan speed
   .swing    = 0,    // Swing direction
   .powerful = 0,    // Powerful mode
   .eco      = 0,    // Eco mode
   .home     = 245,  // Reported temparatures (multiplied by 10 here)
   .outside  = 205,
   .inlet    = 185,
   .fanrpm   = 52,   // Fan RPM (divided by 10 here)
   .comprpm  = 42,   // Compressor RPM
   .power    = 2,	 // Power consumption in 100 Wh units
   .protocol = 2,    // Protocol version
   .model    = {'1', '3', '5', 'D'},     // Reported A/C model code. Default taken from FTXF20D5V1B
   // Taken from FTXF20D
   .FB       = {0x30, 0x33, 0x36, 0x30}, // 0630
   .FG       = {0x30, 0x34, 0x30, 0x30}, // 0040
   .FK       = {0x71, 0x73, 0x35, 0x31}, // 15sq
   .FN       = {0x30, 0x30, 0x30, 0x30}, // 0000
   .FP       = {0x37, 0x33, 0x30, 0x30}, // 0037
   .FQ       = {0x45, 0x33, 0x30, 0x30}, // 003E
   .FR       = {0x30, 0x30, 0x30, 0x30}, // 0000
   .FS       = {0x30, 0x30, 0x30, 0x30}, // 0000
   .FT       = {0x31, 0x30, 0x30, 0x30}  // 0001
};

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

   printf(" -> Unknown command %c%c, sending NAK\n", buf[S21_CMD0_OFFSET], buf[S21_CMD1_OFFSET]);
   serial_write(p, &response, 1);
   
   buf[0] = 0; // Clear read buffer
}

static void s21_ack(int p)
{
   static unsigned char response = ACK;

   serial_write(p, &response, 1);
}

static void s21_nonstd_reply(int p, unsigned char *response, int body_len)
{
   int pkt_len = S21_FRAMING_LEN + body_len;
   int l;

   s21_ack(p); // Send ACK before the reply

   // Make a proper framing
   response[S21_STX_OFFSET] = STX;
   response[S21_CMD0_OFFSET + body_len] = s21_checksum(response, pkt_len);
   response[S21_CMD0_OFFSET + body_len + 1] = ETX;

   serial_write(p, response, pkt_len);
}

static void s21_reply(int p, unsigned char *response, const unsigned char *cmd, int payload_len)
{
	response[S21_CMD0_OFFSET] = cmd[S21_CMD0_OFFSET] + 1;
    response[S21_CMD1_OFFSET] = cmd[S21_CMD1_OFFSET];

	s21_nonstd_reply(p, response, 2 + payload_len); // Body is two cmd bytes plus payload
}

// A wrapper for unknown command. Useful because we're adding them in bulk
static void unknown_cmd(int p, unsigned char *response, const unsigned char *cmd,
                        unsigned char r0, unsigned char r1, unsigned char r2, unsigned char r3)
{
   if (debug)
      printf(" -> unknown ('%c%c') = 0x%02X 0x%02X 0x%02X 0x%02X\n",
	         cmd[S21_CMD0_OFFSET], cmd[S21_CMD1_OFFSET], r0, r1, r2, r3);
   response[3] = r0;
   response[4] = r1;
   response[5] = r2;
   response[6] = r3;

   s21_reply(p, response, cmd, S21_PAYLOAD_LEN);
}

static void unknown_cmd_a(int p, unsigned char *response, const unsigned char *cmd, const unsigned char *r)
{
   unknown_cmd(p, response, cmd, r[0], r[1], r[2], r[3]);
}

static void send_temp(int p, unsigned char *response, const unsigned char *cmd, int value, const char *name)
{
	char buf[5];
	
	snprintf(buf, sizeof(buf), "%+d", value);
	if (debug)
	   printf(" -> %s = %s\n", name, buf);

    // A decimal value from sensor is sent as ASCII value with sign,
	// spelled backwards for some reason. One decimal place is assumed.
	response[S21_PAYLOAD_OFFSET + 0] = buf[3];
	response[S21_PAYLOAD_OFFSET + 1] = buf[2];
	response[S21_PAYLOAD_OFFSET + 2] = buf[1];
	response[S21_PAYLOAD_OFFSET + 3] = buf[0];
	
	s21_reply(p, response, cmd, S21_PAYLOAD_LEN);
}

static void send_int(int p, unsigned char *response, const unsigned char *cmd, unsigned int value, const char *name)
{
	char buf[4];

	snprintf(buf, sizeof(buf), "%03u", value);
	if (debug)
	   	printf(" -> %s = %s\n", name, buf);

	// Order inverted, the same as in send_temp()
    response[S21_PAYLOAD_OFFSET + 0] = buf[2];
	response[S21_PAYLOAD_OFFSET + 1] = buf[1];
	response[S21_PAYLOAD_OFFSET + 2] = buf[0];
			
	s21_reply(p, response, cmd, 3); // Nontypical response, 3 bytes, not 4!
}

static void send_hex(int p, unsigned char *response, const unsigned char *cmd, unsigned int value, const char *name)
{
	char buf[5];

	snprintf(buf, sizeof(buf), "%04X", value);
	if (debug)
	   	printf(" -> %s = %s\n", name, buf);

	// Order inverted, the same as in send_temp()
    response[S21_PAYLOAD_OFFSET + 0] = buf[3];
	response[S21_PAYLOAD_OFFSET + 1] = buf[2];
	response[S21_PAYLOAD_OFFSET + 2] = buf[1];
	response[S21_PAYLOAD_OFFSET + 2] = buf[0];
			
	s21_reply(p, response, cmd, S21_PAYLOAD_LEN);
}

int
main(int argc, const char *argv[])
{
   const char  *port = NULL;
   char        *model = NULL;
   poptContext optCon;
   const struct poptOption optionsTable[] = {
	  {"port", 'p', POPT_ARG_STRING, &port, 0, "Port", "/dev/cu.usbserial..."},
	  {"debug", 'v', POPT_ARG_NONE, &debug, 0, "Debug"},
	  {"dump", 'V', POPT_ARG_NONE, &dump, 0, "Dump"},
	  {"on", 0, POPT_ARG_NONE, &init_state.power, 0, "Power on"},
	  {"mode", 0, POPT_ARG_INT, &init_state.mode, 0, "Mode", "0=F,1=H,2=C,3=A,7=D"},
	  {"fan", 0, POPT_ARG_INT, &init_state.fan, 0, "Fan", "0 = auto, 1-5 = set speed, 6 = quiet"},
	  {"temp", 0, POPT_ARG_FLOAT, &init_state.temp, 0, "Temp", "C"},
	  {"comprpm", 0, POPT_ARG_INT, &init_state.fanrpm, 0, "Fan rpm (divided by 10)"},
	  {"comprpm", 0, POPT_ARG_INT, &init_state.comprpm, 0, "Compressor rpm"},
	  {"powerful", 0, POPT_ARG_NONE, &init_state.powerful, 0, "Debug"},
	  {"protocol", 0, POPT_ARG_INT, &init_state.protocol, 0, "Reported protocol version"},
	  {"consumption", 0, POPT_ARG_INT, &init_state.consumption, 0, "Reported power consumption"},
	  {"model", 0, POPT_ARG_STRING, &model, 0, "Reported model code"},
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

   if (model) {
	  if (strlen(model) < sizeof(init_state.model)) {
	  	fprintf(stderr, "Invalid --model code given, %d characters required", sizeof(init_state.model));
	  	return -1;
	  }
	  memcpy(init_state.model, model, sizeof(init_state.model));
	  free(model);
   }

   // Create shared memory and initialize it with contents of init_state
   struct S21State *state = create_shmem(SHARED_MEM_NAME, &init_state, sizeof(init_state));

   if (!state) {
	  fprintf(stderr, "Failed to create shared memory");
	  exit(255);
   }

   int p = open(port, O_RDWR);

   if (p < 0) {
      fprintf(stderr, "Cannot open %s: %s", port, strerror(errno));
	  exit(255);
   }

   set_serial(p, 2400, CS8, EVENPARITY, TWOSTOPBITS);

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
		    state->power = buf[S21_PAYLOAD_OFFSET + 0] - '0'; // ASCII char
			state->mode  = buf[S21_PAYLOAD_OFFSET + 1] - '0'; // See AC_MODE_*
			state->temp  = s21_decode_target_temp(buf[S21_PAYLOAD_OFFSET + 2]);
			state->fan   = s21_decode_fan(buf[S21_PAYLOAD_OFFSET + 3]);

			printf(" Set power %d mode %d temp %.1f fan %d\n", state->power, state->mode, state->temp, state->fan);
			break;
		 case '5':
		    state->swing = buf[S21_PAYLOAD_OFFSET + 0] - '0'; // ASCII char
			// Payload offset 1 equals to '?' for "on" and '0' for "off
			// Payload offset 2 and 3 are always '0', seem unused

			printf(" Set swing %d spare bytes", state->swing);
			hexdump_raw(&buf[S21_PAYLOAD_OFFSET + 1], S21_PAYLOAD_LEN - 1);
			break;
		 case '6':
		    state->powerful = buf[S21_PAYLOAD_OFFSET + 0] == '2'; // '2' or '0'
			// My Daichi controller always sends 'D6 0 0 0 0' for 'Eco',
			// both on and off. Bug or feature ?

			printf(" Set powerful %d spare bytes", state->powerful);
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
		       printf(" -> power %d mode %d temp %.1f\n", state->power, state->mode, state->temp);
		    response[3] = state->power + '0'; // sent as ASCII
			response[4] = state->mode + '0';
			// 18.0 + 0.5 * (signed) (payload[2] - '@')
			response[5] = s21_encode_target_temp(state->temp);
			response[6] = s21_encode_fan(state->fan);

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '2':
		    // BRP069B41 sends this as first command. If NAK is received, it keeps retrying
			// and doesn't send anything else. Suggestion - query AC features
			// The response values here are kindly provided by a user in reverse engineering
			// thread: https://github.com/revk/ESP32-Faikin/issues/408#issuecomment-2278296452
			// Correspond to A/C models CTXM60RVMA, CTXM35RVMA
			// It was experimentally found that with different values, given by FTXF20D, the
			// controller falls into error 252 and refuses to accept A/C commands over HTTP.
			// FTXF20D: 34 3A 00 80
			unknown_cmd(p, response, buf, 0x3D, 0x3B, 0x00, 0x80);
			break;
		 case '3':
		    if (debug)
		       printf(" -> powerful ('F3') %d\n", state->powerful);
			response[3] = 0x30; // No idea what this is, taken from my FTXF20D
			response[4] = 0xFE;
			response[5] = 0xFE;
			response[6] = state->powerful ? 2 : 0;

		    s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '4':
		    // Also taken from CTXM60RVMA, CTXM35RVMA, and also error 252 if wrong
			// FTXF20D: 30 00 A0 30
			unknown_cmd(p, response, buf, 0x30, 0x00, 0x80, 0x30);
			break;
		 case '5':
		    if (debug)
		       printf(" -> swing %d\n", state->swing);
		    response[3] = '0' + state->swing;
			response[4] = 0x3F;
			response[5] = 0x30;
			response[6] = 0x80;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '6':
		    if (debug)
		       printf(" -> powerful ('F6') %d\n", state->powerful);
		    response[3] = state->powerful ? '2' : '0';
			response[4] = 0x30;
			response[5] = 0x30;
			response[6] = 0x30;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '7':
		    if (debug)
		       printf(" -> eco %d\n", state->eco);
		    response[3] = 0x30;
			response[4] = state->eco ? '2' : '0';
			response[5] = 0x30;
			response[6] = 0x30;

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		case '8':
		    if (debug)
		       printf(" -> Protocol version = %d\n", state->protocol);
			// 'F8' - this is found out to be protocol version.
			// My FTXF20D replies with '0020' (assuming reading in reverse like everything else).
			// If we say that, BRP069B41 then asks for F9 (we know it's different form of home/outside sensor)
			// then proceeds requiring more commands, majority of english alphabet. I got tired implementing
			// all of them and tried to downgrade the response to '0000'. This caused the controller sending
			// 'MM' command (see below), and then it goes online with our emulated A/C.
			// '0010' gives the same results
		    response[3] = '0';
			response[4] = '0' + state->protocol;
			response[5] = '0';
			response[6] = '0';

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case '9':
			// In debug log temperature values will appear multiplied by 2
		    response[3] = state->home / 5 + 0x80;
			response[4] = state->outside / 5 + 0x80; // This is from Faikin sources, but FTXF20D returnx 0xFF here
			response[5] = 0xFF; // Copied from FTFX20D
			response[6] = 0x30; // Copied from FTFX20D

		    if (debug)
		       printf(" -> home = 0x%02X (%.1f) outside = 0x%02X (%.1f)\n",
			          response[3], state->home / 10.0, response[4], state->outside / 10.0);

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case 'C':
		    // Protocol v2 - model code. Reported as "model=" in aircon/get_model_info.
			// One of few commands, which is only sent by controller once after bootup.
			// Even if communication is broken, then recovered (sim restarted), it won't
			// be sent again. Controller reboot would be required to accept the new value.
		 	if (debug)
		       printf(" -> model = %.4s\n", state->model);

		    response[3] = state->model[3];
			response[4] = state->model[2];
			response[5] = state->model[1];
			response[6] = state->model[0];

			s21_reply(p, response, buf, S21_PAYLOAD_LEN);
			break;
		 case 'M':
		 	// Protocol v2 - power consumption in 100 Wh units
		 	send_hex(p, response, buf, state->power, "Power comsumption");
			break;
		 // All unknown_cmd's below are queried by BRP069B41 for protocol version 2.
		 // They are all mandatory; if we respond NAK, the controller keeps retrying
		 // this command and doesn't proceed.
		 // All response values are taken from FTXF20D
		 case 'B':
			unknown_cmd_a(p, response, buf, state->FB);
			break;
		 case 'G':
			unknown_cmd_a(p, response, buf, state->FG);
			break;
		 case 'K':
		    // Optional features. Displayed in /aircon/get_model_info:
			// byte 0:
			// - bit 2: acled=<bool>. LED control presence ?
			// - bit 3: land=<bool>
			// byte 1:
			// - bit 0: elec=<bool>
			// - bit 2: temp_rng=<bool>
			// - bit 3: m_dtct 0=<bool>
			// byte 2:
			// - bit 0: Not understood
			//   0 -> ac_dst=jp
			//   1 -> ac_dst=--
			// - bit 1: Not understood. Something with humidity ?
			//   0 -> s_humd=0
			//   1 -> s_humd=16
			// - bit 2: Enable fan controls ???
			//    0 -> en_frate=0 en_fdir=0 s_fdir=0
			//    1 -> en_frate=1 en_fdir=1 s_fdir=3
			//    When set to 0, the "Online controller" app always shows "fan off",
			//    and attempts to control it do nothing.
			// - bit 3: disp_dry=<bool>
			// byte 3 - doesn't change anything
			// FTXF20D values: 0x71, 0x73, 0x35, 0x31
			unknown_cmd_a(p, response, buf, state->FK);
			break;
		 case 'N':
			unknown_cmd_a(p, response, buf, state->FN);
			break;
		 case 'P':
			unknown_cmd_a(p, response, buf, state->FP);
			break;
		 case 'Q':
			unknown_cmd_a(p, response, buf, state->FQ);
			break;
		 case 'R':
			unknown_cmd_a(p, response, buf, state->FR);
			break;
		 case 'S':
			unknown_cmd_a(p, response, buf, state->FS);
			break;
		 case 'T':
			unknown_cmd_a(p, response, buf, state->FT);
			break;
		 case 'V':
		 	// This one is not sent by BRP069B41, but i quickly got tired of adding these
			// one by one and simply ran all the alphabet up to FZZ on my FTXF20D, so here it is.
			unknown_cmd(p, response, buf, 0x33, 0x37, 0x83, 0x30);
			break;
		 // BRP069B41 also sends 'FY' command, but accepts NAK and stops doing so.
		 // Therefore the command is optional. My FTXF20D also doesn't recognize it.
		 default:
		    // Respond NAK to an unknown command. My FTXF20D does the same.
		    s21_nak(p, buf);
		    continue;
		 }
	  } else if (buf[S21_CMD0_OFFSET] == 'M') {
		if (debug)
		    printf(" -> unknown ('MM')\n");
		// This is sent by BRP069B41 and response is mandatory. The controller
		// loops forever if NAK is received.
		// I experimentally found out that this command doesn't have a second
		// byte, and the A/C always responds with this. Note non-standard
		// response form.
		response[S21_CMD0_OFFSET] = 'M';
		response[2] = 'F';
		response[3] = 'F';
		response[4] = 'F';
		response[5] = 'F';

		s21_nonstd_reply(p, response, 5);
	  } else if (buf[S21_CMD0_OFFSET] == 'R') {
		 // Query sensors
		 switch (buf[S21_CMD1_OFFSET]) {
	     case 'H':
		    send_temp(p, response, buf, state->home, "home");
		    break;
	     case 'I':
		    send_temp(p, response, buf, state->inlet, "inlet");
		    break;
	     case 'a':
		    send_temp(p, response, buf, state->outside, "outside");
		    break;
	     case 'L':
		 	send_int(p, response, buf, state->fanrpm, "fanrpm");
		    break;
		 case 'd':
		 	send_int(p, response, buf, state->comprpm, "compressor rpm");
			break;
	     case 'N':
		 	// These two are queried by BRP069B41, at least for protocol version 1, but we have no idea
			// what they mean. Not found anywhere in controller's http responses. We're replying with
			// some distinct values for possible identification in case if they pop up somewhere.
			// The following is what my FTX20D returns, also with known commands from above, for comparison:
			// {"protocol":"S21","dump":"0253483035322B5D03","SH":"052+"} - home
			// {"protocol":"S21","dump":"0253493535322B6303","SI":"552+"} - inlet
			// {"protocol":"S21","dump":"0253613035312B7503","Sa":"051+"} - outside
			// {"protocol":"S21","dump":"02534E3532312B6403","SN":"521+"} - ???
			// {"protocol":"S21","dump":"0253583033322B6B03","SX":"032+"} - ???
		    send_temp(p, response, buf, 235, "unknown ('RN')");
		    break;
	     case 'X':
		    send_temp(p, response, buf, 215, "unknown ('RX')");
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
