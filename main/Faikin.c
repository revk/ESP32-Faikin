/* Faikin app */
/* Copyright ©2022 Adrian Kennard, Andrews & Arnold Ltd. See LICENCE file for details .GPL 3.0 */

static const char TAG[] = "Faikin";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include "esp_http_server.h"
#include <math.h>
#include "mdns.h"
#include "ela.h"

#ifndef	CONFIG_HTTPD_WS_SUPPORT
#error Need CONFIG_HTTPD_WS_SUPPORT
#endif

#define	STX	2
#define	ETX	3
#define	ENQ	5
#define	ACK	6
#define	NAK	21

// Macros for setting values
#define	daikin_set_v(name,value)	daikin_set_value(#name,&daikin.name,CONTROL_##name,value)
#define	daikin_set_i(name,value)	daikin_set_int(#name,&daikin.name,CONTROL_##name,value)
#define	daikin_set_e(name,value)	daikin_set_enum(#name,&daikin.name,CONTROL_##name,value,CONTROL_##name##_VALUES)
#define	daikin_set_t(name,value)	daikin_set_temp(#name,&daikin.name,CONTROL_##name,value)

// Settings (RevK library used by MQTT setting command)
#define	settings		\
	u8(webcontrol,2)	\
	bl(debug)		\
	bl(dump)		\
	bl(livestatus)		\
	b(dark,false)		\
	b(ble,false)		\
	b(ha,true)		\
	u8(uart,1)		\
	u8l(thermref,50)	\
	u8l(autop10,5)		\
	u8l(coolover,6)		\
	u8l(coolback,6)		\
	u8l(heatover,6)		\
	u8l(heatback,6)		\
	u8l(switch10,5)		\
	u8l(push10,1)		\
	u16l(auto0,0)		\
	u16l(auto1,0)		\
	u16l(autot,0)		\
	u8l(autor,0)		\
	bl(autop)		\
	sl(autob)		\
	u8(tmin,16)		\
	u8(tmax,32)		\
	u32(tpredicts,30)	\
	u32(tpredictt,120)	\
	u32(tsample,900)	\
	u32(tcontrol,600)	\
	u8(fanstep,0)		\
	u32(reporting,60)	\
	io(tx,CONFIG_FAIKIN_TX)	\
	io(rx,CONFIG_FAIKIN_RX)	\

#define u32(n,d) uint32_t n;
#define s8(n,d) int8_t n;
#define u8(n,d) uint8_t n;
#define u8l(n,d) uint8_t n;
#define u16l(n,d) uint16_t n;
#define b(n,d) uint8_t n;
#define bl(n) uint8_t n;
#define s(n) char * n;
#define sl(n) char * n;
#define io(n,d)           uint8_t n;
settings
#undef io
#undef u32
#undef s8
#undef u8
#undef u8l
#undef u16l
#undef b
#undef bl
#undef s
#undef sl
#define PORT_INV 0x40
#define port_mask(p) ((p)&63)
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


// Globals
static httpd_handle_t webserver = NULL;
static uint8_t s21 = 0;         // Currently using S21 mode
static uint8_t protocol_set = 0;        // protocol confirmed
static uint8_t loopback = 0;    // Loopback detected
#ifdef ELA
static ela_t *bletemp = NULL;
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
   xSemaphoreTake (daikin.mutex, portMAX_DELAY);
   *ptr = value;
   daikin.control_changed |= flag;
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
   jo_bool (j, protocol_set ? "s21" : "s21-try", s21);
   return j;
}

jo_t s21debug = NULL;

void
daikin_s21_response (uint8_t cmd, uint8_t cmd2, int len, uint8_t * payload)
{
   if (len > 1 && s21debug)
   {
      char tag[3] = { cmd, cmd2 };
      jo_stringn (s21debug, tag, (char *) payload, len);
   }
   // Remember to add to polling if we add more handlers
   if (cmd == 'G' && len == 4)
      switch (cmd2)
      {
      case '1':
         set_val (online, 1);
         set_val (power, (payload[0] == '1') ? 1 : 0);
         set_val (mode, "30721003"[payload[1] & 0x7] - '0');    // FHCA456D mapped from AXDCHXF
         set_val (heat, daikin.mode == 1);      // Crude - TODO find if anything actually tells us this
         if (daikin.mode == 1 || daikin.mode == 2 || daikin.mode == 3)
            set_temp (temp, 18.0 + 0.5 * (signed) (payload[2] - '@'));
         else if (!isnan (daikin.temp))
            set_temp (temp, daikin.temp);       // Does not have temp in other modes
         if (payload[3] == 'A' && daikin.fan == 6)
            set_val (fan, 6);   // Quiet (returns as auto)
         else if (payload[3] == 'A')
            set_val (fan, 0);   // Auto
         else
            set_val (fan, "00012345"[payload[3] & 0x7] - '0');  // XXX12345 mapped to A12345Q
         break;
      case '5':
         set_val (swingv, (payload[0] & 1) ? 1 : 0);
         set_val (swingh, (payload[0] & 2) ? 1 : 0);
         break;
      case '6':
         set_val (powerful, payload[0] == '2' ? 1 : 0);
         break;
      case '7':
         set_val (econo, payload[1] == '2' ? 1 : 0);
         break;
         // Check 'G'
      }
   if (cmd == 'S' && len == 4)
   {
      float t = (payload[0] - '0') * 0.1 + (payload[1] - '0') + (payload[2] - '0') * 10;
      if (payload[3] == '-')
         t = -t;
      if (t < 100)              // Sanity check
         switch (cmd2)
         {                      // Temperatures
         case 'H':             // Guess
            set_temp (home, t);
            break;
         case 'a':             // Guess
            set_temp (outside, t);
            break;
         case 'I':             // Guess
            set_temp (liquid, t);
            break;
         case 'N':             // ?
            break;
         case 'X':             // ?
            break;
         }
   }
   if (cmd == 'S' && len == 3)
   {
      int v = (payload[0] - '0') + (payload[1] - '0') * 10 + (payload[2] - '0') * 100;
      switch (cmd2)
      {
      case 'L':                // Fan
         set_int (fanrpm, v * 10);
         break;
      }
   }
}

void
daikin_response (uint8_t cmd, int len, uint8_t * payload)
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

enum
{
   S21_OK,
   S21_NAK,
   S21_NOACK,
   S21_BAD,
   S21_WAIT,
};

int
daikin_s21_command (uint8_t cmd, uint8_t cmd2, int txlen, char *payload)
{
   if (debug && txlen > 2 && !dump)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%c%c", cmd, cmd2);
      if (txlen)
      {
         jo_base16 (j, "payload", payload, txlen);
         jo_stringn (j, "text", (char *) payload, txlen);
      }
      revk_info (daikin.talking ? "tx" : "cannot-tx", &j);
   }
   if (!daikin.talking)
      return S21_WAIT;          // Failed
   uint8_t buf[256],
     temp;
   buf[0] = STX;
   buf[1] = cmd;
   buf[2] = cmd2;
   if (txlen)
      memcpy (buf + 3, payload, txlen);
   uint8_t c = 0;
   for (int i = 1; i < 3 + txlen; i++)
      c += buf[i];
   if (c == ETX)
      c = ENQ;                  // Seems 03 sent as 05
   buf[3 + txlen] = c;
   buf[4 + txlen] = ETX;
   if (dump)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "dump", buf, txlen + 5);
      revk_info ("tx", &j);
   }
   uart_write_bytes (uart, buf, 5 + txlen);
   // Wait ACK
   int rxlen = uart_read_bytes (uart, &temp, 1, 100 / portTICK_PERIOD_MS);
   if (rxlen != 1 || temp != ACK)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%c%c", cmd, cmd2);
      if (txlen)
      {
         jo_base16 (j, "payload", payload, txlen);
         jo_stringn (j, "text", (char *) payload, txlen);
      }
      if (rxlen == 1 && temp == NAK)
      {
         if (debug)
         {
            jo_bool (j, "nak", 1);
            revk_error ("comms", &j);
         } else
            jo_free (&j);
         return S21_NAK;
      }
      daikin.talking = 0;
      jo_bool (j, "noack", 1);
      if (rxlen)
         jo_stringf (j, "value", "%02X", temp);
      revk_error ("comms", &j);
      return S21_NOACK;
   }
   if (cmd == 'D')
      return S21_OK;            // No response expected
   while (1)
   {
      rxlen = uart_read_bytes (uart, buf, 1, 100 / portTICK_PERIOD_MS);
      if (rxlen != 1)
      {
         daikin.talking = 0;
         jo_t j = jo_comms_alloc ();
         jo_bool (j, "timeout", 1);
         revk_error ("comms", &j);
         return S21_NOACK;
      }
      if (*buf == STX)
         break;
   }
   while (rxlen < sizeof (buf))
   {
      if (uart_read_bytes (uart, buf + rxlen, 1, 10 / portTICK_PERIOD_MS) != 1)
      {
         daikin.talking = 0;
         jo_t j = jo_comms_alloc ();
         jo_bool (j, "timeout", 1);
         revk_error ("comms", &j);
         return S21_NOACK;
      }
      rxlen++;
      if (buf[rxlen - 1] == ETX)
         break;
   }
   // Send ACK regardless, data is repeated, so will be sent again if we ignore due to checksum, for example.
   temp = ACK;
   uart_write_bytes (uart, &temp, 1);
   if (dump)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "dump", buf, rxlen);
      revk_info ("rx", &j);
   }
   // Check checksum
   c = 0;
   for (int i = 1; i < rxlen - 2; i++)
      c += buf[i];
   if (c != buf[rxlen - 2] && (c != ACK || buf[rxlen - 2] != ENQ))
   {                            // Sees checksum of 03 actually sends as 05
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "badsum", "%02X", c);
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      return S21_BAD;           // Ignore - it'll get resent some time
   }
   if (rxlen >= 5 && buf[0] == STX && buf[rxlen - 1] == ETX && buf[1] == cmd)
   {                            // Loop back
      daikin.talking = 0;
      loopback = 1;
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "loopback", 1);
      revk_error ("comms", &j);
      return S21_OK;
   }
   loopback = 0;
   if (buf[0] == STX)
      protocol_set = 1;         // Good format
   if (rxlen < 5 || buf[0] != STX || buf[rxlen - 1] != ETX || buf[1] != cmd + 1 || buf[2] != cmd2)
   {                            // Bad message
      daikin.talking = 0;       // Fail, restart comms
      jo_t j = jo_comms_alloc ();
      if (buf[0] != 2)
         jo_bool (j, "badhead", 1);
      if (buf[1] != cmd + 1 || buf[2] != cmd2)
         jo_bool (j, "mismatch", 1);
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      return S21_BAD;
   }
   daikin_s21_response (buf[1], buf[2], rxlen - 5, buf + 3);
   return S21_OK;
}

void
daikin_command (uint8_t cmd, int txlen, uint8_t * payload)
{                               // Send a command and get response
   if (debug && txlen)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%02X", cmd);
      jo_base16 (j, "payload", payload, txlen);
      revk_info (daikin.talking ? "tx" : "cannot-tx", &j);
   }
   if (!daikin.talking)
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
   if (dump)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "dump", buf, txlen + 6);
      revk_info ("tx", &j);
   }
   uart_write_bytes (uart, buf, 6 + txlen);
   // Wait for reply
   int rxlen = uart_read_bytes (uart, buf, sizeof (buf), 100 / portTICK_PERIOD_MS);
   if (rxlen <= 0)
   {
      daikin.talking = 0;
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "timeout", 1);
      revk_error ("comms", &j);
      return;
   }
   if (dump)
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
   if (buf[1] == 0xFF)
   {                            // Error report
      daikin.talking = 0;
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "fault", 1);
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      return;
   }
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
      loopback = 1;
      jo_t j = jo_comms_alloc ();
      jo_bool (j, "loopback", 1);
      revk_error ("comms", &j);
      return;
   }
   loopback = 0;
   if (buf[0] == 0x06)
      protocol_set = 1;         // Good message format
   daikin_response (cmd, rxlen - 6, buf + 5);
}

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
         jo_int (s, tag, lroundf (strtof (val, NULL) * 10.0));
      }
      if (!strcmp (tag, "autob"))
      {                         // Stored settings
         if (!s)
            s = jo_object_alloc ();
         if (!strcmp (val, "+"))
            jo_bool (s, "ble", 1);      // Enable BLE (reboots)
         else if (!strcmp (val, "-"))
         {
            jo_bool (s, "ble", 0);      // Disable BLE (reboots)
            jo_string (s, tag, "");
         } else
            jo_string (s, tag, val);    // Set BLE value
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
const char *
app_callback (int client, const char *prefix, const char *target, const char *suffix, jo_t j)
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
      if (!*autob)
         daikin.env = env;
      if (!autor && !*autob)
         daikin.remote = 1;     // Hides local automation settings
      daikin.status_known |= CONTROL_env;       // So we report it
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
         jo_lit (s, "temp", value);
      // HA stuff
      if (!strcmp (suffix, "mode"))
      {
         jo_bool (s, "power", *value == 'o' ? 0 : 1);
         if (*value != 'o')
            jo_stringf (s, "mode", "%c", toupper (*value));
      }
      if (!strcmp (suffix, "fan"))
         jo_stringf (s, "fan", "%c",
                     *value == 'l' ? '1' : *value == 'm' ? '3' : *value == 'h' ? '5' : *value == 'n' ? 'Q' : toupper (*value));
      if (!strcmp (suffix, "swing"))
      {
         jo_bool (s, "swingh", strchr (value, 'H') ? 1 : 0);
         jo_bool (s, "swingv", strchr (value, 'V') ? 1 : 0);
      }
      if (!strcmp (suffix, "preset"))
      {
         jo_bool (s, "econo", *value == 'e');
         jo_bool (s, "powerful", *value == 'b');
      }
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
   jo_t j = jo_object_alloc ();
#define b(name)         if(daikin.status_known&CONTROL_##name)jo_bool(j,#name,daikin.name);
#define t(name)         if(daikin.status_known&CONTROL_##name){if(isnan(daikin.name)||daikin.name>=100)jo_null(j,#name);else jo_litf(j,#name,"%.1f",daikin.name);}
#define i(name)         if(daikin.status_known&CONTROL_##name)jo_int(j,#name,daikin.name);
#define e(name,values)  if((daikin.status_known&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
#define s(name,len)     if((daikin.status_known&CONTROL_##name)&&*daikin.name)jo_string(j,#name,daikin.name);
#include "acextras.m"
#ifdef	ELA
   if (bletemp && !bletemp->missing)
      jo_string (j, "ble", bletemp->name);
   if (ble)
      jo_string (j, "autob", autob);
#endif
   if (daikin.remote)
      jo_bool (j, "remote", 1);
   else
   {
      jo_litf (j, "autor", "%.1f", autor / 10.0);
      jo_litf (j, "autot", "%.1f", autot / 10.0);
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
   httpd_resp_set_type (req, "text/html;charset=utf-8");
   httpd_resp_sendstr_chunk (req, "<meta name='viewport' content='width=device-width, initial-scale=1'>");
   httpd_resp_sendstr_chunk (req, "<html><head><title>");
   if (title)
      httpd_resp_sendstr_chunk (req, title);
   httpd_resp_sendstr_chunk (req, "</title></head><style>"      //
                             "body{font-family:sans-serif;background:#8cf;}"    //
                             ".on{opacity:1;transition:1s;}"    // 
                             ".off{opacity:0;transition:1s;}"   // 
                             ".switch,.box{position:relative;display:inline-block;min-width:64px;min-height:34px;margin:3px;}"  //
                             ".switch input,.box input{opacity:0;width:0;height:0;}"    //
                             ".slider,.button{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;-webkit-transition:.4s;transition:.4s;}"     //
                             ".slider:before{position:absolute;content:\"\";min-height:26px;min-width:26px;left:4px;bottom:3px;background-color:white;-webkit-transition:.4s;transition:.4s;}"  //
                             "input:checked+.slider,input:checked+.button{background-color:#12bd20;}"   //
                             "input:checked+.slider:before{-webkit-transform:translateX(30px);-ms-transform:translateX(30px);transform:translateX(30px);}"      //
                             "span.slider:before{border-radius:50%;}"   //
                             "span.slider,span.button{border-radius:34px;padding-top:8px;padding-left:10px;border:1px solid gray;box-shadow:3px 3px 3px #0008;}"        //
                             "select{min-height:34px;border-radius:34px;background-color:#ccc;border:1px solid gray;color:black;box-shadow:3px 3px 3px #0008;}" //
                             "input.temp{min-width:300px;}"     //
                             "input.time{min-height:34px;min-width:64px;border-radius:34px;background-color:#ccc;border:1px solid gray;color:black;box-shadow:3px 3px 3px #0008;}"      //
                             "</style><body><h1>");
   if (title)
      httpd_resp_sendstr_chunk (req, title);
   httpd_resp_sendstr_chunk (req, "</h1>");
}

static esp_err_t
web_foot (httpd_req_t * req)
{
   char temp[20];
   httpd_resp_sendstr_chunk (req, "<hr><address>");
   httpd_resp_sendstr_chunk (req, appname);
   httpd_resp_sendstr_chunk (req, ": ");
   httpd_resp_sendstr_chunk (req, revk_version);
   httpd_resp_sendstr_chunk (req, " ");
   httpd_resp_sendstr_chunk (req, revk_build_date (temp) ? : "?");
   httpd_resp_sendstr_chunk (req, "</address></body></html>");
   httpd_resp_sendstr_chunk (req, NULL);
   return ESP_OK;
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
   // TODO cookies
   // webcontrol=0 means no web
   // webcontrol=1 means user settings, not wifi settings
   // webcontrol=2 means all
   if (revk_link_down () && webcontrol >= 2)
      return revk_web_config (req);     // Direct to web set up
   web_head (req, hostname == revk_id ? appname : hostname);
   httpd_resp_sendstr_chunk (req, "<div id=top class=off><form name=F><table id=live>");
   void addh (const char *tag)
   {                            // Head (well, start of row)
      httpd_resp_sendstr_chunk (req, "<tr><td align=right>");
      httpd_resp_sendstr_chunk (req, tag);
      httpd_resp_sendstr_chunk (req, "</td>");
   }
   void addf (const char *tag)
   {                            // Foot (well, end of row)
      httpd_resp_sendstr_chunk (req, "<td colspan=2 id=");
      httpd_resp_sendstr_chunk (req, tag);
      httpd_resp_sendstr_chunk (req, "></td></tr>");
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
            httpd_resp_sendstr_chunk (req, "</tr><tr><td></td>");
            n = 0;
         }
         n++;
         const char *value = va_arg (ap, char *);
         httpd_resp_sendstr_chunk (req, "<td><label class=box><input type=radio name=");
         httpd_resp_sendstr_chunk (req, field);
         httpd_resp_sendstr_chunk (req, " value=");
         httpd_resp_sendstr_chunk (req, value);
         httpd_resp_sendstr_chunk (req, " id=");
         httpd_resp_sendstr_chunk (req, field);
         httpd_resp_sendstr_chunk (req, value);
         httpd_resp_sendstr_chunk (req, " onchange=\"if(this.checked)w('");
         httpd_resp_sendstr_chunk (req, field);
         httpd_resp_sendstr_chunk (req, "','");
         httpd_resp_sendstr_chunk (req, value);
         httpd_resp_sendstr_chunk (req, "');\"><span class=button>");
         httpd_resp_sendstr_chunk (req, tag);
         httpd_resp_sendstr_chunk (req, "</span></label></td>");
      }
      va_end (ap);
      addf (tag);
   }
   void addb (const char *tag, const char *field)
   {
      httpd_resp_sendstr_chunk (req, "<td align=right>");
      httpd_resp_sendstr_chunk (req, tag);
      httpd_resp_sendstr_chunk (req, "</td><td><label class=switch><input type=checkbox id=");
      httpd_resp_sendstr_chunk (req, field);
      httpd_resp_sendstr_chunk (req, " onchange=\"w('");
      httpd_resp_sendstr_chunk (req, field);
      httpd_resp_sendstr_chunk (req, "',this.checked);\"><span class=slider></span></label></td>");
   }
   void addhf (const char *tag)
   {
      addh (tag);
      addf (tag);
   }
   void addtemp (const char *tag, const char *field)
   {
      addh (tag);
      char temp[300];
      sprintf (temp,
               "<td colspan=5><input type=range class=temp min=%d max=%d step=%s id=%s onchange=\"w('%s',+this.value);\"><span id=T%s></span></td>",
               tmin, tmax, s21 ? "0.5" : "0.1", field, field, field);
      addf (tag);
   }
   void addtime (const char *tag, const char *field)
   {
      httpd_resp_sendstr_chunk (req, "<td align=right>");
      httpd_resp_sendstr_chunk (req, tag);
      httpd_resp_sendstr_chunk (req, "</td><td><input class=time type=time title=\"Set 00:00 to disable\" id=");
      httpd_resp_sendstr_chunk (req, field);
      httpd_resp_sendstr_chunk (req, " onchange=\"w('");
      httpd_resp_sendstr_chunk (req, field);
      httpd_resp_sendstr_chunk (req, "',this.value);\"></td>");
   }
   httpd_resp_sendstr_chunk (req, "<tr>");
   addb ("⏻", "power");
   httpd_resp_sendstr_chunk (req, "</tr>");
   add ("Mode", "mode", "Auto", "A", "Heat", "H", "Cool", "C", "Dry", "D", "Fan", "F", NULL);
   if (fanstep == 1 || (!fanstep && s21))
      add ("Fan", "fan", "1", "1", "2", "2", "3", "3", "4", "4", "5", "5", "Auto", "A", "(Night)", "Q", NULL);
   else
      add ("Fan", "fan", "Low", "1", "Mid", "3", "High", "5", NULL);
   addtemp ("Set", "temp");
   addhf ("Temp");
   addhf ("Coil");
   if (daikin.status_known & (CONTROL_econo | CONTROL_powerful))
   {
      httpd_resp_sendstr_chunk (req, "<tr>");
      if (daikin.status_known & CONTROL_econo)
         addb ("Eco", "econo");
      if (daikin.status_known & CONTROL_powerful)
         addb ("💪", "powerful");
      httpd_resp_sendstr_chunk (req, "</tr>");
   }
   if (daikin.status_known & (CONTROL_swingv | CONTROL_powerful))
   {
      httpd_resp_sendstr_chunk (req, "<tr>");
      if (daikin.status_known & CONTROL_swingv)
         addb ("↕", "swingv");
      if (daikin.status_known & CONTROL_swingh)
         addb ("↔", "swingh");
      httpd_resp_sendstr_chunk (req, "</tr>");
   }
   httpd_resp_sendstr_chunk (req, "</table>");
   httpd_resp_sendstr_chunk (req, "<p id=offline style='display:none'><b>System is off line.</b></p>");
   httpd_resp_sendstr_chunk (req, "<p id=shutdown style='display:none;color:red;'></p>");
   httpd_resp_sendstr_chunk (req,
                             "<p id=slave style='display:none'>❋ Another unit is controlling the mode, so this unit is not operating at present.</p>");
   httpd_resp_sendstr_chunk (req, "<p id=control style='display:none'>✷ Automatic control means some functions are limited.</p>");
   httpd_resp_sendstr_chunk (req,
                             "<p id=antifreeze style='display:none'>❄ System is in anti-freeze now, so cooling is suspended.</p>");
#ifdef ELA
   if (autor || *autob || !daikin.remote)
   {
      httpd_resp_sendstr_chunk (req, "<div id=remote><hr><p>Automated local controls</p><table>");
      add ("Auto", "autor", "Off", "0", "±½℃", "0.5", "±1℃", "1", "±2℃", "2", NULL);
      addtemp ("Target", "autot");
      httpd_resp_sendstr_chunk (req, "<tr>");
      addtime ("On", "auto1");
      addtime ("Off", "auto0");
      addb ("Auto", "autop");
      httpd_resp_sendstr_chunk (req, "<tr><td>BLE</td><td colspan=5>");
      httpd_resp_sendstr_chunk (req, "<select name=autob onchange=\"w('autob',this.options[this.selectedIndex].value);\">");
      if (!ble)
         httpd_resp_sendstr_chunk (req, "<option value=\"\">-- Disabled --");
      else if (!*autob)
         httpd_resp_sendstr_chunk (req, "<option value=\"\">-- None --");
      char found = 0;
      if (!ble)
         httpd_resp_sendstr_chunk (req, "<option value=+>-- Enable BLE --");
      else
         for (ela_t * e = ela; e; e = e->next)
         {
            httpd_resp_sendstr_chunk (req, "<option value=\"");
            httpd_resp_sendstr_chunk (req, e->name);
            httpd_resp_sendstr_chunk (req, "\"");
            if (*autob && !strcmp (autob, e->name))
            {
               httpd_resp_sendstr_chunk (req, " selected");
               found = 1;
            }
            httpd_resp_sendstr_chunk (req, ">");
            httpd_resp_sendstr_chunk (req, e->name);
            if (!e->missing && e->rssi)
            {
               char temp[20];
               snprintf (temp, sizeof (temp), " %ddB", e->rssi);
               httpd_resp_sendstr_chunk (req, temp);
            }
         }
      if (!found && *autob)
      {
         httpd_resp_sendstr_chunk (req, "<option selected value=\"");
         httpd_resp_sendstr_chunk (req, autob);
         httpd_resp_sendstr_chunk (req, "\">");
         httpd_resp_sendstr_chunk (req, autob);
      }
      if (ble)
         httpd_resp_sendstr_chunk (req, "<option value=->-- Disable BLE --");
      httpd_resp_sendstr_chunk (req, "</select>");
      if (ble && (uptime () < 60 || !found))
         httpd_resp_sendstr_chunk (req, " (reload to refresh list)");
      httpd_resp_sendstr_chunk (req, "</td></tr>");
      httpd_resp_sendstr_chunk (req, "</table><hr></div>");
   }
#endif
   httpd_resp_sendstr_chunk (req, "</form>");
   httpd_resp_sendstr_chunk (req, "</div>");
   if (webcontrol >= 2)
      httpd_resp_sendstr_chunk (req, "<p><a href='wifi'>Settings</a></p>");
   httpd_resp_sendstr_chunk (req, "<script>"    //
                             "var ws=0;"        //
                             "var reboot=0;"    //
                             "function g(n){return document.getElementById(n);};"       //
                             "function b(n,v){var d=g(n);if(d)d.checked=v;}"    //
                             "function h(n,v){var d=g(n);if(d)d.style.display=v?'block':'none';}"       //
                             "function s(n,v){var d=g(n);if(d)d.textContent=v;}"        //
                             "function n(n,v){var d=g(n);if(d)d.value=v;}"      //
                             "function e(n,v){var d=g(n+v);if(d)d.checked=true;}"       //
                             "function w(n,v){var m=new Object();m[n]=v;ws.send(JSON.stringify(m))}"    //
                             "function c(){"    //
                             "ws=new WebSocket('ws://'+window.location.host+'/status');"        //
                             "ws.onopen=function(v){g('top').className='on';};" //
                             "ws.onclose=function(v){ws=undefined;g('top').className='off';if(reboot)location.reload();};"      //
                             "ws.onerror=function(v){ws.close();};"     //
                             "ws.onmessage=function(v){"        //
                             "o=JSON.parse(v.data);"    //
                             "b('power',o.power);"      //
                             "h('offline',!o.online);"  //
                             "h('control',o.control);"  //
                             "h('slave',o.slave);"      //
                             "h('remote',!o.remote);"   //
                             "b('swingh',o.swingh);"    //
                             "b('swingv',o.swingv);"    //
                             "b('econo',o.econo);"      //
                             "e('mode',o.mode);"        //
                             "s('Temp',(o.home?o.home+'℃':'---')+(o.env?' / '+o.env+'℃':''));"      //
                             "n('temp',o.temp);"        //
                             "s('Ttemp',(o.temp?o.temp+'℃':'---')+(o.control?'✷':''));"     //
                             "b('autop',o.autop);"      //
                             "n('autot',o.autot);"      //
                             "e('autor',o.autor);"      //
                             "n('autob',o.autob);"      //
                             "n('auto0',o.auto0);"      //
                             "n('auto1',o.auto1);"      //
                             "s('Tautot',(o.autot?o.autot+'℃':''));"  //
                             "s('Coil',(o.liquid?o.liquid+'℃':'---'));"       //
                             "s('⏻',(o.slave?'❋':'')+(o.antifreeze?'❄':''));"     //
                             "s('Fan',(o.fanrpm?o.fanrpm+'RPM':'')+(o.antifreeze?'❄':'')+(o.control?'✷':''));"      //
                             "e('fan',o.fan);"  //
                             "if(o.shutdown){reboot=true;s('shutdown','Restarting: '+o.shutdown);h('shutdown',true);};" //
                             "};};c();" //
                             "setInterval(function() {if(!ws)c();else ws.send('');},1000);"     //
                             "</script>");
   return web_foot (req);
}

static esp_err_t
web_status (httpd_req_t * req)
{                               // Web socket status report
   // TODO cookies
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

static esp_err_t
web_get_control_info (httpd_req_t * req)
{
   httpd_resp_set_type (req, "text/plain");
   char resp[1000],
    *o = resp;
   o += sprintf (o, "ret=OK");
   o += sprintf (o, ",pow=%d", daikin.power);
   if (daikin.mode <= 7)
      o += sprintf (o, ",mode=%c", "64310002"[daikin.mode]);    // Mapped from FHCA456D
   o += sprintf (o, ",adv=%s", daikin.powerful ? "2" : "");
   o += sprintf (o, ",stemp=%.1f", daikin.temp);
   o += sprintf (o, ",shum=0");
   for (int i = 1; i <= 7; i++)
      if (i != 6)
         o += sprintf (o, ",dt%d=%.1f", i, daikin.temp);
   for (int i = 1; i <= 7; i++)
      if (i != 6)
         o += sprintf (o, ",dh%d=0", i);
   o += sprintf (o, "dhh=0");
   if (daikin.mode <= 7)
      o += sprintf (o, ",b_mode=%c", "64310002"[daikin.mode]);  // Mapped from FHCA456D
   o += sprintf (o, ",b_stemp=%.1f", daikin.temp);
   o += sprintf (o, ",b_shum=0");
   o += sprintf (o, ",alert=255");
   if (daikin.fan <= 6)
      o += sprintf (o, ",f_rate=%c", "A34567B"[daikin.fan]);
   o += sprintf (o, ",f_dir=%d", daikin.swingh * 2 + daikin.swingv);
   for (int i = 1; i <= 7; i++)
      if (i != 6)
         o += sprintf (o, ",dfr%d=0", i);
   o += sprintf (o, ",dfrh=0");
   for (int i = 1; i <= 7; i++)
      if (i != 6)
         o += sprintf (o, ",dfd%d=0", i);
   o += sprintf (o, ",dmdh=0");
   o += sprintf (o, ",dmnd_run=0");
   o += sprintf (o, ",en_demand=0");
   httpd_resp_sendstr (req, resp);
   return ESP_OK;
}

static esp_err_t
web_set_control_info (httpd_req_t * req)
{
   if (httpd_req_get_url_query_len (req))
   {
      char query[1000],
        value[10];
      if (!httpd_req_get_url_query_str (req, query, sizeof (query)))
      {                         // Assumes sane values sent mostly, and no error checking
         if (!httpd_query_key_value (query, "pow", value, sizeof (value)) && *value)
            daikin_set_v (power, *value == '1');
         if (!httpd_query_key_value (query, "mode", value, sizeof (value)) && *value && *value >= '1' && *value <= '7')
            daikin_set_v (mode, "03721003"[*value - '0']);
         if (!httpd_query_key_value (query, "stemp", value, sizeof (value)) && *value)
            daikin_set_t (temp, strtof (value, NULL));
         if (!httpd_query_key_value (query, "f_rate", value, sizeof (value)) && *value)
            daikin_set_v (fan, *value == 'A' ? 0 : *value == 'B' ? 6 : *value - '0');
      }
   }
   httpd_resp_set_type (req, "text/plain");
   httpd_resp_sendstr (req, "ret=OK,adv=");
   return ESP_OK;
}

static void
send_ha_config (void)
{
   daikin.ha_send = 0;
   char *topic;
   jo_t make (const char *tag)
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
      jo_string (j, "icon", "mdi:coolant-temperature");
      return j;
   }
   void addtemp (const char *tag)
   {
      if (asprintf (&topic, "homeassistant/sensor/%s%s/config", revk_id, tag) >= 0)
      {
         jo_t j = make (tag);
         jo_string (j, "name", tag);
         jo_string (j, "dev_cla", "temperature");
         jo_string (j, "stat_t", revk_id);
         jo_string (j, "unit_of_meas", "°C");
         jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
         revk_mqtt_send (NULL, 1, topic, &j);
         free (topic);
      }
   }
   if (asprintf (&topic, "homeassistant/climate/%s/config", revk_id) >= 0)
   {
      jo_t j = make ("");
      jo_string (j, "name", hostname);
      jo_stringf (j, "~", "command/%s", hostname);      // Prefix for command
#if 0                           // Cannot get this logic working
      if (daikin.status_known & CONTROL_online)
      {
         jo_object (j, "avty");
         jo_string (j, "t", revk_id);
         jo_string (j, "val_tpl", "{{value_json.online}}");
         jo_close (j);
      }
#endif
      jo_int (j, "min_temp", tmin);
      jo_int (j, "max_temp", tmax);
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
         if (fanstep == 1 || (!fanstep && s21))
         {
            jo_array (j, "fan_modes");
            jo_string (j, NULL, "auto");
            jo_string (j, NULL, "1");
            jo_string (j, NULL, "2");
            jo_string (j, NULL, "3");
            jo_string (j, NULL, "4");
            jo_string (j, NULL, "5");
            jo_string (j, NULL, "night");
            jo_close (j);
         }
      }
      if (daikin.status_known & (CONTROL_swingh | CONTROL_swingv))
      {
         jo_string (j, "swing_mode_cmd_t", "~/swing");
         jo_string (j, "swing_mode_stat_t", revk_id);
         jo_string (j, "swing_mode_stat_tpl", "{{value_json.swing}}");
         jo_array (j, "swing_modes");
         jo_string (j, NULL, "off");
         jo_string (j, NULL, "H");
         jo_string (j, NULL, "V");
         jo_string (j, NULL, "H+V");
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
   if ((daikin.status_known & CONTROL_home) && (daikin.status_known & CONTROL_inlet))
      addtemp ("inlet");        // Both defined so we used home as temp, so lets add inlet here
   if (daikin.status_known & CONTROL_outside)
      addtemp ("outside");
   if (daikin.status_known & CONTROL_liquid)
      addtemp ("liquid");
}

static void
ha_status (void)
{                               // Home assistant message
   if (!ha)
      return;
   jo_t j = jo_object_alloc ();
   if (daikin.status_known & CONTROL_online)
      jo_bool (j, "online", daikin.online);
   if (daikin.status_known & CONTROL_temp)
      jo_litf (j, "target", "%.2f", daikin.temp);
   if (daikin.status_known & CONTROL_home)
      jo_litf (j, "temp", "%.2f", daikin.home); // We use home if present, else inlet
   else if (daikin.status_known & CONTROL_inlet)
      jo_litf (j, "temp", "%.2f", daikin.inlet);
   if ((daikin.status_known & CONTROL_home) && (daikin.status_known & CONTROL_inlet))
      jo_litf (j, "inlet", "%.2f", daikin.inlet);       // Both so report inlet as well
   if (daikin.status_known & CONTROL_outside)
      jo_litf (j, "outside", "%.2f", daikin.outside);
   if (daikin.status_known & CONTROL_liquid)
      jo_litf (j, "liquid", "%.2f", daikin.liquid);
   if (daikin.status_known & CONTROL_mode)
   {
      const char *modes[] = { "fan_only", "heat", "cool", "auto", "4", "5", "6", "dry" };       // FHCA456D
      jo_string (j, "mode", daikin.power ? modes[daikin.mode] : "off");
   }
   if (daikin.status_known & CONTROL_fan)
   {
      if (fanstep == 1 || (!fanstep && s21))
      {
         const char *fans[] = { "auto", "1", "2", "3", "4", "5", "night" };     // A12345Q
         jo_string (j, "fan", fans[daikin.fan]);
      } else
      {
         const char *fans[] = { "auto", "low", "low", "medium", "high", "high", "auto" };       // A12345Q
         jo_string (j, "fan", fans[daikin.fan]);
      }
   }
   if (daikin.status_known & (CONTROL_swingh | CONTROL_swingv))
      jo_string (j, "swing", daikin.swingh & daikin.swingv ? "H+V" : daikin.swingh ? "H" : daikin.swingv ? "V" : "off");
   if (daikin.status_known & (CONTROL_econo | CONTROL_powerful))
      jo_string (j, "preset", daikin.econo ? "eco" : daikin.powerful ? "boost" : "home");       // Limited modes
   revk_mqtt_send_clients (NULL, 1, revk_id, &j, 1);
}

// --------------------------------------------------------------------------------
// Main
void
app_main ()
{
   daikin.mutex = xSemaphoreCreateMutex ();
   daikin.status_known = CONTROL_online;
#define	t(name)	daikin.name=NAN;
#define	r(name)	daikin.min##name=NAN;daikin.max##name=NAN;
#include "acextras.m"
   revk_boot (&app_callback);
#define str(x) #x
#define io(n,d)           revk_register(#n,0,sizeof(n),&n,"- "str(d),SETTING_SET|SETTING_BITFIELD);
#define b(n,d) revk_register(#n,0,sizeof(n),&n,str(d),SETTING_BOOLEAN);
#define bl(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN|SETTING_LIVE);
#define u32(n,d) revk_register(#n,0,sizeof(n),&n,str(d),0);
#define s8(n,d) revk_register(#n,0,sizeof(n),&n,str(d),SETTING_SIGNED);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,str(d),0);
#define u8l(n,d) revk_register(#n,0,sizeof(n),&n,str(d),SETTING_LIVE);
#define u16l(n,d) revk_register(#n,0,sizeof(n),&n,str(d),SETTING_LIVE);
#define s(n) revk_register(#n,0,0,&n,NULL,0);
#define sl(n) revk_register(#n,0,0,&n,NULL,SETTING_LIVE);
   settings
#undef io
#undef u32
#undef s8
#undef u8
#undef u8l
#undef u16l
#undef b
#undef bl
#undef s
#undef sl
      revk_start ();
   revk_blink (0, 0, "");
   void uart_setup (void)
   {
      esp_err_t err = 0;
      if (!protocol_set)
         s21 = 1 - s21;         // Flip
      ESP_LOGI (TAG, "Starting UART%s", s21 ? " S21" : "");
      uart_config_t uart_config = {
         .baud_rate = s21 ? 2400 : 9600,
         .data_bits = UART_DATA_8_BITS,
         .parity = UART_PARITY_EVEN,
         .stop_bits = s21 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
         .source_clk = UART_SCLK_DEFAULT,
      };
      if (!err)
         err = uart_param_config (uart, &uart_config);
      if (!err)
         err = uart_set_pin (uart, port_mask (tx), port_mask (rx), -1, -1);
      if (!err && ((tx & PORT_INV) || (rx & PORT_INV)))
         err =
            uart_set_line_inverse (uart, ((rx & PORT_INV) ? UART_SIGNAL_RXD_INV : 0) | ((tx & PORT_INV) ? UART_SIGNAL_TXD_INV : 0));
      if (!err)
         err = uart_driver_install (uart, 1024, 0, 0, NULL, 0);
      if (!err)
         err = uart_set_rx_full_threshold (uart, 1);
      if (err)
      {
         jo_t j = jo_object_alloc ();
         jo_string (j, "error", "Failed to uart");
         jo_int (j, "uart", uart);
         jo_int (j, "gpio", port_mask (rx));
         jo_string (j, "description", esp_err_to_name (err));
         revk_error ("uart", &j);
         return;
      }
   }

   // Web interface
   httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
   if (!httpd_start (&webserver, &config))
   {
      if (webcontrol)
      {
         {
            httpd_uri_t uri = {
               .uri = "/",
               .method = HTTP_GET,
               .handler = web_root,
            };
            REVK_ERR_CHECK (httpd_register_uri_handler (webserver, &uri));
         }
         {
            httpd_uri_t uri = {
               .uri = "/apple-touch-icon.png",
               .method = HTTP_GET,
               .handler = web_icon,
            };
            REVK_ERR_CHECK (httpd_register_uri_handler (webserver, &uri));
         }
         if (webcontrol >= 2)
         {
            httpd_uri_t uri = {
               .uri = "/wifi",
               .method = HTTP_GET,
               .handler = revk_web_config,
            };
            REVK_ERR_CHECK (httpd_register_uri_handler (webserver, &uri));
         }
         {
            httpd_uri_t uri = {
               .uri = "/status",
               .method = HTTP_GET,
               .handler = web_status,
               .is_websocket = true,
            };
            REVK_ERR_CHECK (httpd_register_uri_handler (webserver, &uri));
         }
         {
            httpd_uri_t uri = {
               .uri = "/aircon/get_control_info",
               .method = HTTP_GET,
               .handler = web_get_control_info,
            };
            REVK_ERR_CHECK (httpd_register_uri_handler (webserver, &uri));
         }
         {
            httpd_uri_t uri = {
               .uri = "/aircon/set_control_info",
               .method = HTTP_GET,
               .handler = web_set_control_info,
            };
            REVK_ERR_CHECK (httpd_register_uri_handler (webserver, &uri));
         }
      }
      revk_web_config_start (webserver);
   }

#ifdef	ELA
   if (ble)
      ela_run ();
   else
      esp_wifi_set_ps (WIFI_PS_NONE);
#endif

   if (!tx && !rx)
   {                            // Dummy
      daikin.status_known |= CONTROL_power | CONTROL_fan | CONTROL_temp | CONTROL_mode;
      daikin.power = 1;
      daikin.mode = 1;
      daikin.temp = 20.0;
   }

   while (1)
   {                            // Main loop
      daikin.talking = 1;
      if (tx || rx)
      {
         // Poke UART
         uart_setup ();
         sleep (1);
         uart_flush (uart);     // Clean start
         if (!s21)
         {                      // Startup
            daikin_command (0xAA, 1, (uint8_t[])
                            {
                            0x01}
            );
            daikin_command (0xBA, 0, NULL);
            daikin_command (0xBB, 0, NULL);
         }
         if (protocol_set && daikin.online != daikin.talking)
         {
            daikin.online = daikin.talking;
            daikin.status_changed = 1;
         }
      } else
         daikin.control_changed = 0;    // Dummy
      if (ha)
         daikin.ha_send = 1;
      do
      {                         // Polling loop
         usleep (1000000LL - (esp_timer_get_time () % 1000000LL));      /* wait for next second */
#ifdef ELA
         if (ble && *autob)
         {                      // Automatic external temperature logic - only really useful if autor/autot set
            ela_expire (60);
            if (!bletemp || strcmp (bletemp->name, autob))
            {
               bletemp = NULL;
               ela_clean ();
               for (ela_t * e = ela; e; e = e->next)
                  if (!strcmp (e->name, autob))
                  {
                     bletemp = e;
                     break;
                  }
            }
            if (bletemp && !bletemp->missing)
            {                   // Use temp
               daikin.env = bletemp->temp / 100.0;
               daikin.status_known |= CONTROL_env;      // So we report it
            } else
               daikin.status_known &= ~CONTROL_env;     // So we dont report it
         }
#endif
         if (autor && autot)
         {                      // Automatic setting of "external" controls, autot is temp(*10), autor is range(*10), autob is BLE name
            daikin.controlvalid = uptime () + 10;
            daikin.mintarget = (autot - autor) / 10.0;
            daikin.maxtarget = (autot + autor) / 10.0;
         }
         if (tx || rx)
         {
            if (s21)
            {                   // Older S21
               char temp[5];
               if (debug)
                  s21debug = jo_object_alloc ();
               // These are what their wifi polls
#define poll(a,b,c,d) static uint8_t a##b##d=10; if(a##b##d){int r=daikin_s21_command(*#a,*#b,c,#d); if(r==S21_OK)a##b##d=100; else if(r==S21_NAK)a##b##d--;} if(!daikin.talking)a##b##d=10;
               poll (F, 1, 0,);
               if (debug)
               {
                  poll (F, 2, 0,);
                  poll (F, 3, 0,);
                  poll (F, 4, 0,);
               }
               poll (F, 5, 0,);
               poll (F, 6, 0,);
               poll (F, 7, 0,);
               if (debug)
               {
                  poll (F, 8, 0,);
                  poll (F, 9, 0,);
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
               if (debug)
               {
                  poll (R, N, 0,);
                  poll (R, X, 0,);
                  poll (R, D, 0,);
               }
#undef poll
               if (debug)
                  revk_info ("s21", &s21debug);
               if (daikin.control_changed & (CONTROL_power | CONTROL_mode | CONTROL_temp | CONTROL_fan))
               {                // D1
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = daikin.power ? '1' : '0';
                  temp[1] = ("64300002"[daikin.mode]);  // FHCA456D mapped to AXDCHXF
                  if (daikin.mode == 1 || daikin.mode == 2 || daikin.mode == 3)
                     temp[2] = 0x40 + lroundf ((daikin.temp - 18.0) * 2);
                  else
                     temp[2] = '@';     // No temp in other modes
                  temp[3] = ("A34567B"[daikin.fan]);
                  daikin_s21_command ('D', '1', 4, temp);
                  xSemaphoreGive (daikin.mutex);
               }
               if (daikin.control_changed & (CONTROL_swingh | CONTROL_swingv))
               {                // D5
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = '0' + (daikin.swingh ? 2 : 0) + (daikin.swingv ? 1 : 0) + (daikin.swingh && daikin.swingv ? 4 : 0);
                  temp[1] = (daikin.swingh || daikin.swingv ? '?' : '0');
                  temp[2] = '0';
                  temp[3] = '0';
                  daikin_s21_command ('D', '5', 4, temp);
                  xSemaphoreGive (daikin.mutex);
               }
               if (daikin.control_changed & CONTROL_powerful)
               {                // D6
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = '0' + (daikin.powerful ? 2 : 0);
                  temp[1] = '0';
                  temp[2] = '0';
                  temp[3] = '0';
                  daikin_s21_command ('D', '6', 4, temp);
                  xSemaphoreGive (daikin.mutex);
               }
               if (daikin.control_changed & CONTROL_econo)
               {                // D7
                  xSemaphoreTake (daikin.mutex, portMAX_DELAY);
                  temp[0] = '0';
                  temp[1] = '0' + (daikin.econo ? 2 : 0);
                  temp[2] = '0';
                  temp[3] = '0';
                  daikin_s21_command ('D', '7', 4, temp);
                  xSemaphoreGive (daikin.mutex);
               }
            } else
            {                   // Newer protocol
               //daikin_command(0xB7, 0, NULL);       // Not sure this is actually meaningful
               daikin_command (0xBD, 0, NULL);
               daikin_command (0xBE, 0, NULL);
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
               daikin_command (0xCA, sizeof (ca), ca);
               daikin_command (0xCB, sizeof (cb), cb);
            }
         }
         if (!daikin.control_changed && (daikin.status_changed || daikin.status_report || daikin.mode_changed))
         {
            uint8_t send = ((debug || (livestatus && daikin.status_report) || daikin.mode_changed) ? 1 : 0);
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
         revk_blink (0, 0, loopback ? "RGB" : !daikin.online ? "M" : dark ? "" : !daikin.power ? "" : daikin.mode == 0 ? "O" : daikin.mode == 7 ? "C" : daikin.heat ? "R" : "B");       // FHCA456D
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
         // Apply hysteresis
         if (daikin.control && daikin.power && !isnan (min) && !isnan (max))
         {
            if (hot)
            {
               max += switch10 / 10.0;  // Overshoot for switching (heating)
               min += push10 / 10.0;    // Adjust target
            } else
            {
               min -= switch10 / 10.0;  // Overshoot for switching (cooling)
               max -= push10 / 10.0;    // Adjust target
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
            set_val (control, 1);
            samplestart ();
            if (hot && current > max)
            {
               hot = 0;
               daikin_set_e (mode, "C");        // Set cooling as over temp
            } else if (!hot && current < min)
            {
               hot = 1;
               daikin_set_e (mode, "H");        // Set heating as under temp
            }
            if (daikin.fan && ((hot && current < min - 2 * switch10 * 0.1) || (!hot && current > max + 2 * switch10 * 0.1)))
            {                   // Not in auto mode, and not close to target temp - force a high fan to get there
               daikin.fansaved = daikin.fan;    // Save for when we get to temp
               daikin_set_v (fan, 5);   // Max fan at start
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
         if (auto0 || auto1)
         {                      // Auto on/off
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
               if (daikin.mode != 3 && !isnan (current) && !isnan (min) && !isnan (max)
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
                     int step = (fanstep ? : s21 ? 1 : 2);
                     if ((b * 2 > t || daikin.slave) && !a)
                     {          // Mode switch
                        jo_string (j, "set-mode", hot ? "C" : "H");
                        daikin_set_e (mode, hot ? "C" : "H");   // Swap mode
                        if (step && daikin.fan > 1 && daikin.fan <= 5)
                        {
                           jo_int (j, "set-fan", 1);
                           daikin_set_v (fan, 1);
                        }
                     } else if (a * 10 < t * 7 && step && daikin.fan > 1 && daikin.fan <= 5)
                     {
                        jo_int (j, "set-fan", daikin.fan - step);
                        daikin_set_v (fan, daikin.fan - step);  // Reduce fan
                     } else if (!daikin.slave && a * 10 > t * 9 && step && daikin.fan >= 1 && daikin.fan < 5)
                     {
                        jo_int (j, "set-fan", daikin.fan + step);
                        daikin_set_v (fan, daikin.fan + step);  // Increase fan
                     } else if ((autop || (daikin.remote && autop10)) && !a && !b)
                     {          // Auto off
                        jo_bool (j, "set-power", 0);
                        daikin_set_v (power, 0);        // Turn off as 100% in band for last two period
                     }
                  } else if ((autop || (daikin.remote && autop10))
                             && (daikin.counta == daikin.countt || daikin.countb == daikin.countt)
                             && (current >= max + autop10 / 10.0 || current <= min - autop10 / 10.0))
                  {             // Auto on
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
         if (daikin.power && daikin.controlvalid && !revk_shutting_down (NULL))
         {                      // Local auto controls
            if (now > daikin.controlvalid)
            {                   // End of auto mode and no env data either
               daikin.controlvalid = 0;
               daikin.status_known &= ~CONTROL_env;
               daikin.env = NAN;
               daikin.remote = 0;
               controlstop ();
            } else
            {                   // Auto mode
               // TODO manual control override
               // Get the settings atomically
               if (isnan (min) || isnan (max))
                  controlstop ();
               else
               {                // Control
                  controlstart ();
                  // What the A/C is using as current temperature
                  float reference = NAN;
                  if ((daikin.status_known & (CONTROL_home | CONTROL_inlet)) == (CONTROL_home | CONTROL_inlet))
                     reference = (daikin.home * thermref + daikin.inlet * (100 - thermref)) / 100;      // thermref is how much inlet and home are used as reference
                  else if (daikin.status_known & CONTROL_home)
                     reference = daikin.home;
                  else if (daikin.status_known & CONTROL_inlet)
                     reference = daikin.inlet;
                  // It looks like the ducted units are using inlet in some way, even when field settings say controller.
                  if (daikin.mode == 3)
                     daikin_set_e (mode, hot ? "H" : "C");      // Out of auto
                  // Temp set
                  float set = min + reference - current;        // Where we will set the temperature
                  if ((hot && current < min) || (!hot && current > max))
                  {
                     if (hot)
                        set = max + reference - current + heatover;     // Ensure heating by applying A/C offset to force it
                     else
                        set = max + reference - current - coolover;     // Ensure cooling by applying A/C offset to force it
                  } else
                  {             // At or beyond temp
                     if (daikin.fansaved)
                     {
                        daikin_set_v (fan, daikin.fansaved);    // revert fan speed
                        daikin.fansaved = 0;
                        samplestart (); // Initial phase complete, start samples again.
                     }
                     if (hot)
                        set = min + reference - current - heatback;     // Heating mode but apply negative offset to not actually heat any more than this
                     else
                        set = max + reference - current + coolback;     // Cooling mode but apply positive offset to not actually cool any more than this
                  }
                  // Limit settings to acceptable values
                  if (s21)
                     set = roundf (set * 2.0) / 2.0;    // S21 only does 0.5C steps
                  if (set < tmin)
                     set = tmin;
                  if (set > tmax)
                     set = tmax;
                  if (!isnan (reference))
                     daikin_set_t (temp, set);  // Apply temperature setting
               }
            }
         } else
            controlstop ();
         if (reporting && !revk_link_down ())
         {                      // Environment logging
            time_t clock = time (0);
            static time_t last = 0;
            if (clock / reporting != last / reporting)
            {
               last = clock;
               if (daikin.statscount)
               {
                  jo_t j = jo_object_alloc ();
                  {             // Timestamp
                     struct tm tm;
                     gmtime_r (&clock, &tm);
                     jo_stringf (j, "ts", "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                 tm.tm_hour, tm.tm_min, tm.tm_sec);
                  }
#define	b(name)		if(daikin.status_known&CONTROL_##name){if(!daikin.total##name)jo_bool(j,#name,0);else if(daikin.total##name==daikin.statscount)jo_bool(j,#name,1);else jo_litf(j,#name,"%.2f",(float)daikin.total##name/daikin.statscount);} \
		  	daikin.total##name=0;
#define	t(name)		if(daikin.count##name&&!isnan(daikin.total##name)){if(daikin.min##name==daikin.max##name)jo_litf(j,#name,"%.2f",daikin.min##name);	\
		  	else {jo_array(j,#name);jo_litf(j,NULL,"%.2f",daikin.min##name);jo_litf(j,NULL,"%.2f",daikin.total##name/daikin.count##name);jo_litf(j,NULL,"%.2f",daikin.max##name);jo_close(j);}}	\
		  	daikin.min##name=NAN;daikin.total##name=0;daikin.max##name=NAN;daikin.count##name=0;
#define	r(name)		if(!isnan(daikin.min##name)&&!isnan(daikin.max##name)){if(daikin.min##name==daikin.max##name)jo_litf(j,#name,"%.2f",daikin.min##name);	\
			else {jo_array(j,#name);jo_litf(j,NULL,"%.2f",daikin.min##name);jo_litf(j,NULL,"%.2f",daikin.max##name);jo_close(j);}}
#define	i(name)		if(daikin.status_known&CONTROL_##name){if(daikin.min##name==daikin.max##name)jo_int(j,#name,daikin.total##name/daikin.statscount);     \
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
         if (daikin.ha_send)
         {
            send_ha_config ();
            ha_status ();       // Update status now sent
         }
      }
      while (daikin.talking);
      uart_driver_delete (uart);
   }
}
