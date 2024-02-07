/* Faikin app */
/* Copyright ©2022 Adrian Kennard, Andrews & Arnold Ltd. See LICENCE file for details .GPL 3.0 */

static const char TAG[] = "Faikin";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <driver/rmt_tx.h>
#include <driver/rmt_rx.h>
#include "esp_http_server.h"
#include <math.h>
#include "mdns.h"
#include "bleenv.h"
#include "daikin_s21.h"

#ifndef	CONFIG_HTTPD_WS_SUPPORT
#error Need CONFIG_HTTPD_WS_SUPPORT
#endif

// Macros for setting values
// They set new values for parameters inside the big "daikin" state struct
// and also set appropriate flags, so that changes are picked up by the main
// loop and commands are sent to the aircon to apply the settings
#define	daikin_set_v(name,value)	daikin_set_value(#name,&daikin.name,CONTROL_##name,value)
#define	daikin_set_i(name,value)	daikin_set_int(#name,&daikin.name,CONTROL_##name,value)
#define	daikin_set_e(name,value)	daikin_set_enum(#name,&daikin.name,CONTROL_##name,value,CONTROL_##name##_VALUES)
#define	daikin_set_t(name,value)	daikin_set_temp(#name,&daikin.name,CONTROL_##name,value)

// Settings (RevK library used by MQTT setting command)

enum
{                               // Number the control fields
#define	b(name)		CONTROL_##name##_pos,
#define	t(name)		b(name)
#define	r(name)		b(name)
#define	i(name)		b(name)
#define	e(name,values)	b(name)
#define	s(name,len)	b(name)
#include "acextras.m"
};
#define	b(name)		const uint64_t CONTROL_##name=(1ULL<<CONTROL_##name##_pos);
#define	t(name)		b(name)
#define	r(name)		b(name)
#define	i(name)		b(name)
#define	e(name,values)	b(name) const char CONTROL_##name##_VALUES[]=#values;
#define	s(name,len)	b(name)
#include "acextras.m"

enum
{
   PROTO_TYPE_S21,
   PROTO_TYPE_X50A,
   PROTO_TYPE_CN_WIRED,
   PROTO_TYPE_MAX = PROTO_TYPE_CN_WIRED,        // Fudge, don't scan CN_WIRED for now
};
const char *const prototype[] = { "S21", "X50A", "CN_WIRED" };

const char *const fans[] = {    // mapping A12345Q
   "auto",
   "low",
   "lowMedium",
   "medium",
   "mediumHigh",
   "high",
   "night",
};

#define	PROTO_TXINVERT	1
#define	PROTO_RXINVERT	2
#define	PROTO_SCALE	4

// Globals
struct
{
   uint8_t loopback:1;
   uint8_t dumping:1;
} b = { 0 };

static httpd_handle_t webserver = NULL;
static uint8_t protocol_set = 0;        // protocol confirmed
static uint8_t proto = 0;

static uint8_t
proto_type (void)
{
   return proto / PROTO_SCALE;
}

static const char *
proto_name (void)
{
   return prototype[proto_type ()];
}

// 'fanstep' setting overrides number of available fan speeds
// 1 = force 5 speeds; 2 = force 3 speeds; 0 = default
// For experiments only !
static int
have_5_fan_speeds (void)
{
   return fanstep == 1 || (!fanstep && proto_type () == PROTO_TYPE_S21);
}

#define	CN_WIRED_LEN	8
#define	CN_WIRED_SYNC	2600    // uS
#define	CN_WIRED_START	1000    // uS
#define	CN_WIRED_SPACE	300     // uS
#define	CN_WIRED_0	400     // uS
#define	CN_WIRED_1	1000    // uS
#define	CN_WIRED_IDLE	16000   // uS
#define	CN_WIRED_TERM	2000    // uS
#define	CN_WIRED_MARGIN	200     // uS
rmt_channel_handle_t rmt_tx = NULL,
   rmt_rx = NULL;
rmt_encoder_handle_t rmt_encoder = NULL;
rmt_symbol_word_t rmt_rx_raw[128];
volatile size_t rmt_rx_len = 0; // Rx is ready
const rmt_receive_config_t rmt_rx_config = {
   .signal_range_min_ns = 1000, // shortest - to eliminate glitches
   .signal_range_max_ns = 5000000,      // longest - needs to be over the 2600uS sync pulse...
};

const rmt_transmit_config_t rmt_tx_config = {
   .flags.eot_level = 1,
};

#ifdef ELA
static bleenv_t *bletemp = NULL;
#endif

// The current aircon state and stats
struct
{
   SemaphoreHandle_t mutex;     // Control changes
   uint64_t control_changed;    // Which control fields are being set
   uint64_t status_known;       // Which fields we know, and hence can control
   uint8_t control_count;       // How many times we have tried to change control and not worked yet
   uint32_t statscount;         // Count for b() i(), etc.
#define	b(name)		uint8_t	name;uint32_t total##name;
#define	t(name)		float name;float min##name;float total##name;float max##name;uint32_t count##name;
#define	r(name)		float min##name;float max##name;
#define	i(name)		int name;int min##name;int total##name;int max##name;
#define	e(name,values)	uint8_t name;
#define	s(name,len)	char name[len];
#include "acextras.m"
   float envlast;               // Predictive, last period value
   float envdelta;              // Predictive, diff to last
   float envdelta2;             // Predictive, previous diff
   uint32_t controlvalid;       // uptime to which auto mode is valid
   uint32_t sample;             // Last uptime sampled
   uint32_t counta,
     counta2;                   // Count of "approaching temp", and previous sample
   uint32_t countb,
     countb2;                   // Count of "beyond temp", and previous sample
   uint32_t countt,
     countt2;                   // Count total, and previous sample
   uint8_t fansaved;            // Saved fan we override at start
   uint8_t talking:1;           // We are getting answers
   uint8_t lastheat:1;          // Last heat mode
   uint8_t status_changed:1;    // Status has changed
   uint8_t mode_changed:1;      // Status or control has changed for enum or bool
   uint8_t status_report:1;     // Send status report
   uint8_t ha_send:1;           // Send HA config
   uint8_t remote:1;            // Remote control via MQTT
   uint8_t hysteresis:1;        // Thermostat hysteresis state
   uint8_t cnresend:2;          // Resends
} daikin = { 0 };

const char *
daikin_set_value (const char *name, uint8_t * ptr, uint64_t flag, uint8_t value)
{                               // Setting a value (uint8_t)
   if (*ptr == value)
      return NULL;              // No change
   if (!(daikin.status_known & flag))
      return "Setting cannot be controlled";
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   *ptr = value;
   daikin.control_changed |= flag;
   daikin.mode_changed = 1;
   xSemaphoreGive (daikin.mutex);
   return NULL;
}

const char *
daikin_set_int (const char *name, int *ptr, uint64_t flag, int value)
{                               // Setting a value (uint8_t)
   if (*ptr == value)
      return NULL;              // No change
   if (!(daikin.status_known & flag))
      return "Setting cannot be controlled";
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   daikin.control_changed |= flag;
   daikin.mode_changed = 1;
   *ptr = value;
   xSemaphoreGive (daikin.mutex);
   return NULL;
}

const char *
daikin_set_enum (const char *name, uint8_t * ptr, uint64_t flag, char *value, const char *values)
{                               // Setting a value (uint8_t) based on string which is expected to be single character from values set
   if (!value || !*value)
      return "No value";
   if (value[1])
      return "Value is meant to be one character";
   char *found = strchr (values, *value);
   if (!found)
      return "Value is not a valid value";
   daikin_set_value (name, ptr, flag, (int) (found - values));
   return NULL;
}

const char *
daikin_set_temp (const char *name, float *ptr, uint64_t flag, float value)
{                               // Setting a value (float)
   if (*ptr == value)
      return NULL;              // No change
   if (proto_type () == PROTO_TYPE_CN_WIRED)
      value = roundf (value);   // CN_WIRED only does 1C steps
   else if (proto_type () == PROTO_TYPE_S21)
      value = roundf (value * 2.0) / 2.0;       // S21 only does 0.5C steps
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   *ptr = value;
   daikin.control_changed |= flag;
   daikin.mode_changed = 1;
   xSemaphoreGive (daikin.mutex);
   return NULL;
}

void
set_uint8 (const char *name, uint8_t * ptr, uint64_t flag, uint8_t val)
{                               // Updating status
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   if (!(daikin.status_known & flag))
   {
      daikin.status_known |= flag;
      daikin.status_changed = 1;
   }
   if (*ptr == val)
   {                            // No change
      if (daikin.control_changed & flag)
      {
         daikin.control_changed &= ~flag;
         daikin.status_changed = 1;
      }
   } else if (!(daikin.control_changed & flag))
   {                            // Changed (and not something we are trying to set)
      *ptr = val;
      daikin.status_changed = 1;
      daikin.mode_changed = 1;
   }
   xSemaphoreGive (daikin.mutex);
}

void
set_int (const char *name, int *ptr, uint64_t flag, int val)
{                               // Updating status
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   if (!(daikin.status_known & flag))
   {
      daikin.status_known |= flag;
      daikin.status_changed = 1;
   }
   if (*ptr == val)
   {                            // No change
      if (daikin.control_changed & flag)
      {
         daikin.control_changed &= ~flag;
         daikin.status_changed = 1;
      }
   } else if (!(daikin.control_changed & flag))
   {                            // Changed (and not something we are trying to set)
      if (*ptr / 10 != val / 10)
         daikin.status_changed = 1;
      *ptr = val;
   }
   xSemaphoreGive (daikin.mutex);
}

void
set_float (const char *name, float *ptr, uint64_t flag, float val)
{                               // Updating status
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   if (!(daikin.status_known & flag))
   {
      daikin.status_known |= flag;
      daikin.status_changed = 1;
   }
   if (lroundf (*ptr * 10) == lroundf (val * 10))
   {                            // No change (allow within 0.1C)
      if (daikin.control_changed & flag)
      {
         daikin.control_changed &= ~flag;
         daikin.status_changed = 1;
      }
   } else if (!(daikin.control_changed & flag))
   {                            // Changed (and not something we are trying to set)
      *ptr = val;
      daikin.status_changed = 1;
      if (flag == CONTROL_temp)
         daikin.mode_changed = 1;
   }
   xSemaphoreGive (daikin.mutex);
}

#define set_val(name,val) set_uint8(#name,&daikin.name,CONTROL_##name,val)
#define set_int(name,val) set_int(#name,&daikin.name,CONTROL_##name,val)
#define set_temp(name,val) set_float(#name,&daikin.name,CONTROL_##name,val)

jo_t
jo_comms_alloc (void)
{
   jo_t j = jo_object_alloc ();
   jo_stringf (j, "protocol", "%s%s%s", b.loopback ? "loopback" : proto_name (),
               (proto & PROTO_TXINVERT) ? "¬Tx" : "", (proto & PROTO_RXINVERT) ? "¬Rx" : "");
   return j;
}

jo_t s21debug = NULL;

enum
{
   S21_OK,
   S21_NAK,
   S21_NOACK,
   S21_BAD,
   S21_WAIT,
};

static int
check_length (uint8_t cmd, uint8_t cmd2, int len, int required, const uint8_t * payload)
{
   if (len >= required)
      return 1;

   jo_t j = jo_comms_alloc ();
   jo_stringf (j, "badlength", "%d", len);
   jo_stringf (j, "expected", "%d", required);
   jo_stringf (j, "command", "%c%c", cmd, cmd2);
   jo_base16 (j, "data", payload, len);
   revk_error ("comms", &j);

   return 0;
}

// Decode S21 response payload
int
daikin_s21_response (uint8_t cmd, uint8_t cmd2, int len, uint8_t * payload)
{
   if (len > 1 && s21debug)
   {
      char tag[3] = { cmd, cmd2 };
      jo_stringn (s21debug, tag, (char *) payload, len);
   }
   // Remember to add to polling if we add more handlers
   if (cmd == 'G')
      switch (cmd2)
      {
      case '1':                // 'G1' - basic status
         if (check_length (cmd, cmd2, len, S21_PAYLOAD_LEN, payload))
         {
            set_val (online, 1);
            set_val (power, (payload[0] == '1') ? 1 : 0);
            set_val (mode, "30721003"[payload[1] & 0x7] - '0'); // FHCA456D mapped from AXDCHXF
            set_val (heat, daikin.mode == 1);   // Crude - TODO find if anything actually tells us this
            if (daikin.mode == 1 || daikin.mode == 2 || daikin.mode == 3)
               set_temp (temp, s21_decode_target_temp (payload[2]));
            else if (!isnan (daikin.temp))
               set_temp (temp, daikin.temp);    // Does not have temp in other modes
            if (payload[3] != 'A')      // Set fan speed
               set_val (fan, "00012345"[payload[3] & 0x7] - '0');       // XXX12345 mapped to A12345Q
            else if (daikin.fan == 6)
               set_val (fan, 6);        // Quiet mode set (it returns as auto, so we assume it set to quiet if not powered on)
            else if (!daikin.power || !daikin.fan || daikin.fanrpm >= 750)
               set_val (fan, 0);        // Auto as fan too fast to be quiet mode
         }
         break;
      case '3':                // Seems to be an alternative to G6
         if (check_length (cmd, cmd2, len, 1, payload))
         {
            set_val (powerful, payload[3] & 0x02 ? 1 : 0);
         }
         break;
      case '5':                // 'G5' - swing status
         if (check_length (cmd, cmd2, len, 1, payload))
         {
            if (!noswingw)
               set_val (swingv, (payload[0] & 1) ? 1 : 0);
            if (!noswingh)
               set_val (swingh, (payload[0] & 2) ? 1 : 0);
         }
         break;
      case '6':                // 'G6' - "powerful" mode and some others
         if (check_length (cmd, cmd2, len, S21_PAYLOAD_LEN, payload))
         {
            if (!nopowerful)
               set_val (powerful, payload[0] & 0x02 ? 1 : 0);
            if (!nocomfort)
               set_val (comfort, payload[0] & 0x40 ? 1 : 0);
            if (!noquiet)
               set_val (quiet, payload[0] & 0x80 ? 1 : 0);
            if (!nostreamer)
               set_val (streamer, payload[1] & 0x80 ? 1 : 0);
            if (!nosensor)
               set_val (sensor, payload[3] & 0x08 ? 1 : 0);
            if (!noled)
               set_val (led, (payload[3] & 0x0C) != 0x0C);
         }
         break;
      case '7':                // 'G7' - "demand" and "eco" mode
         if (check_length (cmd, cmd2, len, 2, payload))
         {
            if (!nodemand && payload[0] != '1')
               set_int (demand, 100 - (payload[0] - '0'));
            set_val (econo, payload[1] & 0x02 ? 1 : 0);
         }
         break;
      case '9':
         if (check_length (cmd, cmd2, len, 2, payload))
         {
            set_temp (home, (float) ((signed) payload[0] - 0x80) / 2);
            set_temp (outside, (float) ((signed) payload[1] - 0x80) / 2);
         }
         break;
      }
   if (cmd == 'S')
   {
      if (cmd2 == 'L' || cmd2 == 'd' || cmd2 == 'D')
      {                         // These responses are always only 3 bytes long
         if (check_length (cmd, cmd2, len, 3, payload))
         {
            int v = s21_decode_int_sensor (payload);
            switch (cmd2)
            {
            case 'L':          // Fan
               set_int (fanrpm, v * 10);
               break;
            case 'd':          // Compressor
               set_int (comp, v);
               break;
            }
         }
      } else if (check_length (cmd, cmd2, len, S21_PAYLOAD_LEN, payload))
      {
         float t = s21_decode_float_sensor (payload);

         if (t < 100)           // Sanity check
         {
            switch (cmd2)
            {                   // Temperatures (guess)
            case 'H':          // 'SH' - home temp
               set_temp (home, t);
               break;
            case 'a':          // 'Sa' - outside temp
               set_temp (outside, t);
               break;
            case 'I':          // 'SI' - liquid ???
               set_temp (liquid, t);
               break;
            case 'N':          // ?
               break;
            case 'X':          // ?
               break;
            }
         }
      }
   }
   return S21_OK;
}


bool
rmt_rx_callback (rmt_channel_handle_t channel, const rmt_rx_done_event_data_t * edata, void *user_data)
{
   if (edata->num_symbols < 64)
   {                            // Silly... restart rx
      rmt_rx_len = 0;
      rmt_receive (rmt_rx, rmt_rx_raw, sizeof (rmt_rx_raw), &rmt_rx_config);
      return pdFALSE;
   }
   // Got something
   rmt_rx_len = edata->num_symbols;
   return pdFALSE;
}

void
daikin_cn_wired_response (int len, uint8_t * payload)
{                               // Process response
   if (len != CN_WIRED_LEN)
      return;
   daikin.control_changed = 0;  // Assume all handled
   if (payload[7] & 1)
   {                            // Mode change
      set_temp (temp, (payload[0] >> 4) * 10 + (payload[0] & 0xF));
      set_val (mode, "7020100030000000"[payload[3] & 15] - '0');        // Map DFCXHXXXAXXXXXXX to FHCA456D
      set_val (power, (payload[3] & 0x10) ? 0 : 1);
      set_val (fan, "0040200016000000"[payload[4] & 15] - '0'); // Map XA4P2XXX1QXXXXXX to A12345Q
      set_val (powerful, (payload[4] == 3) ? 1 : 0);
      set_val (swingv, (payload[5] & 0x20) ? 1 : 0);
   } else
   {                            // Temp
      set_temp (home, (payload[0] >> 4) * 10 + (payload[0] & 0xF));
      daikin.status_known |= (CONTROL_temp | CONTROL_mode | CONTROL_power | CONTROL_fan | CONTROL_powerful | CONTROL_swingv);   // Bodge as we may not see a type 1 for some time|
   }
}

void
daikin_x50a_response (uint8_t cmd, int len, uint8_t * payload)
{                               // Process response
   if (debug && len)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%02X", cmd);
      jo_base16 (j, "payload", payload, len);
      revk_info ("rx", &j);
   }
   if (cmd == 0xAA && len >= 1)
   {                            // Initialisation response
      if (!*payload)
         daikin.talking = 0;    // Not ready
      return;
   }
   if (cmd == 0xBA && len >= 20)
   {
      strncpy (daikin.model, (char *) payload, sizeof (daikin.model));
      daikin.status_changed = 1;
      return;
   }
   if (cmd == 0xCA && len >= 7)
   {                            // Main status settings
      set_val (online, 1);
      set_val (power, payload[0]);
      set_val (mode, payload[1]);
      set_val (heat, payload[2] == 1);
      set_val (slave, payload[9]);
      set_val (fan, (payload[6] >> 4) & 7);
      return;
   }
   if (cmd == 0xCB && len >= 2)
   {                            // We get all this from CA
      return;
   }
   if (cmd == 0xBD && len >= 29)
   {                            // Looks like temperatures - we assume 0000 is not set
      float t;
      if ((t = (int16_t) (payload[0] + (payload[1] << 8)) / 128.0) && t < 100)
         set_temp (inlet, t);
      if ((t = (int16_t) (payload[2] + (payload[3] << 8)) / 128.0) && t < 100)
         set_temp (home, t);
      if ((t = (int16_t) (payload[4] + (payload[5] << 8)) / 128.0) && t < 100)
         set_temp (liquid, t);
      if ((t = (int16_t) (payload[8] + (payload[9] << 8)) / 128.0) && t < 100)
         set_temp (temp, t);
#if 0
      if (debug)
      {
         jo_t j = jo_object_alloc ();   // Debug dump
         jo_litf (j, "a", "%.2f", (int16_t) (payload[0] + (payload[1] << 8)) / 128.0);  // inlet
         jo_litf (j, "b", "%.2f", (int16_t) (payload[2] + (payload[3] << 8)) / 128.0);  // Room
         jo_litf (j, "c", "%.2f", (int16_t) (payload[4] + (payload[5] << 8)) / 128.0);  // liquid
         jo_litf (j, "d", "%.2f", (int16_t) (payload[6] + (payload[7] << 8)) / 128.0);  // same as inlet?
         jo_litf (j, "e", "%.2f", (int16_t) (payload[8] + (payload[9] << 8)) / 128.0);  // Target
         jo_litf (j, "f", "%.2f", (int16_t) (payload[10] + (payload[11] << 8)) / 128.0);        // same as inlet
         revk_info ("temps", &j);
      }
#endif
      return;
   }
   if (cmd == 0xBE && len >= 9)
   {                            // Status/flags?
      set_int (fanrpm, (payload[2] + (payload[3] << 8)));
      // Flag4 ?
      set_val (flap, payload[5]);
      set_val (antifreeze, payload[6]);
      // Flag7 ?
      // Flag8 ?
      // Flag9 ?
      // 0001B0040100000001
      // 010476050101000001
      // 010000000100000001
#if 0
      jo_t j = jo_comms_alloc ();       // Debug
      jo_base16 (j, "be", payload, len);
      revk_info ("rx", &j);
#endif
      return;
   }
}

void
protocol_found (void)
{
   protocol_set = 1;
   if (proto != protocol)
   {
      jo_t j = jo_object_alloc ();
      jo_int (j, "protocol", proto);
      revk_setting (j);
      jo_free (&j);
   }
}

// Timeout value for serial port read
#define READ_TIMEOUT (500 / portTICK_PERIOD_MS)

int
daikin_s21_command (uint8_t cmd, uint8_t cmd2, int txlen, char *payload)
{
   if (debug && txlen > 2 && !b.dumping)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%c%c", cmd, cmd2);
      if (txlen)
      {
         jo_base16 (j, "payload", payload, txlen);
         jo_stringn (j, "text", (char *) payload, txlen);
      }
      revk_info (daikin.talking || protofix ? "tx" : "cannot-tx", &j);
   }
   if (!daikin.talking && !protofix)
      return S21_WAIT;          // Failed
   uint8_t buf[256],
     temp;
   if (!snoop)
   {                            // Send
      buf[0] = STX;
      buf[1] = cmd;
      buf[2] = cmd2;
      if (txlen)
         memcpy (buf + 3, payload, txlen);
      buf[3 + txlen] = s21_checksum (buf, S21_MIN_PKT_LEN + txlen);
      buf[4 + txlen] = ETX;
      if (b.dumping)
      {
         jo_t j = jo_comms_alloc ();
         jo_base16 (j, "dump", buf, txlen + S21_MIN_PKT_LEN);
         char c[3] = { cmd, cmd2 };
         jo_stringn (j, c, payload, txlen);
         revk_info ("tx", &j);
      }
      uart_write_bytes (uart, buf, S21_MIN_PKT_LEN + txlen);
   }
   // Wait ACK
   int rxlen = uart_read_bytes (uart, &temp, 1, READ_TIMEOUT);
   if (rxlen != 1 || (temp != ACK && temp != STX))
   {
      // Got something else
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%c%c", cmd, cmd2);
      if (txlen)
      {
         jo_base16 (j, "payload", payload, txlen);
         jo_stringn (j, "text", (char *) payload, txlen);
      }
      if (rxlen == 1 && temp == NAK)
      {
         // Got an explicit NAK
         if (debug)
         {
            jo_bool (j, "nak", 1);
            revk_error ("comms", &j);
         } else
            jo_free (&j);
         return S21_NAK;
      }
      // Unexpected reply, protocol broken
      daikin.talking = 0;
      jo_bool (j, "noack", 1);
      if (rxlen)
         jo_stringf (j, "value", "%02X", temp);
      revk_error ("comms", &j);
      return S21_NOACK;
   }
   if (temp == STX)
      *buf = temp;
   else
   {
      if (cmd == 'D')
         return S21_OK;         // No response expected
      while (1)
      {
         rxlen = uart_read_bytes (uart, buf, 1, READ_TIMEOUT);
         if (rxlen != 1)
         {
            daikin.talking = 0;
            b.loopback = 0;
            jo_t j = jo_comms_alloc ();
            jo_bool (j, "timeout", 1);
            revk_error ("comms", &j);
            return S21_NOACK;
         }
         if (*buf == STX)
            break;
      }
   }
   // Receive the rest of response till ETX
   while (rxlen < sizeof (buf))
   {
      if (uart_read_bytes (uart, buf + rxlen, 1, READ_TIMEOUT) != 1)
      {
         daikin.talking = 0;
         b.loopback = 0;
         jo_t j = jo_comms_alloc ();
         jo_bool (j, "timeout", 1);
         jo_base16 (j, "data", buf, rxlen);
         revk_error ("comms", &j);
         return S21_NOACK;
      }
      rxlen++;
      if (buf[rxlen - 1] == ETX)
         break;
   }
   ESP_LOG_BUFFER_HEX (TAG, buf, rxlen);        // TODO 
   // Send ACK regardless of packet quality. If we don't ack due to checksum error,
   // for example, the response will be sent again.
   // Note not all ACs do that. My FTXF20D doesn't - Sonic-Amiga
   temp = ACK;
   uart_write_bytes (uart, &temp, 1);
   if (b.dumping || snoop)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "dump", buf, rxlen);
      char c[3] = { buf[1], buf[2] };
      jo_stringn (j, c, (char *) buf + 3, rxlen - 5);
      revk_info ("rx", &j);
   }
   int s21_bad (jo_t j)
   {                            // Report error and return S21_BAD - also pause/flush
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      if (!protocol_set)
      {
         sleep (1);
         uart_flush (uart);
      }
      return S21_BAD;
   }
   // Check checksum
   uint8_t c = s21_checksum (buf, rxlen);
   if (c != buf[rxlen - 2])
   {                            // Sees checksum of 03 actually sends as 05
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "badsum", "%02X", c);
      return s21_bad (j);
   }
   if (!snoop && rxlen >= 5 && buf[0] == STX && buf[rxlen - 1] == ETX && buf[1] == cmd)
   {                            // Loop back
      daikin.talking = 0;
      if (!b.loopback)
      {
         ESP_LOGE (TAG, "Loopback");
         b.loopback = 1;
         revk_blink (0, 0, "RGB");
      }
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "loopback", 1);
      revk_error ("comms", &j);
      return S21_OK;
   }
   b.loopback = 0;
   // If we've got an STX, S21 protocol is now confirmed; we won't change it any more
   if (buf[0] == STX && !protocol_set)
      protocol_found ();
   // An expected S21 reply contains the first character of the command
   // incremented by 1, the second character is left intact
   if (!snoop && (rxlen < S21_MIN_PKT_LEN || buf[S21_STX_OFFSET] != STX || buf[rxlen - 1] != ETX || buf[S21_CMD0_OFFSET] != cmd + 1
                  || buf[S21_CMD1_OFFSET] != cmd2))
   {
      // Malformed response, no proper S21
      daikin.talking = 0;       // Protocol is broken, will restart communication
      jo_t j = jo_comms_alloc ();
      if (buf[0] != STX)
         jo_bool (j, "badhead", 1);
      if (buf[1] != cmd + 1 || buf[2] != cmd2)
         jo_bool (j, "mismatch", 1);
      return s21_bad (j);
   }
   return daikin_s21_response (buf[S21_CMD0_OFFSET], buf[S21_CMD1_OFFSET], rxlen - S21_MIN_PKT_LEN, buf + S21_PAYLOAD_OFFSET);
}

void
daikin_cn_wired_command (int len, uint8_t * buf)
{                               // Waits rx and sends command/response to it
   if (!rmt_tx || !rmt_encoder || !rmt_rx)
   {
      daikin.talking = 0;       // Not ready?
      return;
   }

   {                            // Wait rx
      int wait = 5000;
      while (!rmt_rx_len && --wait)
         usleep (1000);
   }

   if (rmt_rx_len)
   {                            // Process receive
      uint32_t sum0 = 0,
         sum1 = 0,
         sums = 0,
         cnt0 = 0,
         cnt1 = 0,
         cnts = 0,
         sync = 0,
         start = 0;
      uint8_t rx[CN_WIRED_LEN] = { 0 };
      const char *e = NULL;
      int p = 0,
         dur = 0;
      // Sanity checking
      if (!e && rmt_rx_len != sizeof (rx) * 8 + 2)
         e = "Wrong length";
      if (!e && rmt_rx_raw[p].level0)
         e = "Bad start polarity";
      if (!e && ((dur = rmt_rx_raw[p].duration0) < CN_WIRED_SYNC - CN_WIRED_MARGIN || dur > CN_WIRED_SYNC + CN_WIRED_MARGIN))
         e = "Bad start duration";
      sync = rmt_rx_raw[p].duration0;
      if (!e && ((dur = rmt_rx_raw[p].duration1) < CN_WIRED_START - CN_WIRED_MARGIN || dur > CN_WIRED_START + CN_WIRED_MARGIN))
         e = "Bad start bit";
      start = rmt_rx_raw[p].duration1;
      p++;
      for (int i = 0; !e && i < sizeof (rx); i++)
         for (uint8_t b = 0x01; !e && b; b <<= 1)
         {
            if (!e
                && ((dur = rmt_rx_raw[p].duration0) < CN_WIRED_SPACE - CN_WIRED_MARGIN || dur > CN_WIRED_SPACE + CN_WIRED_MARGIN))
               e = "Bad space duration";
            sums += rmt_rx_raw[p].duration0;
            cnts++;
            if (!e && (dur = rmt_rx_raw[p].duration1) > CN_WIRED_1 - CN_WIRED_MARGIN && dur < CN_WIRED_1 + CN_WIRED_MARGIN)
            {
               rx[i] |= b;
               sum1 += rmt_rx_raw[p].duration1;
               cnt1++;
            } else if (!e && ((dur = rmt_rx_raw[p].duration1) < CN_WIRED_0 - CN_WIRED_MARGIN || dur > CN_WIRED_1 + CN_WIRED_MARGIN))
               e = "Bad bit duration";
            else
            {
               sum0 += rmt_rx_raw[p].duration1;
               cnt0++;
            }
            p++;
         }
      if (!e)
      {
         dur = 0;               // Not a duration error
         uint8_t sum = (rx[sizeof (rx) - 1] & 0x0F);
         for (int i = 0; i < sizeof (rx) - 1; i++)
            sum += (rx[i] >> 4) + rx[i];
         if ((rx[sizeof (rx) - 1] >> 4) != (sum & 0xF))
            e = "Bad checksum";
      }
      if (e)
      {
         jo_t j = jo_comms_alloc ();
         jo_string (j, "error", e);
         jo_int (j, "ts", esp_timer_get_time ());
         if (dur)
            jo_int (j, "duration", dur);
         if (p > 1)
            jo_base16 (j, "data", rx, (p - 1) / 8);
         jo_int (j, "sync", sync);
         jo_int (j, "start", start);
         if (cnts)
            jo_int (j, "space", sums / cnts);
         if (cnt0)
            jo_int (j, "0", sum0 / cnt0);
         if (cnt1)
            jo_int (j, "1", sum1 / cnt1);
         revk_error ("comms", &j);
      } else
      {                         // Got a message, yay!
         if (!protocol_set)
            protocol_found ();
         if (b.dumping)
         {
            jo_t j = jo_comms_alloc ();
            jo_int (j, "ts", esp_timer_get_time ());
            jo_base16 (j, "dump", rx, sizeof (rx));
            jo_int (j, "sync", sync);
            jo_int (j, "start", start);
            if (cnts)
               jo_int (j, "space", sums / cnts);
            if (cnt0)
               jo_int (j, "0", sum0 / cnt0);
            if (cnt1)
               jo_int (j, "1", sum1 / cnt1);
            revk_info ("rx", &j);
         }
         daikin_cn_wired_response (sizeof (rx), rx);
         buf[1] = rx[1];        // TODO try that
      }
   }
   // Next Rx
   rmt_rx_len = 0;
   REVK_ERR_CHECK (rmt_receive (rmt_rx, rmt_rx_raw, sizeof (rmt_rx_raw), &rmt_rx_config));

   if (daikin.status_changed || !(daikin.status_known & CONTROL_power) || daikin.cnresend)
   {                            // Send response
      if (daikin.status_changed)
         daikin.cnresend = 3;
      else if (daikin.cnresend)
         daikin.cnresend--;
      // Checksum (LOL)
      uint8_t sum = (buf[len - 1] & 0x0F);
      for (int i = 0; i < len - 1; i++)
         sum += (buf[i] >> 4) + buf[i];
      buf[len - 1] = (sum << 4) + (buf[len - 1] & 0x0F);
      if (b.dumping)
      {
         jo_t j = jo_comms_alloc ();
         jo_int (j, "ts", esp_timer_get_time ());
         jo_base16 (j, "dump", buf, len);
         revk_info ("tx", &j);
      }
      // Encode manually, yes, silly, but bytes encoder has no easy way to add the start bits.
      rmt_symbol_word_t seq[3 + len * 8 + 1];
      int p = 0;
      seq[p].duration0 = CN_WIRED_SYNC - 1000;  // 2500us low - do in two parts? so we start with high for data
      seq[p].level0 = 0;
      seq[p].duration1 = 1000;
      seq[p++].level1 = 0;
      void add (int d)
      {
         seq[p].duration0 = d;
         seq[p].level0 = 1;
         seq[p].duration1 = CN_WIRED_SPACE;
         seq[p++].level1 = 0;
      }
      add (CN_WIRED_START);
      for (int i = 0; i < len; i++)
         for (uint8_t b = 0x01; b; b <<= 1)
            add ((buf[i] & b) ? CN_WIRED_1 : CN_WIRED_0);
      seq[p].duration0 = CN_WIRED_IDLE;
      seq[p].level0 = 1;
      seq[p].duration1 = CN_WIRED_TERM;
      seq[p++].level1 = 0;

      REVK_ERR_CHECK (rmt_transmit (rmt_tx, rmt_encoder, seq, p * sizeof (rmt_symbol_word_t), &rmt_tx_config));
      REVK_ERR_CHECK (rmt_tx_wait_all_done (rmt_tx, 1000));
   }
}

void
daikin_x50a_command (uint8_t cmd, int txlen, uint8_t * payload)
{                               // Send a command and get response
   if (debug && txlen)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%02X", cmd);
      jo_base16 (j, "payload", payload, txlen);
      revk_info (daikin.talking || protofix ? "tx" : "cannot-tx", &j);
   }
   if (!daikin.talking && !protofix)
      return;                   // Failed
   uint8_t buf[256];
   buf[0] = 0x06;
   buf[1] = cmd;
   buf[2] = txlen + 6;
   buf[3] = 1;
   buf[4] = 0;
   if (txlen)
      memcpy (buf + 5, payload, txlen);
   uint8_t c = 0;
   for (int i = 0; i < 5 + txlen; i++)
      c += buf[i];
   buf[5 + txlen] = 0xFF - c;
   if (b.dumping)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "dump", buf, txlen + 6);
      revk_info ("tx", &j);
   }
   uart_write_bytes (uart, buf, 6 + txlen);
   // Wait for reply
   int rxlen = uart_read_bytes (uart, buf, sizeof (buf), READ_TIMEOUT);
   if (rxlen <= 0)
   {
      daikin.talking = 0;
      b.loopback = 0;
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "timeout", 1);
      revk_error ("comms", &j);
      return;
   }
   if (b.dumping)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "dump", buf, rxlen);
      revk_info ("rx", &j);
   }
   // Check checksum
   c = 0;
   for (int i = 0; i < rxlen; i++)
      c += buf[i];
   if (c != 0xFF)
   {
      daikin.talking = 0;
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "badsum", "%02X", c);
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      return;
   }
   // Process response
   if (rxlen < 6 || buf[0] != 0x06 || buf[1] != cmd || buf[2] != rxlen || buf[3] != 1)
   {                            // Basic checks
      daikin.talking = 0;
      jo_t j = jo_comms_alloc ();
      if (buf[0] != 0x06)
         jo_bool (j, "badhead", 1);
      if (buf[1] != cmd)
         jo_bool (j, "mismatch", 1);
      if (buf[2] != rxlen || rxlen < 6)
         jo_bool (j, "badrxlen", 1);
      if (buf[3] != 1)
         jo_bool (j, "badform", 1);
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      return;
   }
   if (!buf[4])
   {                            // Tx sends 00 here, rx is 06
      daikin.talking = 0;
      if (!b.loopback)
      {
         ESP_LOGE (TAG, "Loopback");
         b.loopback = 1;
         revk_blink (0, 0, "RGB");
      }
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "loopback", 1);
      revk_error ("comms", &j);
      return;
   }
   b.loopback = 0;
   if (buf[0] == 0x06 && !protocol_set && (buf[1] != 0xFF || (proto & PROTO_TXINVERT)))
      protocol_found ();
   if (buf[1] == 0xFF)
   {                            // Error report
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "fault", 1);
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      return;
   }
   daikin_x50a_response (cmd, rxlen - 6, buf + 5);
}

// Parse control JSON, arrived by MQTT, and apply values
const char *
daikin_control (jo_t j)
{                               // Control settings as JSON
   jo_type_t t = jo_next (j);   // Start object
   jo_t s = NULL;
   while (t == JO_TAG)
   {
      const char *err = NULL;
      char tag[20] = "",
         val[20] = "";
      jo_strncpy (j, tag, sizeof (tag));
      t = jo_next (j);
      jo_strncpy (j, val, sizeof (val));
#define	b(name)		if(!strcmp(tag,#name)&&(t==JO_TRUE||t==JO_FALSE))err=daikin_set_v(name,t==JO_TRUE?1:0);
#define	t(name)		if(!strcmp(tag,#name)&&t==JO_NUMBER)err=daikin_set_t(name,strtof(val,NULL));
#define	i(name)		if(!strcmp(tag,#name)&&t==JO_NUMBER)err=daikin_set_i(name,atoi(val));
#define	e(name,values)	if(!strcmp(tag,#name)&&t==JO_STRING)err=daikin_set_e(name,val);
#include "accontrols.m"
      if (!strcmp (tag, "auto0") || !strcmp (tag, "auto1"))
      {                         // Stored settings
         if (strlen (val) >= 5)
         {
            if (!s)
               s = jo_object_alloc ();
            jo_int (s, tag, atoi (val) * 100 + atoi (val + 3));
         }
      }
      if (!strcmp (tag, "autop"))
      {
         if (!s)
            s = jo_object_alloc ();
         jo_bool (s, tag, *val == 't');
      }
      if (!strcmp (tag, "autot") || !strcmp (tag, "autor"))
      {                         // Stored settings
         if (!s)
            s = jo_object_alloc ();
         jo_lit (s, tag, val);
         daikin.status_changed = 1;
      }
      if (!strcmp (tag, "autob"))
      {                         // Stored settings
         if (!s)
            s = jo_object_alloc ();
         jo_string (s, tag, val);       // Set BLE value
      }
      if (err)
      {                         // Error report
         jo_t j = jo_object_alloc ();
         jo_string (j, "field", tag);
         jo_string (j, "error", err);
         revk_error ("control", &j);
         return err;
      }
      t = jo_skip (j);
   }
   if (s)
   {
      revk_setting (s);
      jo_free (&s);
   }
   return "";
}

// --------------------------------------------------------------------------------
char debugsend[10] = "";
// Called by an MQTT client inside the revk library
const char *
mqtt_client_callback (int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{                               // MQTT app callback
   const char *ret = NULL;
   if (client || !prefix || target || strcmp (prefix, prefixcommand))
      return NULL;              // Not for us or not a command from main MQTT
   if (!suffix)
      return daikin_control (j);        // General setting
   if (!strcmp (suffix, "reconnect"))
   {
      daikin.talking = 0;       // Disconnect and reconnect
      return "";
   }
   if (!strcmp (suffix, "connect") || !strcmp (suffix, "status"))
   {
      daikin.status_report = 1; // Report status on connect
      if (ha)
         daikin.ha_send = 1;
   }
   if (!strcmp (suffix, "send") && jo_here (j) == JO_STRING)
   {
      jo_strncpy (j, debugsend, sizeof (debugsend));
      return "";
   }
   if (!strcmp (suffix, "control"))
   {                            // Control, e.g. from environmental monitor
      float env = NAN;
      float min = NAN;
      float max = NAN;
      jo_type_t t = jo_next (j);        // Start object
      while (t == JO_TAG)
      {
         char tag[20] = "",
            val[20] = "";
         jo_strncpy (j, tag, sizeof (tag));
         t = jo_next (j);
         jo_strncpy (j, val, sizeof (val));
         if (!strcmp (tag, "env"))
            env = strtof (val, NULL);
         else if (!strcmp (tag, "target"))
         {
            if (jo_here (j) == JO_ARRAY)
            {
               jo_next (j);
               if (jo_here (j) == JO_NUMBER)
               {
                  jo_strncpy (j, val, sizeof (val));
                  min = strtof (val, NULL);
                  jo_next (j);
               }
               if (jo_here (j) == JO_NUMBER)
               {
                  jo_strncpy (j, val, sizeof (val));
                  max = strtof (val, NULL);
                  jo_next (j);
               }
               while (jo_here (j) > JO_CLOSE)
                  jo_next (j);  // Should not be more
               t = jo_next (j); // Pass the close
               continue;        // As we passed the close, don't skip}
            } else
               min = max = strtof (val, NULL);
         }
#define	b(name)		else if(!strcmp(tag,#name)){if(t!=JO_TRUE&&t!=JO_FALSE)ret= "Expecting boolean";else ret=daikin_set_v(name,t==JO_TRUE?1:0);}
#define	t(name)		else if(!strcmp(tag,#name)){if(t!=JO_NUMBER)ret= "Expecting number";else ret=daikin_set_t(name,jo_read_float(j));}
#define	i(name)		else if(!strcmp(tag,#name)){if(t!=JO_NUMBER)ret= "Expecting number";else ret=daikin_set_i(name,jo_read_int(j));}
#define	e(name,values)	else if(!strcmp(tag,#name)){if(t!=JO_STRING)ret= "Expecting string";else ret=daikin_set_e(name,val);}
#include "accontrols.m"
         t = jo_skip (j);
      }

      xSemaphoreTake (daikin.mutex, portMAX_DELAY);
      daikin.controlvalid = uptime () + tcontrol;
      if (!autor)
      {
         daikin.mintarget = min;
         daikin.maxtarget = max;
      }
      if (!ble || !*autob)
      {
         daikin.env = env;
         daikin.status_known |= CONTROL_env;    // So we report it
      }
      if (!autor && !*autob)
         daikin.remote = 1;     // Hides local automation settings
      xSemaphoreGive (daikin.mutex);
      return ret ? : "";
   }

   jo_t s = jo_object_alloc ();
   // Crude commands - setting one thing
   if (!j)
   {
      if (!strcmp (suffix, "on"))
         jo_bool (s, "power", 1);
      if (!strcmp (suffix, "off"))
         jo_bool (s, "power", 0);
      if (!strcmp (suffix, "auto"))
         jo_string (s, "mode", "A");
      if (!strcmp (suffix, "heat"))
         jo_string (s, "mode", "H");
      if (!strcmp (suffix, "cool"))
         jo_string (s, "mode", "C");
      if (!strcmp (suffix, "dry"))
         jo_string (s, "mode", "D");
      if (!strcmp (suffix, "fan"))
         jo_string (s, "mode", "F");
      if (!strcmp (suffix, "low"))
         jo_string (s, "fan", "1");
      if (!strcmp (suffix, "medium"))
         jo_string (s, "fan", "3");
      if (!strcmp (suffix, "high"))
         jo_string (s, "fan", "5");
   } else
   {
      char value[20] = "";
      jo_strncpy (j, value, sizeof (value));
      if (!strcmp (suffix, "temp"))
      {
         if (autor)
         {                      // Setting the control
            jo_t s = jo_object_alloc ();
            jo_lit (s, "autot", value);
            revk_setting (s);
            jo_free (&s);
         } else
            jo_lit (s, "temp", value);  // Direct controls
      }
      // HA stuff
      if (!strcmp (suffix, "mode"))
      {
         jo_bool (s, "power", *value == 'o' ? 0 : 1);
         if (*value != 'o')
            jo_stringf (s, "mode", "%c", toupper (*value));
      }
      if (!strcmp (suffix, "fan"))
      {
         int f;
         for (f = 0; f < sizeof (fans) / sizeof (*fans) && strcmp (fans[f], value); f++);
         if (f < sizeof (fans) / sizeof (*fans))
            jo_stringf (s, "fan", "%c", CONTROL_fan_VALUES[f]);
         else
            jo_stringf (s, "fan", "%c", *value);
      }
      if (!strcmp (suffix, "swing"))
      {
         if (*value == 'C')
            jo_bool (s, "comfort", 1);
         else
         {
            jo_bool (s, "swingh", strchr (value, 'H') ? 1 : 0);
            jo_bool (s, "swingv", strchr (value, 'V') ? 1 : 0);
         }
      }
      if (!strcmp (suffix, "preset"))
      {
         jo_bool (s, "econo", *value == 'e');
         jo_bool (s, "powerful", *value == 'b');
      }
      if (!strcmp (suffix, "demand"))
         jo_int (s, "demand", atoi (value));
      // TODO comfort/streamer/sensor/quiet
   }

   jo_close (s);
   jo_rewind (s);
   if (jo_next (s) == JO_TAG)
   {
      jo_rewind (s);
      ret = daikin_control (s);
   }
   jo_free (&s);
   return ret;
}

jo_t
daikin_status (void)
{
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   jo_t j = jo_comms_alloc ();
#define b(name)         if(daikin.status_known&CONTROL_##name)jo_bool(j,#name,daikin.name);
#define t(name)         if(daikin.status_known&CONTROL_##name){if(isnan(daikin.name)||daikin.name>=100)jo_null(j,#name);else jo_litf(j,#name,"%.1f",daikin.name);}
#define i(name)         if(daikin.status_known&CONTROL_##name)jo_int(j,#name,daikin.name);
#define e(name,values)  if((daikin.status_known&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
#define s(name,len)     if((daikin.status_known&CONTROL_##name)&&*daikin.name)jo_string(j,#name,daikin.name);
#include "acextras.m"
#ifdef	ELA
   if (bletemp && !bletemp->missing)
   {
      jo_object (j, "ble");
      if (bletemp->tempset)
         jo_litf (j, "temp", "%.2f", bletemp->temp / 100.0);
      if (bletemp->humset)
         jo_litf (j, "hum", "%.2f", bletemp->hum / 100.0);
      if (bletemp->batset)
         jo_int (j, "bat", bletemp->temp);
      if (bletemp->voltset)
         jo_litf (j, "volt", "%.2f", bletemp->volt / 100.0);
      jo_close (j);
   }
   if (ble && *autob)
      jo_string (j, "autob", autob);
#endif
   if (daikin.remote)
      jo_bool (j, "remote", 1);
   else
   {
      jo_litf (j, "autor", "%.1f", (float) autor / autor_scale);
      jo_litf (j, "autot", "%.1f", (float) autot / autot_scale);
      jo_stringf (j, "auto0", "%02d:%02d", auto0 / 100, auto0 % 100);
      jo_stringf (j, "auto1", "%02d:%02d", auto1 / 100, auto1 % 100);
      jo_bool (j, "autop", autop);
   }
   xSemaphoreGive (daikin.mutex);
   return j;
}

// --------------------------------------------------------------------------------
// Web
static void
web_head (httpd_req_t * req, const char *title)
{
   revk_web_head (req, title);
   revk_web_send (req, "<style>"        //
                  "body{font-family:sans-serif;background:#8cf;}"       //
                  ".on{opacity:1;transition:1s;}"       // 
                  ".off{opacity:0;transition:1s;}"      // 
                  "select{min-height:34px;border-radius:34px;background-color:#ccc;border:1px solid gray;color:black;box-shadow:3px 3px 3px #0008;}"    //
                  "input.temp{min-width:300px;}"        //
                  "input.time{min-height:34px;min-width:64px;border-radius:34px;background-color:#ccc;border:1px solid gray;color:black;box-shadow:3px 3px 3px #0008;}" //
                  "</style><body><h1>%s</h1>", title ? : "");
}

static esp_err_t
web_icon (httpd_req_t * req)
{                               // serve image -  maybe make more generic file serve
   extern const char start[] asm ("_binary_apple_touch_icon_png_start");
   extern const char end[] asm ("_binary_apple_touch_icon_png_end");
   httpd_resp_set_type (req, "image/png");
   httpd_resp_send (req, start, end - start);
   return ESP_OK;
}

static esp_err_t
web_root (httpd_req_t * req)
{
   if ((!webcontrol || revk_link_down ()) && websettings)
      return revk_web_settings (req);   // Direct to web set up
   web_head (req, hostname == revk_id ? appname : hostname);
   revk_web_send (req, "<div id=top class=off><form name=F><table id=live>");
   void addh (const char *tag)
   {                            // Head (well, start of row)
      revk_web_send (req, "<tr><td align=right>%s</td>", tag);
   }
   void addf (const char *tag)
   {                            // Foot (well, end of row)
      revk_web_send (req, "<td colspan=2 id='%s'></td></tr>", tag);
   }
   void add (const char *tag, const char *field, ...)
   {
      addh (tag);
      va_list ap;
      va_start (ap, field);
      int n = 0;
      while (1)
      {
         const char *tag = va_arg (ap, char *);
         if (!tag)
            break;
         if (n == 5)
         {
            revk_web_send (req, "</tr><tr><td></td>");
            n = 0;
         }
         n++;
         const char *value = va_arg (ap, char *);
         revk_web_send (req,
                        "<td><label class=box><input type=radio name='%s' value='%s' id='%s%s' onchange=\"if(this.checked)w('%s','%s');\"><span class=button>%s</span></label></td>",
                        field, value, field, value, field, value, tag);
      }
      va_end (ap);
      addf (tag);
   }
   void addb (const char *tag, const char *field, const char *help)
   {
      revk_web_send (req,
                     "<td align=right>%s</td><td title=\"%s\"><label class=switch><input type=checkbox id=\"%s\" onchange=\"w('%s',this.checked);\"><span class=slider></span></label></td>",
                     tag, help, field, field);
   }
   void addslider (const char *tag, const char *field, int min, int max, const char *step)
   {
      addh (tag);
      revk_web_send (req,
                     "<td colspan=6><input type=range class=temp min=%d max=%d step=%s id=%s onchange=\"w('%s',+this.value);\"><span id=T%s></span></td>",
                     min, max, step, field, field, field);
      addf (tag);
   }
   revk_web_send (req, "<tr>");
   addb ("0/1", "power", "Main power");
   revk_web_send (req, "</tr>");
   add ("Mode", "mode", "Auto", "A", "Heat", "H", "Cool", "C", "Dry", "D", "Fan", "F", NULL);
   if (have_5_fan_speeds ())
      add ("Fan", "fan", "1", "1", "2", "2", "3", "3", "4", "4", "5", "5", "Auto", "A", "Night", "Q", NULL);
   else if (proto_type () == PROTO_TYPE_CN_WIRED)
      add ("Fan", "fan", "Low", "1", "Mid", "3", "High", "5", "Auto", "A", NULL);
   else
      add ("Fan", "fan", "Low", "1", "Mid", "3", "High", "5", NULL);
   addslider ("Set", "temp", tmin, tmax,
              proto_type () == PROTO_TYPE_CN_WIRED ? "1" : proto_type () == PROTO_TYPE_S21 ? "0.5" : "0.1");
   void addt (const char *tag, const char *help)
   {
      revk_web_send (req, "<td title=\"%s\" align=right>%s<br><span id=\"%s\"></span></td>", help, tag, tag);
   }
   revk_web_send (req, "<tr><td>Temps</td>");
   if (daikin.status_known & CONTROL_inlet)
      addt ("Inlet", "Inlet temperature");
   if (daikin.status_known & CONTROL_home)
      addt ("Home", "Inlet temperature");
   if (daikin.status_known & CONTROL_liquid)
      addt ("Liquid", "Liquid coolant temperature");
   if (daikin.status_known & CONTROL_outside)
      addt ("Outside", "Outside temperature");
   if ((daikin.status_known & CONTROL_env) && (!ble || !*autob))
      addt ("Env", "External reference temperature");
   if (ble)
   {
      addt ("BLE", "External BLE temperature");
      addt ("Hum", "External BLE humidity");
   }
   revk_web_send (req, "</tr>");
   if (daikin.status_known & CONTROL_demand)
      addslider ("Demand", "demand", 30, 100, "5");
   if (daikin.status_known & (CONTROL_econo | CONTROL_powerful | CONTROL_led))
   {
      revk_web_send (req, "<tr>");
      if (daikin.status_known & CONTROL_econo)
         addb ("Eco", "econo", "Eco mode");
      if (daikin.status_known & CONTROL_powerful)
         addb ("💪", "powerful", "Powerful mode");
      if (daikin.status_known & CONTROL_led)
         addb ("💡", "led", "LED high");
      revk_web_send (req, "</tr>");
   }
   if (daikin.status_known & (CONTROL_swingv | CONTROL_swingh | CONTROL_comfort))
   {
      revk_web_send (req, "<tr>");
      if (daikin.status_known & CONTROL_swingv)
         addb ("↕", "swingv", "Vertical Swing");
      if (daikin.status_known & CONTROL_swingh)
         addb ("↔", "swingh", "Horizontal Swing");
      if (daikin.status_known & CONTROL_comfort)
         addb ("🧸", "comfort", "Comfort mode");
      revk_web_send (req, "</tr>");
   }
   if (daikin.status_known & (CONTROL_streamer | CONTROL_sensor | CONTROL_quiet))
   {
      revk_web_send (req, "<tr>");
      if (daikin.status_known & CONTROL_streamer)
         addb ("🦠", "streamer", "Stream/filter enable");
      if (daikin.status_known & CONTROL_sensor)
         addb ("🙆", "sensor", "Sensor mode");
      if (daikin.status_known & CONTROL_quiet)
         addb ("🤫", "quiet", "Quiet outdoor unit");
      revk_web_send (req, "</tr>");
   }
   revk_web_send (req, "</table>"       //
                  "<p id=offline style='display:none'><b>System is off line.</b></p>"   //
                  "<p id=loopback style='display:none'><b>System is in loopback test.</b></p>"  //
                  "<p id=shutdown style='display:none;color:red;'></p>" //
                  "<p id=slave style='display:none'>❋ Another unit is controlling the mode, so this unit is not operating at present.</p>"    //
                  "<p id=control style='display:none'>✷ Automatic control means some functions are limited.</p>"      //
                  "<p id=antifreeze style='display:none'>❄ System is in anti-freeze now, so cooling is suspended.</p>");
#ifdef ELA
   if (autor || (ble && *autob) || (!nofaikinauto && !daikin.remote))
   {
      void addnote (const char *note)
      {
         revk_web_send (req, "<tr><td colspan=6>%s</td></tr>", note);
      }

      void addtime (const char *tag, const char *field)
      {
         revk_web_send (req,
                        "<td align=right>%s</td><td><input class=time type=time title=\"Set 00:00 to disable\" id='%s' onchange=\"w('%s',this.value);\"></td>",
                        tag, field, field);
      }
      revk_web_send (req,
                     "<div id=remote><hr><p>Faikin-auto mode (sets hot/cold and temp high/low to aim for the following target), and timed and auto power on/off.</p><table>");
      add ("Enable", "autor", "Off", "0", fahrenheit ? "±0.9℉" : "±½℃", "0.5", fahrenheit ? "±1.8℉" : "±1℃", "1",
           fahrenheit ? "±3.6℉" : "±2℃", "2", NULL);
      addslider ("Target", "autot", tmin, tmax,
                 proto_type () == PROTO_TYPE_CN_WIRED ? "1" : proto_type () == PROTO_TYPE_S21 ? "0.5" : "0.1");
      addnote ("Timed on and off (set other than 00:00)<br>Automated on/off if temp is way off target.");
      revk_web_send (req, "<tr>");
      addtime ("On", "auto1");
      addtime ("Off", "auto0");
      addb ("Auto", "autop", "Automatic power on/off");
      revk_web_send (req, "</tr>");
      if (ble)
      {
         addnote ("External temperature reference for Faikin-auto mode");
         revk_web_send (req, "<tr><td>BLE</td><td colspan=6>"   //
                        "<select name=autob onchange=\"w('autob',this.options[this.selectedIndex].value);\">");
         if (!*autob)
            revk_web_send (req, "<option value=\"\">-- None --");
         char found = 0;
         for (bleenv_t * e = bleenv; e; e = e->next)
         {
            revk_web_send (req, "<option value=\"%s\"", e->name);
            if (*autob && !strcmp (autob, e->name))
            {
               revk_web_send (req, " selected");
               found = 1;
            }
            revk_web_send (req, ">%s", e->name);
            if (!e->missing && e->rssi)
               revk_web_send (req, " %ddB", e->rssi);
         }
         if (!found && *autob)
         {
            revk_web_send (req, "<option selected value=\"%s\">%s", autob, autob);
         }
         revk_web_send (req, "</select>");
         if (ble && (uptime () < 60 || !found))
            revk_web_send (req, " (reload to refresh list)");
         revk_web_send (req, "</td></tr>");
      }
      revk_web_send (req, "</table></div>");
   }
#endif
   revk_web_send (req, "</form>"        //
                  "</div>"      //
                  "<script>"    //
                  "var ws=0;"   //
                  "var reboot=0;"       //
                  "function cf(v){return %s;}"  //
                  "function g(n){return document.getElementById(n);};"  //
                  "function b(n,v){var d=g(n);if(d)d.checked=v;}"       //
                  "function h(n,v){var d=g(n);if(d)d.style.display=v?'block':'none';}"  //
                  "function s(n,v){var d=g(n);if(d)d.textContent=v;}"   //
                  "function n(n,v){var d=g(n);if(d)d.value=v;}" //
                  "function e(n,v){var d=g(n+v);if(d)d.checked=true;}"  //
                  "function w(n,v){var m=new Object();m[n]=v;ws.send(JSON.stringify(m));}"      //
                  "function t(n,v){s(n,v!=undefined?cf(v):'---');}"     //
                  "function c(){"       //
                  "ws=new WebSocket('ws://'+window.location.host+'/status');"   //
                  "ws.onopen=function(v){g('top').className='on';};"    //
                  "ws.onclose=function(v){ws=undefined;g('top').className='off';if(reboot)location.reload();};" //
                  "ws.onerror=function(v){ws.close();};"        //
                  "ws.onmessage=function(v){"   //
                  "o=JSON.parse(v.data);"       //
                  "b('power',o.power);" //
                  "h('offline',!o.online);"     //
                  "h('loopback',o.loopback);"   //
                  "h('control',o.control);"     //
                  "h('slave',o.slave);" //
                  "h('remote',!o.remote);"      //
                  "b('swingh',o.swingh);"       //
                  "b('swingv',o.swingv);"       //
                  "b('econo',o.econo);" //
                  "b('powerful',o.powerful);"   //
                  "b('comfort',o.comfort);"     //
                  "b('sensor',o.sensor);"       //
                  "b('led',o.led);"     //
                  "b('quiet',o.quiet);" //
                  "b('streamer',o.streamer);"   //
                  "e('mode',o.mode);"   //
                  "t('Inlet',o.inlet);" //
                  "t('Home',o.home);"   //
                  "t('Env',o.env);"     //
                  "t('Outside',o.outside);"     //
                  "t('Liquid',o.liquid);"       //
                  "if(o.ble)t('BLE',o.ble.temp);"       //
                  "if(o.ble)s('Hum',o.ble.hum?o.ble.hum+'%%':'');"      //
                  "n('demand',o.demand);"       //
                  "s('Tdemand',(o.demand!=undefined?o.demand+'%%':'---'));"     //
                  "n('temp',o.temp);"   //
                  "s('Ttemp',(o.temp?cf(o.temp):'---')+(o.control?'✷':''));"  //
                  "b('autop',o.autop);" //
                  "e('autor',o.autor);" //
                  "n('autob',o.autob);" //
                  "n('auto0',o.auto0);" //
                  "n('auto1',o.auto1);" //
                  "n('autot',o.autot);" //
                  "s('Tautot',(o.autot?cf(o.autot):''));"       //
                  "s('0/1',(o.slave?'❋':'')+(o.antifreeze?'❄':''));"        //
                  "s('Fan',(o.fanrpm?o.fanrpm+'RPM':'')+(o.antifreeze?'❄':'')+(o.control?'✷':''));" //
                  "e('fan',o.fan);"     //
                  "if(o.shutdown){reboot=true;s('shutdown','Restarting: '+o.shutdown);h('shutdown',true);};"    //
                  "};};c();"    //
                  "setInterval(function() {if(!ws)c();else ws.send('');},1000);"        //
                  "</script>", fahrenheit ? "Math.round(10*((v*9/5)+32))/10+'℉'" : "v+'℃'");
   return revk_web_foot (req, 0, websettings, protocol_set ? proto_name () : NULL);
}

static const char *
get_query (httpd_req_t * req, char *buf, size_t buf_len)
{
   if (httpd_req_get_url_query_len (req) && !httpd_req_get_url_query_str (req, buf, buf_len))
      return NULL;
   else
      return "Required arguments missing";
}

// Macros with error collection for HTTP
#define	daikin_set_v_e(err,name,value) {      \
   const char * e = daikin_set_v(name, value); \
   if (e) err = e;                             \
}
#define	daikin_set_i_e(err,name,value) {      \
	const char * e = daikin_set_i(name, value); \
   if (e) err = e;                             \
}
#define	daikin_set_e_e(err,name,value) {      \
   const char *e = daikin_set_e(name, value);  \
   if (e) err = e;                             \
}
#define	daikin_set_t_e(err,name,value) {      \
   const char * e = daikin_set_t(name, value); \
   if (e) err = e;                             \
}

// Our own JSON-based control interface starts here

static esp_err_t
web_status (httpd_req_t * req)
{                               // Web socket status report
   int fd = httpd_req_to_sockfd (req);
   void wsend (jo_t * jp)
   {
      char *js = jo_finisha (jp);
      if (js)
      {
         httpd_ws_frame_t ws_pkt;
         memset (&ws_pkt, 0, sizeof (httpd_ws_frame_t));
         ws_pkt.payload = (uint8_t *) js;
         ws_pkt.len = strlen (js);
         ws_pkt.type = HTTPD_WS_TYPE_TEXT;
         httpd_ws_send_frame_async (req->handle, fd, &ws_pkt);
         free (js);
      }
   }
   esp_err_t status (void)
   {
      jo_t j = daikin_status ();
      const char *reason;
      int t = revk_shutting_down (&reason);
      if (t)
         jo_string (j, "shutdown", reason);
      wsend (&j);
      return ESP_OK;
   }
   if (req->method == HTTP_GET)
      return status ();         // Send status on initial connect
   // received packet
   httpd_ws_frame_t ws_pkt;
   uint8_t *buf = NULL;
   memset (&ws_pkt, 0, sizeof (httpd_ws_frame_t));
   ws_pkt.type = HTTPD_WS_TYPE_TEXT;
   esp_err_t ret = httpd_ws_recv_frame (req, &ws_pkt, 0);
   if (ret)
      return ret;
   if (!ws_pkt.len)
      return status ();         // Empty string
   buf = calloc (1, ws_pkt.len + 1);
   if (!buf)
      return ESP_ERR_NO_MEM;
   ws_pkt.payload = buf;
   ret = httpd_ws_recv_frame (req, &ws_pkt, ws_pkt.len);
   if (!ret)
   {
      jo_t j = jo_parse_mem (buf, ws_pkt.len);
      if (j)
      {
         daikin_control (j);
         jo_free (&j);
      }
   }
   free (buf);
   return status ();
}

// Legacy API
// The following handlers provide web-based control protocol, compatible
// with original Daikin BRP series online controllers.
// These functions make use of the JSON library, even though the requests are query form formatted, and replies are comma formatted

static jo_t
legacy_ok (void)
{
   jo_t j = jo_object_alloc ();
   jo_string (j, "ret", "ok");
   return j;
}

static esp_err_t
legacy_send (httpd_req_t * req, jo_t * jp)
{
   httpd_resp_set_type (req, "text/plain");
   if (jp && *jp)
   {
      jo_t j = *jp;
      int len = jo_len (j);
      char *buf = mallocspi (len + 40),
         *p = buf;;
      if (buf)
      {
         jo_rewind (j);
         while (jo_next (j) == JO_TAG && p + 3 - buf < len)
         {
            if (p > buf)
               *p++ = ',';
            int l = jo_strlen (j);
            if (p - buf + l + 2 > len)
               break;
            jo_strncpy (j, p, l + 1);
            p += l;
            *p++ = '=';
            if (jo_next (j) < JO_STRING)
               break;
            l = jo_strlen (j);
            *p = 0;
            if (p - buf + l + 2 > len)
               break;
            jo_strncpy (j, p, l + 1);
            p += l;
         }
         *p = 0;
         httpd_resp_sendstr (req, buf);
         free (buf);
      }
      jo_free (jp);
   }
   return ESP_OK;
}

static esp_err_t
legacy_simple_response (httpd_req_t * req, const char *err)
{
   jo_t j = jo_object_alloc ();
   if (err && *err)
   {
      jo_string (j, "ret", "PARAM NG");
      jo_string (j, "adv", err);
   } else
   {

      jo_string (j, "ret", "OK");
      jo_string (j, "adv", "");
   }
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_set_demand_control (httpd_req_t * req)
{
   const char *err = NULL;
   jo_t j = revk_web_query (req);
   if (!j)
      err = "Query failed";
   else
   {
      int on = 0,
         demand = 100;
      if (jo_find (j, "en_demand"))
      {
         char *v = jo_strdup (j);
         if (v)
            on = atoi (v);
         free (v);
      }
      if (jo_find (j, "max_pow"))
      {
         char *v = jo_strdup (j);
         if (v)
            demand = atoi (v);
         free (v);
      }
      daikin_set_i_e (err, demand, on ? demand : 100);
      jo_free (&j);
   }
   return legacy_simple_response (req, err);
}

static esp_err_t
legacy_web_get_basic_info (httpd_req_t * req)
{
   // Full string from my BRP module:
   // ret=OK,type=aircon,reg=eu,dst=0,ver=3_3_9,pow=0,err=0,location=0,name=%4c%69%76%69%6e%67%20%72%6f%6f%6d,
   // icon=2,method=home only,port=30050,id=,pw=,lpw_flag=0,adp_kind=2,pv=2,cpv=2,cpv_minor=00,led=1,en_setzone=1,
   // mac=<my_mac>,adp_mode=run,en_hol=0,ssid1=<my_ssid>,radio1=-60,grp_name=,en_grp=0
   jo_t j = legacy_ok ();
   jo_string (j, "type", "aircon");
   jo_string (j, "reg", "eu");
   jo_string (j, "mac", revk_id);
   jo_string (j, "ssid", revk_wifi ());
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_get_model_info (httpd_req_t * req)
{
   jo_t j = legacy_ok ();
   jo_string (j, "model", daikin.model);
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_get_control_info (httpd_req_t * req)
{
   jo_t j = legacy_ok ();
   jo_int (j, "pow", daikin.power);
   if (daikin.mode <= 7)
      jo_stringf (j, "mode", "%c", "64370002"[daikin.mode]);
   jo_string (j, "adv", daikin.powerful ? "2" : "");
   jo_litf (j, "stemp", "%.1f", daikin.temp);
   jo_int (j, "shum", 0);
   for (int i = 1; i <= 7; i++)
      if (i != 6)
      {
         char tag[4] = { 'd', 't', '0' + i };
         jo_litf (j, tag, "%.1f", daikin.temp);
      }
   for (int i = 1; i <= 7; i++)
      if (i != 6)
      {
         char tag[4] = { 'd', 'h', '0' + i };
         jo_int (j, tag, 0);
      }
   jo_int (j, "dhh", 0);
   jo_litf (j, "b_stemp", "%.1f", daikin.temp);
   jo_int (j, "b_shum", 0);
   jo_int (j, "alert", 255);
   if (daikin.fan <= 6)
      jo_stringf (j, "f_rate", "%c", "A34567B"[daikin.fan]);
   jo_int (j, "f_dir", daikin.swingh * 2 + daikin.swingv);
   for (int i = 1; i <= 7; i++)
      if (i != 6)
      {
         char tag[5] = { 'd', 'f', 'r', '0' + i };
         jo_int (j, tag, 0);
      }
   jo_int (j, "dfrh", 0);
   for (int i = 1; i <= 7; i++)
      if (i != 6)
      {
         char tag[5] = { 'd', 'f', 'd', '0' + i };
         jo_int (j, tag, 0);
      }
   jo_int (j, "dfdh", 0);
   jo_int (j, "dmnd_run", 0);
   jo_int (j, "en_demand", (daikin.status_known & CONTROL_demand) && daikin.demand < 100 ? 1 : 0);
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_set_control_info (httpd_req_t * req)
{
   const char *err = NULL;
   jo_t j = revk_web_query (req);
   if (!j)
      err = "Query failed";
   else
   {
      if (jo_find (j, "pow"))
      {
         char *v = jo_strdup (j);
         if (v)
            daikin_set_v_e (err, power, atoi (v));
         free (v);
      }
      if (jo_find (j, "mode"))
      {
         char *v = jo_strdup (j);
         if (v)
         {
            int n = atoi (v);
            static int8_t modes[] = { 3, 3, 7, 2, 1, -1, 0, 3 };        // AADCH-FA
            int8_t setval = (n >= 0 && n <= 7) ? modes[n] : -1;
            if (setval == -1)
               err = "Invalid mode value";
            else
               daikin_set_v_e (err, mode, setval);
         }
         free (v);
      }
      if (jo_find (j, "stemp"))
      {
         char *v = jo_strdup (j);
         if (v)
            daikin_set_t_e (err, temp, strtof (v, NULL));
         free (v);
      }
      if (jo_find (j, "f_rate"))
      {
         char *v = jo_strdup (j);
         if (v)
         {
            int8_t setval;
            if (*v == 'A')
               setval = 0;
            else if (*v == 'B')
               setval = 6;
            else if (*v >= '3' && *v <= '7')
               setval = *v - '2';
            else
               setval = -1;
            if (setval == -1)
               err = "Invalid f_rate value";
            else
               daikin_set_v_e (err, fan, setval);
         }
         free (v);
      }
      if (jo_find (j, "f_dir"))
      {
         // *value is a bitfield, expressed as a single ASCII digit '0' - '3'
         // Since '0' is 0x30, we don't bother, bit checks work as they should
         char *v = jo_strdup (j);
         if (v)
         {
            int n = atoi (v);
            daikin_set_v_e (err, swingv, n);
            daikin_set_v_e (err, swingh, !!(n & 2));
         }
         free (v);
      }
      jo_free (&j);
   }
   return legacy_simple_response (req, err);
}

static esp_err_t
legacy_web_get_sensor_info (httpd_req_t * req)
{
   jo_t j = legacy_ok ();
   if (daikin.status_known & CONTROL_home)
      jo_litf (j, "htemp", "%.2f", daikin.home);
   else
      jo_string (j, "htemp", "-");
   jo_string (j, "hhum", "-");
   if (daikin.status_known & CONTROL_outside)
      jo_litf (j, "otemp", "%.2f", daikin.outside);
   else
      jo_string (j, "otemp", "-");
   jo_int (j, "err", 0);
   jo_string (j, "cmpfreq", "-");
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_register_terminal (httpd_req_t * req)
{
   // This is called with "?key=<security_key>" parameter if any other URL
   // responds with 403. It's supposed that we remember our client and enable access.
   // We don't support authentication currently, so let's just return OK
   // However, it could be a nice idea to have in future
   jo_t j = legacy_ok ();
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_get_year_power (httpd_req_t * req)
{
   // ret=OK,curr_year_heat=0/0/0/0/0/0/0/0/0/0/0/0,prev_year_heat=0/0/0/0/0/0/0/0/0/0/0/0,curr_year_cool=0/0/0/0/0/0/0/0/0/0/0/0,prev_year_cool=0/0/0/0/0/0/0/0/0/0/0/0
   // Have no idea how to implement it, perhaps the original module keeps some internal statistics.
   // For now let's just prevent errors in OpenHAB and return an empty OK response
   // Note all zeroes from my BRP
   jo_t j = legacy_ok ();
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_get_week_power (httpd_req_t * req)
{
   // ret=OK,s_dayw=2,week_heat=0/0/0/0/0/0/0/0/0/0/0/0/0/0,week_cool=0/0/0/0/0/0/0/0/0/0/0/0/0/0
   // Have no idea how to implement it, perhaps the original module keeps some internal statistics.
   // For now let's just prevent errors in OpenHAB and return an empty OK response
   // Note all zeroes from my BRP
   jo_t j = legacy_ok ();
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_set_special_mode (httpd_req_t * req)
{
   char query[200];
   const char *err = get_query (req, query, sizeof (query));
   if (!err)
   {
      char mode[6],
        value[2];
      if (!httpd_query_key_value (query, "spmode_kind", mode, sizeof (mode)) &&
          !httpd_query_key_value (query, "set_spmode", value, sizeof (value)))
      {
         if (!strcmp (mode, "12"))
            err = daikin_set_v (econo, *value == '1');
         else if (!strcmp (mode, "2"))
            err = daikin_set_v (powerful, *value == '1');
         else
            err = "Unsupported spmode_kind value";
         // TODO comfort/streamer/sensor/quiet

         // The following other modes are known from OpenHAB sources:
         // STREAMER "13"
         // POWERFUL_STREAMER "2/13"
         // ECO_STREAMER "12/13"
         // Don't know what to do with them and my AC doesn't support them
      }
   }

   return legacy_simple_response (req, err);
}

static void
send_ha_config (void)
{
   daikin.ha_send = 0;
   char *topic;
   jo_t make (const char *tag, const char *icon)
   {
      jo_t j = jo_object_alloc ();
      jo_stringf (j, "unique_id", "%s%s", revk_id, tag);
      jo_object (j, "dev");
      jo_array (j, "ids");
      jo_string (j, NULL, revk_id);
      jo_close (j);
      jo_string (j, "name", hostname);
      if (*daikin.model)
         jo_string (j, "mdl", daikin.model);
      jo_string (j, "sw", revk_version);
      jo_string (j, "mf", "RevK");
      jo_stringf (j, "cu", "http://%s.local/", hostname);
      jo_close (j);
      if (icon)
         jo_string (j, "icon", icon);
      return j;
   }
   void addtemp (uint64_t ok, const char *tag, const char *icon)
   {
      if (asprintf (&topic, "homeassistant/sensor/%s%s/config", revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", tag);
            jo_string (j, "dev_cla", "temperature");
            jo_string (j, "stat_t", revk_id);
            jo_string (j, "unit_of_meas", "°C");
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   void addhum (uint64_t ok, const char *tag, const char *icon)
   {
      if (asprintf (&topic, "homeassistant/sensor/%s%s/config", revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", tag);
            jo_string (j, "dev_cla", "humidity");
            jo_string (j, "stat_t", revk_id);
            jo_string (j, "unit_of_meas", "%");
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   void addfreq (uint64_t ok, const char *tag, const char *unit, const char *icon)
   {
      if (asprintf (&topic, "homeassistant/sensor/%s%s/config", revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", tag);
            jo_string (j, "dev_cla", "frequency");
            jo_string (j, "stat_t", revk_id);
            jo_string (j, "unit_of_meas", unit);
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   void addbat (uint64_t ok, const char *tag, const char *icon)
   {
      if (asprintf (&topic, "homeassistant/sensor/%s%s/config", revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", tag);
            jo_string (j, "dev_cla", "battery");
            jo_string (j, "stat_t", revk_id);
            jo_string (j, "unit_of_meas", "%");
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   if (asprintf (&topic, "homeassistant/climate/%s/config", revk_id) >= 0)
   {
      jo_t j = make ("", "mdi:thermostat");
      //jo_string (j, "name", hostname);
      //jo_null(j,"name");
      jo_stringf (j, "~", "command/%s", hostname);      // Prefix for command
      jo_int (j, "min_temp", tmin);
      jo_int (j, "max_temp", tmax);
      jo_string (j, "temp_unit", "C");
      jo_string (j, "temp_cmd_t", "~/temp");
      jo_string (j, "temp_stat_t", revk_id);
      jo_string (j, "temp_stat_tpl", "{{value_json.target}}");
      if (daikin.status_known & (CONTROL_inlet | CONTROL_home))
      {
         jo_string (j, "curr_temp_t", revk_id);
         jo_string (j, "curr_temp_tpl", "{{value_json.temp}}");
      }
      if (daikin.status_known & CONTROL_mode)
      {
         jo_string (j, "mode_cmd_t", "~/mode");
         jo_string (j, "mode_stat_t", revk_id);
         jo_string (j, "mode_stat_tpl", "{{value_json.mode}}");
      }
      if (daikin.status_known & CONTROL_fan)
      {
         jo_string (j, "fan_mode_cmd_t", "~/fan");
         jo_string (j, "fan_mode_stat_t", revk_id);
         jo_string (j, "fan_mode_stat_tpl", "{{value_json.fan}}");
         if (have_5_fan_speeds ())
         {
            jo_array (j, "fan_modes");
            for (int f = 0; f < sizeof (fans) / sizeof (*fans); f++)
               jo_string (j, NULL, fans[f]);
            jo_close (j);
         }
      }
      if (daikin.status_known & (CONTROL_swingh | CONTROL_swingv | CONTROL_comfort))
      {
         jo_string (j, "swing_mode_cmd_t", "~/swing");
         jo_string (j, "swing_mode_stat_t", revk_id);
         jo_string (j, "swing_mode_stat_tpl", "{{value_json.swing}}");
         jo_array (j, "swing_modes");
         jo_string (j, NULL, "off");
         if (daikin.status_known & CONTROL_swingh)
            jo_string (j, NULL, "H");
         if (daikin.status_known & CONTROL_swingv)
            jo_string (j, NULL, "V");
         if ((daikin.status_known & (CONTROL_swingh | CONTROL_swingv)) == (CONTROL_swingh | CONTROL_swingv))
            jo_string (j, NULL, "H+V");
         if (daikin.status_known & CONTROL_comfort)
            jo_string (j, NULL, "C");
         jo_close (j);
      }
      if (daikin.status_known & (CONTROL_econo | CONTROL_powerful))
      {
         jo_string (j, "pr_mode_cmd_t", "~/preset");
         jo_string (j, "pr_mode_stat_t", revk_id);
         jo_string (j, "pr_mode_val_tpl", "{{value_json.preset}}");
         jo_array (j, "pr_modes");
         if (daikin.status_known & CONTROL_econo)
            jo_string (j, NULL, "eco");
         if (daikin.status_known & CONTROL_powerful)
            jo_string (j, NULL, "boost");
         jo_string (j, NULL, "home");
         jo_close (j);
      }
      revk_mqtt_send (NULL, 1, topic, &j);
      free (topic);
   }
   addtemp ((daikin.status_known & CONTROL_home) && (daikin.status_known & CONTROL_inlet), "inlet", "mdi:thermometer"); // Both defined so we used home as temp, so lets add inlet here
   addtemp (daikin.status_known & CONTROL_outside, "outside", "mdi:thermometer");
   addtemp (daikin.status_known & CONTROL_liquid, "liquid", "mdi:coolant-temperature");
   addfreq (daikin.status_known & CONTROL_comp, "comp", "Hz", "mdi:sine-wave");
   addfreq (daikin.status_known & CONTROL_fanrpm, "fanfreq", "Hz", "mdi:fan");
   addtemp (ble && bletemp && bletemp->tempset, "bletemp", "mdi:thermometer");
   addhum (ble && bletemp && bletemp->humset, "blehum", "mdi:water-percent");
   addbat (ble && bletemp && bletemp->batset, "blebat", "mdi:battery-bluetooth-variant");
#if 1
   if (asprintf (&topic, "homeassistant/select/%sdemand/config", revk_id) >= 0)
   {
      if (!(daikin.status_known & CONTROL_demand))
         revk_mqtt_send_str (topic);
      else
      {
         jo_t j = make ("demand", NULL);
         jo_string (j, "name", "Demand control");
         jo_stringf (j, "command_topic", "%s/demand", revk_id);
         jo_array (j, "options");
         for (int i = 30; i <= 100; i += 5)
            jo_stringf (j, NULL, "%d", i);
         jo_close (j);
         revk_mqtt_send (NULL, 1, topic, &j);
      }
      free (topic);
   }
#endif
}

static void
ha_status (void)
{                               // Home assistant message
   if (!ha)
      return;
   jo_t j = jo_object_alloc ();
   if (b.loopback)
      jo_bool (j, "loopback", 1);
   else if (daikin.status_known & CONTROL_online)
      jo_bool (j, "online", daikin.online);
   if (daikin.status_known & CONTROL_temp)
      jo_litf (j, "target", "%.2f", autor ? (float) autot / autot_scale : daikin.temp); // Target - either internal or what we are using as reference
   if (daikin.status_known & CONTROL_env)
      jo_litf (j, "temp", "%.2f", daikin.env);  // The external temperature
   else if (daikin.status_known & CONTROL_home)
      jo_litf (j, "temp", "%.2f", daikin.home); // We use home if present, else inlet
   else if (daikin.status_known & CONTROL_inlet)
      jo_litf (j, "temp", "%.2f", daikin.inlet);
   if ((daikin.status_known & CONTROL_home) && (daikin.status_known & CONTROL_inlet))
      jo_litf (j, "inlet", "%.2f", daikin.inlet);       // Both so report inlet as well
   if (daikin.status_known & CONTROL_outside)
      jo_litf (j, "outside", "%.2f", daikin.outside);
   if (daikin.status_known & CONTROL_liquid)
      jo_litf (j, "liquid", "%.2f", daikin.liquid);
   if (daikin.status_known & CONTROL_comp)
      jo_int (j, "comp", daikin.comp);
   if (daikin.status_known & CONTROL_demand)
      jo_int (j, "demand", daikin.demand);
#if 0
   if (daikin.status_known & CONTROL_fanrpm)
      jo_int (j, "fanrpm", daikin.fanrpm);
#else
   if (daikin.status_known & CONTROL_fanrpm)
      jo_litf (j, "fanfreq", "%.1f", daikin.fanrpm / 60.0);
#endif
   if (ble && bletemp)
   {
      if (bletemp->tempset)
         jo_litf (j, "bletemp", "%.2f", bletemp->temp / 100.0);
      if (bletemp->humset)
         jo_litf (j, "blehum", "%.2f", bletemp->hum / 100.0);
      if (bletemp->batset)
         jo_int (j, "blebat", bletemp->bat);
   }
   if (daikin.status_known & CONTROL_mode)
   {
      const char *modes[] = { "fan_only", "heat", "cool", "auto", "4", "5", "6", "dry" };       // FHCA456D
      jo_string (j, "mode", daikin.power ? autor && !lockmode ? "auto" : modes[daikin.mode] : "off");   // If we are controlling, it is auto
   }
   if (daikin.status_known & CONTROL_fan)
      jo_string (j, "fan", fans[daikin.fan]);
   if (daikin.status_known & (CONTROL_swingh | CONTROL_swingv | CONTROL_comfort))
      jo_string (j, "swing",
                 daikin.comfort ? "C" : daikin.swingh & daikin.swingv ? "H+V" : daikin.swingh ? "H" : daikin.swingv ? "V" : "off");
   if (daikin.status_known & (CONTROL_econo | CONTROL_powerful))
      jo_string (j, "preset", daikin.econo ? "eco" : daikin.powerful ? "boost" : "home");       // Limited modes
   revk_mqtt_send_clients (NULL, 1, revk_id, &j, 1);
}

static void
register_uri (const httpd_uri_t * uri_struct)
{
   esp_err_t res = httpd_register_uri_handler (webserver, uri_struct);
   if (res != ESP_OK)
   {
      ESP_LOGE (TAG, "Failed to register %s, error code %d", uri_struct->uri, res);
   }
}

static void
register_get_uri (const char *uri, esp_err_t (*handler) (httpd_req_t * r))
{
   httpd_uri_t uri_struct = {
      .uri = uri,
      .method = HTTP_GET,
      .handler = handler,
   };
   register_uri (&uri_struct);
}

static void
register_ws_uri (const char *uri, esp_err_t (*handler) (httpd_req_t * r))
{
   httpd_uri_t uri_struct = {
      .uri = uri,
      .method = HTTP_GET,
      .handler = handler,
      .is_websocket = true,
   };
   register_uri (&uri_struct);
}

void
revk_web_extra (httpd_req_t * req)
{
   revk_web_setting (req, "Fahrenheit", "fahrenheit");
   revk_web_setting (req, "Home Assistant", "ha");
   revk_web_setting (req, "Dark mode LED", "dark");
   if (!daikin.remote)
   {
      revk_web_setting (req, "No Faikin auto mode", "nofaikinauto");
      if (!nofaikinauto)
         revk_web_setting (req, "BLE Sensors", "ble");
   }
   revk_web_setting (req, "Dump protocol over MQTT", "dump");
}

// --------------------------------------------------------------------------------
// Main
void
app_main ()
{
#ifdef  CONFIG_IDF_TARGET_ESP32S3
   {                            // All unused input pins pull down
      gpio_config_t c = {.pull_down_en = 1,.mode = GPIO_MODE_DISABLE };
      for (uint8_t p = 0; p <= 48; p++)
         if (gpio_ok (p) & 2)
            c.pin_bit_mask |= (1LL << p);
      gpio_config (&c);
   }
#endif
   daikin.mutex = xSemaphoreCreateMutex ();
   daikin.status_known = CONTROL_online;
#define	t(name)	daikin.name=NAN;
#define	r(name)	daikin.min##name=NAN;daikin.max##name=NAN;
#include "acextras.m"
   revk_boot (&mqtt_client_callback);
   revk_start ();
   b.dumping = dump;
   revk_blink (0, 0, "");
   void uart_setup (void)
   {
      esp_err_t err = 0;
      if (!protocol_set && !b.loopback)
      {
         proto++;
         if (proto >= PROTO_TYPE_MAX * PROTO_SCALE)
            proto = 0;
      }
      ESP_LOGI (TAG, "Trying %s Tx %s%d Rx %s%d", proto_name (), (proto & PROTO_TXINVERT) ? "¬" : "",
                tx.num, (proto & PROTO_RXINVERT) ? "¬" : "", rx.num);
      if (!err)
         err = gpio_reset_pin (rx.num);
      if (!err)
         err = gpio_reset_pin (tx.num);
      if (proto_type () == PROTO_TYPE_CN_WIRED)
      {
         if (!rmt_encoder)
         {
            rmt_copy_encoder_config_t encoder_config = {
            };
            REVK_ERR_CHECK (rmt_new_copy_encoder (&encoder_config, &rmt_encoder));
         }
         if (!rmt_tx)
         {                      // Create rmt_tx
            rmt_tx_channel_config_t tx_chan_config = {
               .clk_src = RMT_CLK_SRC_DEFAULT,  // select source clock
               .gpio_num = tx.num,      // GPIO number
               .mem_block_symbols = 72, // symbols
               .resolution_hz = 1 * 1000 * 1000,        // 1 MHz tick resolution, i.e., 1 tick = 1 µs
               .trans_queue_depth = 1,  // set the number of transactions that can pend in the background
               .flags.invert_out = (tx.invert ^ ((proto & PROTO_TXINVERT) ? 1 : 0)),
#ifdef  CONFIG_IDF_TARGET_ESP32S3
               .flags.with_dma = true,
#endif
            };
            REVK_ERR_CHECK (rmt_new_tx_channel (&tx_chan_config, &rmt_tx));
            if (rmt_tx)
               REVK_ERR_CHECK (rmt_enable (rmt_tx));
         }
         if (!rmt_rx)
         {                      // Create rmt_rx
            rmt_rx_channel_config_t rx_chan_config = {
               .clk_src = RMT_CLK_SRC_DEFAULT,  // select source clock
               .resolution_hz = 1 * 1000 * 1000,        // 1MHz tick resolution, i.e. 1 tick = 1us
               .mem_block_symbols = 72, // 
               .gpio_num = rx.num,      // GPIO number
               .flags.invert_in = (rx.invert ^ ((proto & PROTO_RXINVERT) ? 1 : 0)),
#ifdef  CONFIG_IDF_TARGET_ESP32S3
               .flags.with_dma = true,
#endif
            };
            REVK_ERR_CHECK (rmt_new_rx_channel (&rx_chan_config, &rmt_rx));
            if (rmt_rx)
            {
               rmt_rx_event_callbacks_t cbs = {
                  .on_recv_done = rmt_rx_callback,
               };
               REVK_ERR_CHECK (rmt_rx_register_event_callbacks (rmt_rx, &cbs, NULL));
               REVK_ERR_CHECK (rmt_enable (rmt_rx));
            }
         }
      } else
      {
         if (rmt_tx)
         {
            REVK_ERR_CHECK (rmt_disable (rmt_tx));
            REVK_ERR_CHECK (rmt_del_channel (rmt_tx));
            rmt_tx = NULL;
         }
         if (rmt_rx)
         {
            REVK_ERR_CHECK (rmt_disable (rmt_rx));
            REVK_ERR_CHECK (rmt_del_channel (rmt_rx));
            rmt_rx = NULL;
         }
         uart_driver_delete (uart);
         uart_config_t uart_config = {
            .baud_rate = (proto_type () == PROTO_TYPE_S21) ? 2400 : 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_EVEN,
            .stop_bits = (proto_type () == PROTO_TYPE_S21) ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
         };
         if (!err)
            err = uart_param_config (uart, &uart_config);
         if (!err)
            err = uart_set_pin (uart, tx.num, rx.num, -1, -1);
         if (!err)
            err = gpio_pullup_en (rx.num);
         if (!err)
         {
            uint8_t i = 0;
            if (rx.invert ^ ((proto & PROTO_RXINVERT) ? 1 : 0))
               i |= UART_SIGNAL_RXD_INV;
            if (tx.invert ^ ((proto & PROTO_TXINVERT) ? 1 : 0))
               i |= UART_SIGNAL_TXD_INV;
            err = uart_set_line_inverse (uart, i);
         }
         if (!err)
            err = uart_driver_install (uart, 1024, 0, 0, NULL, 0);
         if (!err)
            err = uart_set_rx_full_threshold (uart, 1);
         if (err)
         {
            jo_t j = jo_object_alloc ();
            jo_string (j, "error", "Failed to uart");
            jo_int (j, "uart", uart);
            jo_int (j, "gpio", rx.num);
            jo_string (j, "description", esp_err_to_name (err));
            revk_error ("uart", &j);
            return;
         }
         sleep (1);
         uart_flush (uart);
      }
   }

   if (webcontrol || websettings)
   {
      // Web interface
      httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
      config.stack_size += 2048;        // Being on the safe side
      // When updating the code below, make sure this is enough
      // Note that we're also adding revk's own web config handlers
      config.max_uri_handlers = 13 + revk_num_web_handlers ();
      if (!httpd_start (&webserver, &config))
      {
         if (websettings)
            revk_web_settings_add (webserver);
         register_get_uri ("/", web_root);
         if (webcontrol)
         {
            register_get_uri ("/apple-touch-icon.png", web_icon);
            register_ws_uri ("/status", web_status);
            register_get_uri ("/common/basic_info", legacy_web_get_basic_info);
            register_get_uri ("/aircon/get_model_info", legacy_web_get_model_info);
            register_get_uri ("/aircon/get_control_info", legacy_web_get_control_info);
            register_get_uri ("/aircon/set_control_info", legacy_web_set_control_info);
            register_get_uri ("/aircon/get_sensor_info", legacy_web_get_sensor_info);
            register_get_uri ("/common/register_terminal", legacy_web_register_terminal);
            register_get_uri ("/aircon/get_year_power_ex", legacy_web_get_year_power);
            register_get_uri ("/aircon/get_week_power_ex", legacy_web_get_week_power);
            register_get_uri ("/aircon/set_special_mode", legacy_web_set_special_mode);
            register_get_uri ("/aircon/set_demand_control", legacy_web_set_demand_control);
         }
         // When adding, update config.max_uri_handlers
      }
   }
#ifdef	ELA
   if (ble)
      bleenv_run ();
   else
      esp_wifi_set_ps (WIFI_PS_NONE);
#endif
   if (!tx.set && !rx.set)
   {                            // Mock for interface development and testing
      ESP_LOGE (TAG, "Dummy operational mode (no tx/rx set)");
      daikin.status_known |=
         CONTROL_power | CONTROL_fan | CONTROL_temp | CONTROL_mode | CONTROL_econo | CONTROL_powerful |
         CONTROL_comfort | CONTROL_streamer | CONTROL_sensor | CONTROL_quiet | CONTROL_swingv | CONTROL_swingh;
      daikin.power = 1;
      daikin.mode = 1;
      daikin.temp = 20.0;
   }
   strncpy (daikin.model, model, sizeof (daikin.model));        // Default model
   proto = protocol;
   if (protofix)
      protocol_set = 1;         // Fixed protocol - do not change
   else if (proto >= PROTO_TYPE_MAX * PROTO_SCALE && proto_type () < sizeof (prototype) / sizeof (*prototype))
   {                            // Manually set protocol above the auto scanning range
      protocol_set = 1;
   } else
      proto--;
   while (1)
   {                            // Main loop
      // We're (re)starting comms from scratch, so set "talking" flag.
      // This signals protocol integrity and actually enables communicating with the AC.
      daikin.talking = 1;
      if (tx.set && rx.set)
      {
         // Poke UART
         uart_setup ();
         if ((proto_type () == PROTO_TYPE_X50A))
         {                      // Startup X50A
            daikin_x50a_command (0xAA, 1, (uint8_t[])
                                 {
                                 0x01}
            );
            daikin_x50a_command (0xBA, 0, NULL);
            daikin_x50a_command (0xBB, 0, NULL);
         }
         if (protocol_set && daikin.online != daikin.talking)
         {
            daikin.online = daikin.talking;
            daikin.status_changed = 1;
         }
      } else
      {                         // Mock configuration for interface testing
         proto = PROTO_TYPE_S21 * PROTO_SCALE;
         protocol_set = 1;
         daikin.control_changed = 0;
         daikin.online = 1;
      }
      if (ha)
         daikin.ha_send = 1;
      do
      {
         // Polling loop. We exit from here only if we get a protocol error
         if (proto_type () != PROTO_TYPE_CN_WIRED)
            usleep (1000000LL - (esp_timer_get_time () % 1000000LL));   /* wait for next second  - CN_WIRED has built in wait */
#ifdef ELA
         if (ble && *autob)
         {                      // Automatic external temperature logic - only really useful if autor/autot set
            bleenv_expire (120);
            if (!bletemp || strcmp (bletemp->name, autob))
            {
               bletemp = NULL;
               bleenv_clean ();
               for (bleenv_t * e = bleenv; e; e = e->next)
                  if (!strcmp (e->name, autob))
                  {
                     bletemp = e;
                     if (ha)
                        daikin.ha_send = 1;
                     break;
                  }
            }
            if (bletemp && !bletemp->missing && bletemp->tempset)
            {                   // Use temp
               daikin.env = bletemp->temp / 100.0;
               daikin.status_known |= CONTROL_env;      // So we report it
            } else
               daikin.status_known &= ~CONTROL_env;     // So we don't report it
         }
#endif
         if (autor && autot)
         {                      // Automatic setting of "external" controls, autot is temp(*autot_scale), autor is range(*autor_scale), autob is BLE name
            daikin.controlvalid = uptime () + 10;
            daikin.mintarget = (float) autot / autot_scale - (float) autor / autor_scale;
            daikin.maxtarget = (float) autot / autot_scale + (float) autor / autor_scale;
         }
         // Talk to the AC
         if (tx.set && rx.set)
         {
            if (proto_type () == PROTO_TYPE_CN_WIRED)
            {                   // CN WIRED
               uint8_t cmd[CN_WIRED_LEN] = { 0 };
               cmd[0] = ((int) (daikin.temp) / 10) * 0x10 + ((int) (daikin.temp) % 10);
               if (cmd[0] == 0xC7)
                  cmd[0] = 0x20;        // temp was not set
               cmd[1] = cmd[0]; // Unknown why
               cmd[3] = ((const uint8_t[])
                         { 0x01, 0x04, 0x02, 0x08, 0x00, 0x00, 0x00, 0x00 }[daikin.mode]) + (daikin.power ? 0 : 0x10);  // FHCA456D mapped
               cmd[4] = daikin.powerful ? 0x03 : ((const uint8_t[])
                                                  { 0x01, 0x08, 0x04, 0x04, 0x02, 0x02, 0x09 }[daikin.fan]);    // A12345Q mapped
               cmd[5] = daikin.swingv ? 0x1F : 0x0A;
               cmd[6] = 0x10;   // ?
               daikin_cn_wired_command (sizeof (cmd), cmd);
            } else if (proto_type () == PROTO_TYPE_S21)
            {                   // Older S21
               char temp[5];
               if (debug)
                  s21debug = jo_object_alloc ();
               // Poll the AC status.
               // Each value has a smart NAK counter (see macro below), which allows
               // for autodetecting unsupported commands
#define poll(a,b,c,d)                         \
   static uint8_t a##b##d=2;                  \
   if(a##b##d){                               \
      int r=daikin_s21_command(*#a,*#b,c,#d); \
      if (r==S21_OK)                          \
         a##b##d=100;                         \
      else if(r==S21_NAK)                     \
         a##b##d--;                           \
   }                                          \
   if(!daikin.talking)                        \
      a##b##d=2;
               poll (F, 1, 0,);
               if (debug)
               {
                  poll (F, 2, 0,);
               }
               poll (F, 3, 0,);
               if (debug)
               {
                  poll (F, 4, 0,);
               }
               poll (F, 5, 0,);
               poll (F, 6, 0,);
               poll (F, 7, 0,);
               if (debug)
               {
                  poll (F, 8, 0,);
               }
               poll (F, 9, 0,);
               if (debug)
               {
                  poll (F, A, 0,);
                  poll (F, B, 0,);
                  poll (F, C, 0,);
                  poll (F, G, 0,);
                  poll (F, K, 0,);
                  poll (F, M, 0,);
                  poll (F, N, 0,);
                  poll (F, P, 0,);
                  poll (F, Q, 0,);
                  poll (F, S, 0,);
                  poll (F, T, 0,);
                  //poll (F, U, 2, 02);
                  //poll (F, U, 2, 04);
               }
               poll (R, H, 0,);
               poll (R, I, 0,);
               poll (R, a, 0,);
               poll (R, L, 0,); // Fan speed
               poll (R, d, 0,); // Compressor
               if (debug)
               {
                  poll (R, N, 0,);
                  poll (R, X, 0,);
                  poll (R, D, 0,);
               }
               if (RH == 100 && Ra == 100)
                  F9 = 0;       // Don't use F9
               if (*debugsend)
               {
                  b.dumping = 1;        // Force dumping
                  if (debugsend[1])
                     daikin_s21_command (debugsend[0], debugsend[1], strlen (debugsend + 2), debugsend + 2);
                  *debugsend = 0;
                  b.dumping = dump;     // Back to setting
               }
#undef poll
               if (debug)
                  revk_info ("s21", &s21debug);
               // Now send new values, requested by the user, if any
               if (daikin.control_changed & (CONTROL_power | CONTROL_mode | CONTROL_temp | CONTROL_fan))
               {                // D1
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = daikin.power ? '1' : '0';
                  temp[1] = ("64300002"[daikin.mode]);  // FHCA456D mapped to AXDCHXF
                  if (daikin.mode == 1 || daikin.mode == 2 || daikin.mode == 3)
                     temp[2] = s21_encode_target_temp (daikin.temp);
                  else
                     temp[2] = AC_MIN_TEMP_VALUE;       // No temp in other modes
                  temp[3] = ("A34567B"[daikin.fan]);
                  daikin_s21_command ('D', '1', S21_PAYLOAD_LEN, temp);
                  xSemaphoreGive (daikin.mutex);
               }
               if (daikin.control_changed & (CONTROL_swingh | CONTROL_swingv))
               {                // D5
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = '0' + (daikin.swingh ? 2 : 0) + (daikin.swingv ? 1 : 0) + (daikin.swingh && daikin.swingv ? 4 : 0);
                  temp[1] = (daikin.swingh || daikin.swingv ? '?' : '0');
                  temp[2] = '0';
                  temp[3] = '0';
                  daikin_s21_command ('D', '5', S21_PAYLOAD_LEN, temp);
                  xSemaphoreGive (daikin.mutex);
               }
               if (daikin.control_changed & (CONTROL_powerful | CONTROL_comfort | CONTROL_streamer |
                                             CONTROL_sensor | CONTROL_quiet | CONTROL_led))
               {                // D6
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  if (F3)
                  {             // F3 or F6 depends on model
                     temp[0] = '0';
                     temp[1] = '0';
                     temp[2] = '0';
                     temp[3] = '0' + (daikin.powerful ? 2 : 0);
                     daikin_s21_command ('D', '3', S21_PAYLOAD_LEN, temp);
                  }
                  if (F6)
                  {
                     temp[0] = '0' + (daikin.powerful ? 2 : 0) + (daikin.comfort ? 0x40 : 0) + (daikin.quiet ? 0x80 : 0);
                     temp[1] = '0' + (daikin.streamer ? 0x80 : 0);
                     temp[2] = '0';
                     // If sensor, the 8 is sensor, if not, then 4 and 8 are LED, with 4=high, 8=low, 12=off
                     if (noled || !nosensor)
                        temp[3] = '0' + (daikin.sensor ? 0x08 : 0) + (daikin.led ? 0x04 : 0);   // Messy but gives some controls
                     else
                        temp[3] = '0' + (daikin.led ? dark ? 8 : 4 : 12);
                     daikin_s21_command ('D', '6', S21_PAYLOAD_LEN, temp);
                  }
                  xSemaphoreGive (daikin.mutex);
               }
               if (daikin.control_changed & (CONTROL_demand | CONTROL_econo))
               {                // D7
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = '0' + 100 - daikin.demand;
                  temp[1] = '0' + (daikin.econo ? 2 : 0);
                  temp[2] = '0';
                  temp[3] = '0';
                  daikin_s21_command ('D', '7', S21_PAYLOAD_LEN, temp);
                  xSemaphoreGive (daikin.mutex);
               }
            } else if (proto_type () == PROTO_TYPE_X50A)
            {                   // Newer protocol
               //daikin_x50a_command(0xB7, 0, NULL);       // Not sure this is actually meaningful
               daikin_x50a_command (0xBD, 0, NULL);
               daikin_x50a_command (0xBE, 0, NULL);
               uint8_t ca[17] = { 0 };
               uint8_t cb[2] = { 0 };
               if (daikin.control_changed)
               {
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  ca[0] = 2 + daikin.power;
                  ca[1] = 0x10 + daikin.mode;
                  if (daikin.mode >= 1 && daikin.mode <= 3)
                  {             // Temp
                     int t = lroundf (daikin.temp * 10);
                     ca[3] = t / 10;
                     ca[4] = 0x80 + (t % 10);
                  } else
                     daikin.control_changed &= ~CONTROL_temp;
                  if (daikin.mode == 1 || daikin.mode == 2)
                     cb[0] = daikin.mode;
                  else
                     cb[0] = 6;
                  cb[1] = 0x80 + ((daikin.fan & 7) << 4);
                  xSemaphoreGive (daikin.mutex);
               }
               daikin_x50a_command (0xCA, sizeof (ca), ca);
               daikin_x50a_command (0xCB, sizeof (cb), cb);
            }
         }
         // Report status changes if happen on AC side. Ignore if we've just sent
         // some new control values
         if (!daikin.control_changed && (daikin.status_changed || daikin.status_report || daikin.mode_changed))
         {
            uint8_t send = ((debug || livestatus || daikin.status_report || daikin.mode_changed) ? 1 : 0);
            daikin.status_changed = 0;
            daikin.mode_changed = 0;
            daikin.status_report = 0;
            if (send)
            {
               jo_t j = daikin_status ();
               revk_state ("status", &j);
            }
            ha_status ();
         }
         // Stats
#define b(name)         if(daikin.name)daikin.total##name++;
#define t(name)		if(!isnan(daikin.name)){if(!daikin.count##name||daikin.min##name>daikin.name)daikin.min##name=daikin.name;	\
	 		if(!daikin.count##name||daikin.max##name<daikin.name)daikin.max##name=daikin.name;	\
	 		daikin.total##name+=daikin.name;daikin.count##name++;}
#define i(name)		if(!daikin.statscount||daikin.min##name>daikin.name)daikin.min##name=daikin.name;	\
	 		if(!daikin.statscount||daikin.max##name<daikin.name)daikin.max##name=daikin.name;	\
	 		daikin.total##name+=daikin.name;
#include "acextras.m"
         daikin.statscount++;
         if (!daikin.control_changed)
            daikin.control_count = 0;
         else if (daikin.control_count++ > 10)
         {                      // Tried a lot
            // Report failed settings
            jo_t j = jo_object_alloc ();
#define b(name)         if(daikin.control_changed&CONTROL_##name)jo_bool(j,#name,daikin.name);
#define t(name)         if(daikin.control_changed&CONTROL_##name){if(daikin.name>=100)jo_null(j,#name);else jo_litf(j,#name,"%.1f",daikin.name);}
#define i(name)         if(daikin.control_changed&CONTROL_##name)jo_int(j,#name,daikin.name);
#define e(name,values)  if((daikin.control_changed&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
#include "accontrols.m"
            revk_error ("failed-set", &j);
            daikin.control_changed = 0; // Give up on changes
            daikin.control_count = 0;
         }
         revk_blink (0, 0, b.loopback ? "RGB" : !daikin.online ? "M" : dark ? "" : !daikin.power ? "y" : daikin.mode == 0 ? "O" : daikin.mode == 7 ? "C" : daikin.heat ? "R" : "B");    // FHCA456D
         uint32_t now = uptime ();
         // Basic temp tracking
         xSemaphoreTake (daikin.mutex, portMAX_DELAY);
         uint8_t hot = daikin.heat;     // Are we in heating mode?
         float min = daikin.mintarget;
         float max = daikin.maxtarget;
         float current = daikin.env;
         if (isnan (current))   // We don't have one, so treat as same as A/C view of current temp
            current = daikin.home;
         xSemaphoreGive (daikin.mutex);
         // Predict temp changes
         if (tpredicts && !isnan (current))
         {
            static uint32_t lasttime = 0;
            if (now / tpredicts != lasttime / tpredicts)
            {                   // Every minute - predictive
               lasttime = now;
               daikin.envdelta2 = daikin.envdelta;
               daikin.envdelta = current - daikin.envlast;
               daikin.envlast = current;
            }
            if ((daikin.envdelta <= 0 && daikin.envdelta2 <= 0) || (daikin.envdelta >= 0 && daikin.envdelta2 >= 0))
               current += (daikin.envdelta + daikin.envdelta2) * tpredictt / (tpredicts * 2);   // Predict
         }
         // Apply adjustment
         if (daikin.control && daikin.power && !isnan (min) && !isnan (max))
         {
            if (hot)
            {
               max += (float) switchtemp / switchtemp_scale;    // Overshoot for switching (heating)
               min += (float) pushtemp / pushtemp_scale;        // Adjust target
            } else
            {
               min -= (float) switchtemp / switchtemp_scale;    // Overshoot for switching (cooling)
               max -= (float) pushtemp / pushtemp_scale;        // Adjust target
            }
         }
         void samplestart (void)
         {                      // Start sampling for fan/switch controls
            daikin.sample = 0;  // Start sample period
         }
         void controlstart (void)
         {                      // Start controlling
            if (daikin.control)
               return;
            daikin.hysteresis = 0;
            set_val (control, 1);
            samplestart ();
            if (!lockmode)
            {
               if (hot && current > max)
               {
                  hot = 0;
                  daikin_set_e (mode, "C");     // Set cooling as over temp
               } else if (!hot && current < min)
               {
                  hot = 1;
                  daikin_set_e (mode, "H");     // Set heating as under temp
               }
            }
            if (daikin.fan
                && ((hot && current < min - 2 * (float) switchtemp / switchtemp_scale)
                    || (!hot && current > max + 2 * (float) switchtemp / switchtemp_scale)))
            {                   // Not in auto mode, and not close to target temp - force a high fan to get there
               daikin.fansaved = daikin.fan;    // Save for when we get to temp
               daikin_set_v (fan, fmaxauto);    // Max fan at start
            }
         }
         void controlstop (void)
         {                      // Stop controlling
            if (!daikin.control)
               return;
            set_val (control, 0);
            if (daikin.fansaved)
            {                   // Restore saved fan setting
               daikin_set_v (fan, daikin.fansaved);
               daikin.fansaved = 0;
            }
            // We were controlling, so set to a non controlling mode, best guess at sane settings for now
            if (!isnan (daikin.mintarget) && !isnan (daikin.maxtarget))
               daikin_set_t (temp, daikin.heat ? daikin.maxtarget : daikin.mintarget);
            daikin.mintarget = NAN;
            daikin.maxtarget = NAN;
         }
         if ((auto0 || auto1) && (auto0 != auto1))
         {                      // Auto on/off, 00:00 is not considered valid, use 00:01. Also setting same on and off is not considered valid
            static int last = 0;
            time_t now = time (0);
            struct tm tm;
            localtime_r (&now, &tm);
            int hhmm = tm.tm_hour * 100 + tm.tm_min;
            if (auto0 && last < auto0 && hhmm >= auto0)
               daikin_set_v (power, 0); // Auto off, simple
            if (auto1 && last < auto1 && hhmm >= auto1)
            {                   // Auto on - and consider mode change is not on Auto
               daikin_set_v (power, 1);
               if (!lockmode && daikin.mode != 3 && !isnan (current) && !isnan (min) && !isnan (max)
                   && ((hot && current > max) || (!hot && current < min)))
                  daikin_set_e (mode, hot ? "C" : "H"); // Swap mode
            }
            last = hhmm;
         }
         if (!isnan (current) && !isnan (min) && !isnan (max) && tsample)
         {                      // Monitoring and automation
            if (daikin.power && daikin.lastheat != hot)
            {                   // If we change mode, start samples again
               daikin.lastheat = hot;
               samplestart ();
            }
            daikin.countt++;    // Total
            if ((hot && current < min) || (!hot && current > max))
               daikin.counta++; // Approaching temp
            else if ((hot && current > max) || (!hot && current < min))
               daikin.countb++; // Beyond
            if (!daikin.sample)
               daikin.counta = daikin.counta2 = daikin.countb = daikin.countb2 = daikin.countt = daikin.countt2 = 0;    // Reset sample counts
            if (daikin.sample <= now)
            {                   // New sample, consider some changes
               int t2 = daikin.countt2;
               int a = daikin.counta + daikin.counta2;  // Approaching
               int b = daikin.countb + daikin.countb2;  // Beyond
               int t = daikin.countt + daikin.countt2;  // Total (includes neither approaching or beyond, i.e. in range)
               jo_t j = jo_object_alloc ();
               jo_bool (j, "hot", hot);
               if (t)
               {
                  jo_int (j, "approaching", a);
                  jo_int (j, "beyond", b);
                  jo_int (j, t2 ? "samples" : "initial-samples", t);
               }
               jo_int (j, "period", tsample);
               jo_litf (j, "temp", "%.2f", current);
               jo_litf (j, "min", "%.2f", min);
               jo_litf (j, "max", "%.2f", max);
               if (t2)
               {                // Power, mode, fan, automation
                  if (daikin.power)
                  {
                     int step = (fanstep ? : (proto_type () == PROTO_TYPE_S21) ? 1 : 2);
                     if ((b * 2 > t || daikin.slave) && !a)
                     {          // Mode switch
                        if (!lockmode)
                        {
                           jo_string (j, "set-mode", hot ? "C" : "H");
                           daikin_set_e (mode, hot ? "C" : "H");        // Swap mode
                           if (step && daikin.fan > 1 && daikin.fan <= 5)
                           {
                              jo_int (j, "set-fan", 1);
                              daikin_set_v (fan, 1);
                           }
                        }
                     } else if (a * 10 < t * 7 && step && daikin.fan > 1 && daikin.fan <= 5)
                     {
                        jo_int (j, "set-fan", daikin.fan - step);
                        daikin_set_v (fan, daikin.fan - step);  // Reduce fan
                     } else if (!daikin.slave && a * 10 > t * 9 && step && daikin.fan >= 1 && daikin.fan < fmaxauto)
                     {
                        jo_int (j, "set-fan", daikin.fan + step);
                        daikin_set_v (fan, daikin.fan + step);  // Increase fan
                     } else if ((autop || (daikin.remote && autoptemp)) && !a && !b)
                     {          // Auto off
                        jo_bool (j, "set-power", 0);
                        daikin_set_v (power, 0);        // Turn off as 100% in band for last two period
                     }
                  } else
                     if ((autop || (daikin.remote && autoptemp))
                         && (daikin.counta == daikin.countt || daikin.countb == daikin.countt)
                         && (current >= max + (float) autoptemp / autoptemp_scale
                             || current <= min - (float) autoptemp / autoptemp_scale) && (!lockmode || b != t))
                  {             // Auto on (don't auto on if would reverse mode and lockmode)
                     jo_bool (j, "set-power", 1);
                     daikin_set_v (power, 1);   // Turn on as 100% out of band for last two period
                     if (b == t)
                     {
                        jo_string (j, "set-mode", hot ? "C" : "H");
                        daikin_set_e (mode, hot ? "C" : "H");   // Swap mode
                     }
                  }
               }
               if (t)
                  revk_info ("automation", &j);
               else
                  jo_free (&j);
               // Next sample
               daikin.counta2 = daikin.counta;
               daikin.countb2 = daikin.countb;
               daikin.countt2 = daikin.countt;
               daikin.counta = daikin.countb = daikin.countt = 0;
               daikin.sample = now + tsample;
            }
         }
         // Control
         if (daikin.controlvalid && now > daikin.controlvalid)
         {                      // End of auto mode and no env data either
            daikin.controlvalid = 0;
            daikin.status_known &= ~CONTROL_env;
            daikin.env = NAN;
            daikin.remote = 0;
            controlstop ();
         }
         if (daikin.power && daikin.controlvalid && !revk_shutting_down (NULL))
         {                      // Local auto controls
            // Get the settings atomically
            if (isnan (min) || isnan (max))
               controlstop ();
            else
            {                   // Control
               controlstart ();
               // What the A/C is using as current temperature
               float reference = NAN;
               if ((daikin.status_known & (CONTROL_home | CONTROL_inlet)) == (CONTROL_home | CONTROL_inlet))
                  reference = (daikin.home * thermref + daikin.inlet * (100 - thermref)) / 100; // thermref is how much inlet and home are used as reference
               else if (daikin.status_known & CONTROL_home)
                  reference = daikin.home;
               else if (daikin.status_known & CONTROL_inlet)
                  reference = daikin.inlet;
               // It looks like the ducted units are using inlet in some way, even when field settings say controller.
               if (daikin.mode == 3)
                  daikin_set_e (mode, hot ? "H" : "C"); // Out of auto
               // Temp set
               float set = min + reference - current;   // Where we will set the temperature
               if ((hot && current < (daikin.hysteresis ? max : min)) || (!hot && current > (daikin.hysteresis ? min : max)))
               {                // Apply heat/cool
                  if (thermostat)
                     daikin.hysteresis = 1;     // We're on, so keep going to "beyond"
                  if (hot)
                     set = max + reference - current + heatover;        // Ensure heating by applying A/C offset to force it
                  else
                     set = min + reference - current - coolover;        // Ensure cooling by applying A/C offset to force it
               } else
               {                // At or beyond temp - stop heat/cool
                  daikin.hysteresis = 0;        // We're off, so keep falling back until "approaching" (default when thermostat not set)
                  if (daikin.fansaved)
                  {
                     daikin_set_v (fan, daikin.fansaved);       // revert fan speed
                     daikin.fansaved = 0;
                     samplestart ();    // Initial phase complete, start samples again.
                  }
                  if (hot)
                     set = min + reference - current - heatback;        // Heating mode but apply negative offset to not actually heat any more than this
                  else
                     set = max + reference - current + coolback;        // Cooling mode but apply positive offset to not actually cool any more than this
               }
               // Limit settings to acceptable values
               if (proto_type () == PROTO_TYPE_CN_WIRED)
                  set = roundf (set);   // CN_WIRED only does 1C steps
               else if (proto_type () == PROTO_TYPE_S21)
                  set = roundf (set * 2.0) / 2.0;       // S21 only does 0.5C steps
               if (set < tmin)
                  set = tmin;
               if (set > tmax)
                  set = tmax;
               if (!isnan (reference))
                  daikin_set_t (temp, set);     // Apply temperature setting
            }
         } else
            controlstop ();
         if (reporting && !revk_link_down () && protocol_set)
         {                      // Environment logging
            time_t clock = time (0);
            static time_t last = 0;
            if (clock / reporting != last / reporting)
            {
               last = clock;
               if (daikin.statscount)
               {
                  jo_t j = jo_comms_alloc ();
                  {             // Timestamp
                     struct tm tm;
                     gmtime_r (&clock, &tm);
                     jo_stringf (j, "ts", "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900,
                                 tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                  }
#define	b(name)		if(daikin.status_known&CONTROL_##name){if(!daikin.total##name)jo_bool(j,#name,0);else if(!fixstatus&&daikin.total##name==daikin.statscount)jo_bool(j,#name,1);else jo_litf(j,#name,"%.2f",(float)daikin.total##name/daikin.statscount);} \
		  	daikin.total##name=0;
#define	t(name)		if(daikin.count##name&&!isnan(daikin.total##name)){if(!fixstatus&&daikin.min##name==daikin.max##name)jo_litf(j,#name,"%.2f",daikin.min##name);	\
		  	else {jo_array(j,#name);jo_litf(j,NULL,"%.2f",daikin.min##name);jo_litf(j,NULL,"%.2f",daikin.total##name/daikin.count##name);jo_litf(j,NULL,"%.2f",daikin.max##name);jo_close(j);}}	\
		  	daikin.min##name=NAN;daikin.total##name=0;daikin.max##name=NAN;daikin.count##name=0;
#define	r(name)		if(!isnan(daikin.min##name)&&!isnan(daikin.max##name)){if(!fixstatus&&daikin.min##name==daikin.max##name)jo_litf(j,#name,"%.2f",daikin.min##name);	\
			else {jo_array(j,#name);jo_litf(j,NULL,"%.2f",daikin.min##name);jo_litf(j,NULL,"%.2f",daikin.max##name);jo_close(j);}}
#define	i(name)		if(daikin.status_known&CONTROL_##name){if(!fixstatus&&daikin.min##name==daikin.max##name)jo_int(j,#name,daikin.total##name/daikin.statscount);     \
                        else {jo_array(j,#name);jo_int(j,NULL,daikin.min##name);jo_int(j,NULL,daikin.total##name/daikin.statscount);jo_int(j,NULL,daikin.max##name);jo_close(j);}       \
                        daikin.min##name=0;daikin.total##name=0;daikin.max##name=0;}
#define e(name,values)  if((daikin.status_known&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
#include "acextras.m"
                  revk_mqtt_send_clients ("Faikin", 0, NULL, &j, 1);
                  daikin.statscount = 0;
                  ha_status ();
               }
            }
         }
         if (daikin.ha_send && protocol_set)
         {
            send_ha_config ();
            ha_status ();       // Update status now sent
         }
      }
      while (daikin.talking);
      // We're here if protocol has been broken. We'll reconfigure the UART
      // and restart from scratch, possibly changing the protocol, if we're
      // in detection phase.
   }
}
