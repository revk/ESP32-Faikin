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
#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "bleenv.h"
#endif
#include "cn_wired.h"
#include "cn_wired_driver.h"
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

typedef struct poll_s
{
   uint8_t ack:1;               //      We got an ACK so this is valid
   uint8_t nak:2;               //      Count of NAKs in a row - if too many we set bad
   uint8_t bad:1;               //      Too many NAKs, assume not supported
} poll_t;
struct
{                               // Status of S21 messages that get a valid response - this is a count of NAKs, so 0 means working...
   poll_t DH1000;
   poll_t F1;
   poll_t F2;
   poll_t F3;
   poll_t F4;
   poll_t F5;
   poll_t F6;
   poll_t F7;
   poll_t F8;
   poll_t F9;
   poll_t FA;
   poll_t FB;
   poll_t FC;
   poll_t FG;
   poll_t FK;
   poll_t FN;
   poll_t FM;
   poll_t FP;
   poll_t FQ;
   poll_t FS;
   poll_t FT;
   poll_t RD;
   poll_t RG;
   poll_t RI;
   poll_t RM;
   poll_t RL;
   poll_t RN;
   poll_t RH;
   poll_t RX;
   poll_t Ra;
   poll_t Rd;
   uint8_t rgfan:1;             // Use RG for fan
} s21 = { 0 };

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
   PROTO_TYPE_ALTHERMA_S,
   PROTO_TYPE_MAX
};
const char *const prototype[] = { "S21", "X50A", "CN_WIRED", "Altherma_S" };

struct FanMode
{
   char mode;
   const char *name;
};

const struct FanMode fans[] = {
   {'A', "auto"},
   {'1', "low"},
   {'2', "lowMedium"},
   {'3', "medium"},
   {'4', "mediumHigh"},
   {'5', "high"},
   {'Q', "night"},
   {0, NULL}
};

const struct FanMode cn_wired_fans[] = {
   {'A', "auto"},
   {'1', "low"},
   {0, "lowMedium"},            // Not used
   {'3', "medium"},
   {0, "mediumHigh"},           // Not used
   {'5', "high"},
   {'Q', "quiet"},
   {0, NULL}
};

#define	PROTO_TXINVERT	1
#define	PROTO_RXINVERT	2
#define	PROTO_SCALE	4

// Globals
struct
{
   uint8_t loopback:1;
   uint8_t dumping:1;
   uint8_t hourly:1;            // Hourly stuff
} b = { 0 };

static httpd_handle_t webserver = NULL;
static uint8_t protocol_set = 0;        // protocol confirmed
static uint8_t proto = 0;

static int
uart_enabled (void)
{
   return tx.set && rx.set;
}

static uint8_t
proto_type (void)
{
   return proto / PROTO_SCALE;
}

static int
invert_tx_line (void)
{
   return tx.invert ^ ((proto & PROTO_TXINVERT) ? 1 : 0);
}

static int
invert_rx_line (void)
{
   return rx.invert ^ ((proto & PROTO_RXINVERT) ? 1 : 0);
}

static const char *
proto_name (void)
{
   return uart_enabled ()? prototype[proto_type ()] : "MOCK";
}

static const struct FanMode *
get_fan_modes (void)
{
   return proto_type () == PROTO_TYPE_CN_WIRED ? cn_wired_fans : fans;
}

// Decode fan mode by name into a character (A12345Q)
static char
lookup_fan_mode (const char *name)
{
   const struct FanMode *f;

   for (f = get_fan_modes (); f->name; f++)
   {
      if (!strcmp (name, f->name))
      {
         if (f->mode)
            return f->mode;
         break;                 // The value isn't valid for this protocol
      }
   }

   return *name;                // Fallback
}

static const char *
get_temp_step (void)
{
   switch (proto_type ())
   {
   case PROTO_TYPE_CN_WIRED:
      return "1";
   case PROTO_TYPE_S21:
      return "0.5";
      // TODO(RevK): PROTO_TYPE_ALTHERMA_S
   default:                    // PROTO_TYPE_X50
      return "0.1";
   }
}

// 'fanstep' setting overrides number of available fan speeds
// 1 = force 5 speeds; 2 = force 3 speeds; 0 = default
// For experiments only !
static int
have_5_fan_speeds (void)
{
   return fanstep == 1 || (!fanstep && proto_type () == PROTO_TYPE_S21);
}

#ifdef ELA
static bleenv_t *bletemp = NULL;

static int
ble_sensor_connected (void)
{                               // We actually have BLE configured and connected and not missing
   return (ble && bletemp && !bletemp->missing) ? 1 : 0;
}

static int
ble_sensor_enabled (void)
{                               // We have BLE enabled and a BLE sensor defeined
   return (ble && *autob) ? 1 : 0;
}

#else // ELA

static int
ble_sensor_connected (void)
{                               // No BLE
   return 0;
}

static int
ble_sensor_enabled (void)
{                               // No BLE
   return 0;
}

#endif // ELA

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
   float env_prev;              // Predictive, last period value
   float env_delta;             // Predictive, diff to last
   float env_delta_prev;        // Predictive, previous diff
   uint32_t controlvalid;       // uptime to which auto mode is valid
   uint32_t sample;             // Last uptime sampled
   uint32_t countApproaching,
     countApproachingPrev;      // Count of "approaching temp", and previous sample
   uint32_t countBeyond,
     countBeyondPrev;           // Count of "beyond temp", and previous sample
   uint32_t countTotal,
     countTotalPrev;            // Count total, and previous sample
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
   uint8_t action:3;            // hvac_action
   uint8_t protocol_ver;        // Protocol version
} daikin = { 0 };

enum
{
   HVAC_OFF,
   HVAC_PREHEATING,
   HVAC_HEATING,
   HVAC_COOLING,
   HVAC_DRYING,
   HVAC_FAN,
   HVAC_IDLE,
   HVAC_DEFROSTING,
};
const char *const hvac_action[] = { "off", "preheating", "heating", "cooling", "drying", "fan", "idle", "defrosting" };

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
      daikin.ha_send = 1;
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
      daikin.ha_send = 1;
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
      daikin.ha_send = 1;
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

// These macros are used to report incoming status values from the AC
#define report_uint8(name,val) set_uint8(#name,&daikin.name,CONTROL_##name,val)
#define report_int(name,val) set_int(#name,&daikin.name,CONTROL_##name,val)
#define report_float(name,val) set_float(#name,&daikin.name,CONTROL_##name,val)
#define report_bool(name,val) report_uint8(name, (val ? 1 : 0))

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
   RES_OK,
   RES_NAK,
   RES_NOACK,
   RES_BAD,
   RES_WAIT,
   RES_TIMEOUT
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

static void
comm_timeout (uint8_t * buf, int rxlen)
{
   daikin.talking = 0;
   b.loopback = 0;
   jo_t j = jo_comms_alloc ();
   jo_bool (j, "timeout", 1);
   if (rxlen)
      jo_base16 (j, "data", buf, rxlen);
   revk_error ("comms", &j);
}

static void
comm_badcrc (uint8_t c, const uint8_t * buf, int rxlen)
{
   jo_t j = jo_comms_alloc ();
   jo_stringf (j, "badsum", "%02X", c);
   jo_base16 (j, "data", buf, rxlen);
   revk_error ("comms", &j);
}

// Decode S21 response payload
int
daikin_s21_response (uint8_t cmd, uint8_t cmd2, int len, uint8_t * payload)
{
   if (len >= 1 && s21debug)
   {
      char tag[3] = { cmd, cmd2 };
      if (debughex)
         jo_base16 (s21debug, tag, payload, len);
      else
         jo_stringn (s21debug, tag, (char *) payload, len);
   }
   // Remember to add to polling if we add more handlers
   if (cmd == 'G')
      switch (cmd2)
      {
      case '1':                // 'G1' - basic status
         if (check_length (cmd, cmd2, len, S21_PAYLOAD_LEN, payload))
         {
            report_uint8 (online, 1);
            report_bool (power, payload[0] == '1');
            report_uint8 (mode, "30721003"[payload[1] & 0x7] - '0');    // FHCA456D mapped from AXDCHXF
            report_uint8 (heat, daikin.mode == FAIKIN_MODE_HEAT);       // Crude - TODO find if anything actually tells us this
            if (daikin.mode == FAIKIN_MODE_HEAT || daikin.mode == FAIKIN_MODE_COOL || daikin.mode == FAIKIN_MODE_AUTO)
               report_float (temp, s21_decode_target_temp (payload[2]));
            else if (!isnan (daikin.temp))
               report_float (temp, daikin.temp);        // Does not have temp in other modes
            if (!s21.rgfan)
            {                   // RG is better, so we only look at G1 if RG does not work
               if (payload[3] != 'A')   // Set fan speed
                  report_uint8 (fan, "00012345"[payload[3] & 0x7] - '0');       // XXX12345 mapped to A12345Q
               else if (daikin.fan == 6)
                  report_uint8 (fan, 6);        // Quiet mode set (it returns as auto, so we assume it should be quiet if fan speed is low)
               else
                  report_uint8 (fan, 0);        // Auto as fan too fast to be quiet mode
            }
         }
         break;
      case '3':                // Seems to be an alternative to G6
         // If F6 is supported, F3 does not provide "powerful" flag even if supported.
         // We may still get G3 response for debug or from injection via MQTT "send".
         if (s21.F6.bad && check_length (cmd, cmd2, len, 1, payload))
         {
            report_bool (powerful, payload[3] & 0x02);
         }
         break;
      case '5':                // 'G5' - swing status
         if (check_length (cmd, cmd2, len, 1, payload))
         {
            if (!noswingw)
               report_bool (swingv, payload[0] & 1);
            if (!noswingh)
               report_bool (swingh, payload[0] & 2);
         }
         break;
      case '6':                // 'G6' - "powerful" mode and some others
         if (check_length (cmd, cmd2, len, S21_PAYLOAD_LEN, payload))
         {
            if (!nopowerful)
               report_bool (powerful, payload[0] & 0x02);
            if (!nocomfort)
               report_bool (comfort, payload[0] & 0x40);
            if (!noquiet)
               report_bool (quiet, payload[0] & 0x80);
            if (!nostreamer)
               report_bool (streamer, payload[1] & 0x80);
            if (!nosensor)
               report_bool (sensor, payload[3] & 0x08);
            if (!noled)
               report_bool (led, (payload[3] & 0x0C) != 0x0C);
         }
         break;
      case '7':                // 'G7' - "demand" and "eco" mode
         if (check_length (cmd, cmd2, len, 2, payload))
         {
            if (!nodemand && payload[0] != '1')
               report_int (demand, 100 - (payload[0] - '0'));
            report_bool (econo, payload[1] & 0x02);
         }
         break;
      case '8':
         if (check_length (cmd, cmd2, len, 2, payload))
         {
            daikin.protocol_ver = payload[1] & (~0x30);
         }
         break;
      case '9':
         if (check_length (cmd, cmd2, len, 2, payload))
         {
            report_float (home, (float) ((signed) payload[0] - 0x80) / 2);
            report_float (outside, (float) ((signed) payload[1] - 0x80) / 2);
         }
         break;
      case 'C':
         if (len > 0)
         {
            // Normally response length would be 4, but let's try being more creative
            // and future-proof. Accept the whole payload whatever it is.
            int limit = len >= sizeof (daikin.model) ? sizeof (daikin.model) - 1 : len;
            for (int i = 0; i < limit; i++)     // The string is provided in reverse
               daikin.model[i] = payload[len - i - 1];
            daikin.model[limit] = 0;
         }
         break;
      case 'M':                // Power meter
         report_int (Wh, s21_decode_hex_sensor (payload) * 100);        // 100Wh units
         break;
      }
   if (cmd == 'S')
   {
      if (cmd2 == 'G')
      {
         if (check_length (cmd, cmd2, len, 1, payload))
         {                      // One byte response!
            switch (cmd2)
            {
            case 'G':
               if (strchr ("34567AB", payload[0]))
               {                // Sensible FAN, else us F1
                  if (payload[0] >= '3' && payload[0] <= '7')
                     report_uint8 (fan, payload[0] - '3' + 1);  // 1-5
                  else if (payload[0] == 'A')
                     report_uint8 (fan, 0);     // Auto
                  else if (payload[0] == 'B')
                     report_uint8 (fan, 6);     // Quiet
                  s21.rgfan = 1;
               } else
                  s21.rgfan = 0;
               break;
            }
         }
      } else if (cmd2 == 'L' || cmd2 == 'd' || cmd2 == 'D' || cmd2 == 'N' || cmd2 == 'M')
      {                         // These responses are always only 3 bytes long
         if (check_length (cmd, cmd2, len, 3, payload))
         {
            int v = s21_decode_int_sensor (payload);
            switch (cmd2)
            {
            case 'L':          // Fan
               report_int (fanrpm, v * 10);
               break;
            case 'd':          // Compressor
               report_int (comp, v);
               break;
            case 'N':          // Angle vertical swing
               report_int (anglev, v);
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
               report_float (home, t);
               break;
            case 'a':          // 'Sa' - outside temp
               report_float (outside, t);
               break;
            case 'I':          // 'SI' - liquid ???
               report_float (liquid, t);
               break;
            case 'N':          // ?
               break;
            case 'X':          // ?
               break;
            }
         }
      }
   }
   return RES_OK;
}

void
protocol_found (void)
{
   protocol_set = 1;
   if (proto != protocol)
   {
      jo_t j = jo_object_alloc ();
      jo_int (j, "protocol", proto);
      revk_settings_store (j, NULL, 1);
      jo_free (&j);
   }
}

void
cn_wired_report_fan_speed (const uint8_t * packet)
{
   int8_t new_fan = cnw_decode_fan (packet);

   if (new_fan != FAIKIN_FAN_INVALID)
      report_uint8 (fan, new_fan);
   // Powerful is a dedicated flag\ for us, because this is how
   // other protocols handle it
   report_bool (powerful, packet[CNW_FAN_OFFSET] == CNW_FAN_POWERFUL);
}

// Parse an incoming CN_WIRED packet
// These packets always have a fixed length of CNW_PKT_LEN
void
daikin_cn_wired_incoming_packet (const uint8_t * payload)
{
   static int cnw_retries = 0;
   int8_t new_mode;
   jo_t j;

   uint8_t c = cnw_checksum (payload);

   if (c != payload[CNW_CRC_TYPE_OFFSET])
   {
      // Bad checksum
      comm_badcrc (c >> 4, payload, CNW_PKT_LEN);

      daikin.online = false;

      // When autodetecting a protocol, we only have 2 retries before deciding
      // that it's not CN_WIRED
      if (!protocol_set && ++cnw_retries == 2)
      {
         cnw_retries = 0;
         daikin.talking = 0;
      }
      return;
   }
   // We're now online
   report_uint8 (online, 1);

   if (!protocol_set)
   {
      // Protocol autodetection complete
      protocol_found ();

      // The only way for us to learn actual values is to receive a CNW_MODE_CHANGED
      // packet, which only happens if a remote control is used. So let's default
      // to some sane values. This also sets up what UI controls we see
      report_uint8 (power, 0);
      report_uint8 (mode, FAIKIN_MODE_AUTO);
      report_uint8 (heat, 0);
      report_float (temp, 20);
      report_uint8 (fan, FAIKIN_FAN_AUTO);
      report_uint8 (powerful, 0);
      report_uint8 (swingv, 0);
      if (!noled)
         report_uint8 (led, 0);
   }

   if (b.dumping)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "data", payload, CNW_PKT_LEN);
      cn_wired_stats (j);
      revk_info ("rx", &j);
   }

   switch (payload[CNW_CRC_TYPE_OFFSET] & CNW_TYPE_MASK)
   {
   case CNW_SENSOR_REPORT:
      report_float (home, decode_bcd (payload[CNW_TEMP_OFFSET]));
      break;
   case CNW_MODE_CHANGED:
      new_mode = cnw_decode_mode (payload);
      report_uint8 (power, !(payload[CNW_MODE_OFFSET] & CNW_MODE_POWEROFF));
      if (new_mode != FAIKIN_MODE_INVALID)
         report_uint8 (mode, new_mode);
      report_uint8 (heat, daikin.mode == FAIKIN_MODE_HEAT);
      report_float (temp, decode_bcd (payload[CNW_TEMP_OFFSET]));
      cn_wired_report_fan_speed (payload);
      report_bool (swingv, payload[CNW_SPECIALS_OFFSET] & CNW_V_SWING);
      if (!noled)
         report_bool (led, payload[CNW_SPECIALS_OFFSET] & CNW_LED_ON);
      break;
   default:
      // From testing with people we know there are also packets of other types.
      // Example of a type 2 packet: 0038000000000022
      // We currently don't know what they mean.
      j = jo_comms_alloc ();
      jo_string (j, "error", "Unknown message type");
      jo_base16 (j, "dump", payload, CNW_PKT_LEN);
      revk_error ("rx", &j);
      break;
   }
}

void
daikin_cn_wired_send_modes (void)
{
   int new_fan;
   uint8_t specials = 0;
   uint8_t buf[CNW_PKT_LEN];

   // These A/Cs from internal perspective have 6 fan speeds: Eco, Auto, 1, 2, 3, Powerful
   // For more advanced A/Cs Powerful is a special mode, which can be combined with fan speed settings,
   // so for us this setting is a separate on/off controls. And here are emulating this behavior
   // with the following algorithm:
   // - If the user enables Powerful, fan speed is remembered
   // - If the user disables Powerful, fan speed is reset to remembered value
   // - If the user selects fan speed, Powerful is turned off.
   // This conditional implements this exact logic. We check which control of the two
   // the user has frobbed, and act accordingly
   if (daikin.control_changed & CONTROL_fan)
   {
      // The user has touched fan speed control, set the speed and cancel Powerful
      new_fan = cnw_encode_fan (daikin.fan);
   } else
   {
      // The user has either touched Powerful control, or none of the two. Powerful
      // takes over.
      new_fan = daikin.powerful ? CNW_FAN_POWERFUL : cnw_encode_fan (daikin.fan);
   }

   // Experimental. Setting CNW_V_SWING bit in CNW_SPECIALS_OFFSET does not work;
   // the conditioner doesn't understand it.
   // Here we're replicating what Daichi controller does, with one little exception.
   // Daichi uses value of 0xF0 for CNW_SPECIALS_OFFSET, but from other users we know
   // that bit 7 stands for LED, so we change it to 0x70.
   // Could be that vertical swing flag actually sits in bit 0 of 6th byte; and Daichi got it wrong.
   if (daikin.swingv)
      specials |= 0x70;
   if (daikin.led)
      specials |= CNW_LED_ON;

   buf[CNW_TEMP_OFFSET] = encode_bcd (daikin.temp);
   buf[1] = 0x04;               // These two bytes are perhaps not even used, but from experiments
   buf[2] = 0x50;               // we know these packets work. So let's stick to known working values.
   buf[CNW_MODE_OFFSET] = cnw_encode_mode (daikin.mode, daikin.power);
   buf[CNW_FAN_OFFSET] = new_fan;
   buf[CNW_SPECIALS_OFFSET] = specials;
   buf[6] = daikin.swingv ? 0x11 : 0x10;
   buf[CNW_CRC_TYPE_OFFSET] = CNW_COMMAND;
   buf[CNW_CRC_TYPE_OFFSET] = cnw_checksum (buf);

   if (b.dumping)
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "data", buf, CNW_PKT_LEN);
      revk_info (daikin.talking ? "tx" : "cannot-tx", &j);
   }

   if (cn_wired_write_bytes (buf) == ESP_OK)
   {
      // Modes sent
      daikin.control_changed = 0;
      // This validates fan speed controls by parsing back value
      // from the packet we've just composed and sent. We're reusing
      // receiving code for simplicity. This implements the second part
      // of Powerful vs Fan speed mutual exclusion logic, described above.
      cn_wired_report_fan_speed (buf);
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
      report_uint8 (online, 1);
      report_uint8 (power, payload[0]);
      report_uint8 (mode, payload[1]);
      report_uint8 (heat, payload[2] == 1);
      report_uint8 (slave, payload[9]);
      report_uint8 (fan, (payload[6] >> 4) & 7);
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
         report_float (inlet, t);
      if ((t = (int16_t) (payload[2] + (payload[3] << 8)) / 128.0) && t < 100)
         report_float (home, t);
      if ((t = (int16_t) (payload[4] + (payload[5] << 8)) / 128.0) && t < 100)
         report_float (liquid, t);
      if ((t = (int16_t) (payload[8] + (payload[9] << 8)) / 128.0) && t < 100)
         report_float (temp, t);
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
      report_int (fanrpm, (payload[2] + (payload[3] << 8)));
      // Flag4 ?
      report_uint8 (flap, payload[5]);
      report_uint8 (antifreeze, payload[6]);
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

// Timeout value for serial port read
#define READ_TIMEOUT (500 / portTICK_PERIOD_MS)

void
daikin_as_response (int len, uint8_t * res)
{
   report_uint8 (online, 1);
   switch (*res)
   {
   case 'U':
      // TODO
      break;
   case 'T':
      // TODO
      break;
   case 'P':
      // TODO
      break;
   case 'S':
      // TODO
      break;
   }
}

int
daikin_as_command (int len, uint8_t * buf)
{
   uint8_t cs = 0;
   for (int i = 0; i < len; i++)
      cs += buf[i];
   buf[len++] = ~cs;
   if (b.dumping)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%c", buf[1]);
      jo_base16 (j, "dump", buf, len);
      revk_info ("tx", &j);
   }
   uart_write_bytes (uart, buf, len);
   uint8_t res[18];
   len = uart_read_bytes (uart, res, sizeof (res), READ_TIMEOUT);
   if (len < 0)
   {
      daikin.talking = 0;
      return RES_NOACK;
   }
   cs = 0;
   for (int i = 0; i < len - 1; i++)
      cs += res[i];
   cs = ~cs;
   if (b.dumping && len > 0)
   {
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "cmd", "%c", buf[1]);
      jo_base16 (j, "dump", res, len);
      revk_info ("rx", &j);
   }
   if (len != sizeof (res) || cs != res[len - 1] || *res != buf[1])
   {
      jo_t j = jo_comms_alloc ();
      jo_base16 (j, "payload", res, len);
      if (cs != res[len - 1])
         jo_stringf (j, "bad-cs", "%02X", cs);
      if (*res != 0x15 && *res != buf[1])
         jo_stringf (j, "bad-cmd", "%c", buf[1]);
      revk_error ("comms", &j);
      if (*res == 0x15 && cs == res[len - 1])
         return RES_NAK;
      return RES_BAD;
   }
   if (*res == buf[1] && !protocol_set)
      protocol_found ();
   daikin_as_response (len, res);
   return RES_OK;
}

int
daikin_as_poll (char reg)
{
   uint8_t temp[3];
   temp[0] = 0x02;
   temp[1] = reg;
   return daikin_as_command (2, temp);
}

static int
is_valid_s21_response (const uint8_t * buf, int rxlen, uint8_t cmd, uint8_t cmd2)
{
   return rxlen >= S21_MIN_PKT_LEN && buf[S21_STX_OFFSET] == STX && buf[rxlen - 1] == ETX &&
      buf[S21_CMD0_OFFSET] == cmd && buf[S21_CMD1_OFFSET] == cmd2;
}

static jo_t
jo_s21_alloc (char cmd, char cmd2, const char *payload, int payload_len)
{
   jo_t j = jo_comms_alloc ();
   jo_stringf (j, "cmd", "%c%c", cmd, cmd2);
   if (payload_len)
   {
      jo_base16 (j, "payload", payload, payload_len);
      jo_stringn (j, "text", payload, payload_len);
   }
   return j;
}

int
daikin_s21_command (uint8_t cmd, uint8_t cmd2, int payload_len, char *payload)
{
   if (debug && payload_len > 2 && !b.dumping)
   {
      jo_t j = jo_s21_alloc (cmd, cmd2, payload, payload_len);
      revk_info (daikin.talking || protofix ? "tx" : "cannot-tx", &j);
   }
   if (!daikin.talking && !protofix)
      return RES_WAIT;          // Failed
   uint8_t buf[256],
     temp;
   int txlen = S21_MIN_PKT_LEN + payload_len;
   if (!snoop)
   {                            // Send
      buf[S21_STX_OFFSET] = STX;
      buf[S21_CMD0_OFFSET] = cmd;
      buf[S21_CMD1_OFFSET] = cmd2;
      if (payload_len)
         memcpy (buf + S21_PAYLOAD_OFFSET, payload, payload_len);
      buf[S21_PAYLOAD_OFFSET + payload_len] = s21_checksum (buf, txlen);
      buf[S21_PAYLOAD_OFFSET + payload_len + 1] = ETX;
      if (b.dumping)
      {
         jo_t j = jo_comms_alloc ();
         jo_base16 (j, "dump", buf, txlen);
         char c[3] = { cmd, cmd2 };
         jo_stringn (j, c, payload, payload_len);
         revk_info ("tx", &j);
      }
      uart_write_bytes (uart, buf, txlen);
   }
   // Wait ACK. Apparently some models omit it.
   int rxlen = uart_read_bytes (uart, &temp, 1, READ_TIMEOUT);
   if (rxlen == 0)
   {
      comm_timeout (NULL, 0);
      return RES_TIMEOUT;
   }
   if (rxlen != 1 || (temp != ACK && temp != STX))
   {
      // Got something else
      if (rxlen == 1 && temp == NAK)
      {
         // Got an explicit NAK
         if (debug)
         {
            jo_t j = jo_s21_alloc (cmd, cmd2, payload, payload_len);
            jo_bool (j, "nak", 1);
            revk_error ("comms", &j);
         } else if (b.dumping)
         {
            // We want to see NAKs under info/<name>/rx because we could have sent
            // this command using command/<name>/send. We want to be informed if
            // the unit has NAKed it.
            jo_t j = jo_s21_alloc (cmd, cmd2, payload, payload_len);
            jo_bool (j, "nak", 1);
            revk_info ("rx", &j);
         }
         return RES_NAK;
      } else
      {
         // Unexpected reply, protocol broken
         jo_t j = jo_s21_alloc (cmd, cmd2, payload, payload_len);
         daikin.talking = 0;
         jo_bool (j, "noack", 1);
         jo_stringf (j, "value", "%02X", temp);
         revk_error ("comms", &j);
         return RES_NOACK;
      }
   }
   if (temp == STX)
      *buf = temp;              // No ACK, response started instead.
   else
   {
      if (cmd == 'D')
      {                         // No response expected
         if (b.dumping)
         {                      // We may be probing commands manually using command/<name>/send,
            // and we want to explicitly see ACKs
            jo_t j = jo_s21_alloc (cmd, cmd2, payload, payload_len);
            jo_bool (j, "ack", 1);
            revk_info ("rx", &j);
         }
         return RES_OK;
      }
      while (1)
      {
         rxlen = uart_read_bytes (uart, buf, 1, READ_TIMEOUT);
         if (rxlen != 1)
         {
            comm_timeout (NULL, 0);
            return RES_NOACK;
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
         comm_timeout (buf, txlen);
         return RES_NOACK;
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
   {                            // Report error and return RES_BAD - also pause/flush
      jo_base16 (j, "data", buf, rxlen);
      revk_error ("comms", &j);
      if (!protocol_set)
      {
         sleep (1);
         uart_flush (uart);
      }
      return RES_BAD;
   }
   // Check checksum
   uint8_t c = s21_checksum (buf, rxlen);
   if (c != buf[rxlen - 2])
   {                            // Sees checksum of 03 actually sends as 05
      jo_t j = jo_comms_alloc ();
      jo_stringf (j, "badsum", "%02X", c);
      return s21_bad (j);
   }
   // For reliability, verify that we've got back the exact transmitted data
   // We're using the same buf for both tx and rx, so our sent packet is gone
   // at this point, so we're verifying piece by piece
   if (!snoop && rxlen == txlen && is_valid_s21_response (buf, rxlen, cmd, cmd2) &&
       (payload_len == 0 || !memcmp (payload, buf + S21_PAYLOAD_OFFSET, payload_len)))
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
      return RES_OK;
   }
   b.loopback = 0;
   // If we've got an STX, S21 protocol is now confirmed; we won't change it any more
   if (buf[0] == STX && !protocol_set)
      protocol_found ();
   // An expected S21 reply contains the first character of the command
   // incremented by 1, the second character is left intact
   if (!snoop && !is_valid_s21_response (buf, rxlen, cmd + 1, cmd2))
   {                            // Malformed response, no proper S21
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
   buf[5 + txlen] = ~c;
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
      comm_timeout (NULL, 0);
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
      revk_settings_store (s, NULL, 1);
      jo_free (&s);
   }
   return "";
}

// --------------------------------------------------------------------------------
jo_t debugsend = NULL;

// Called by an MQTT client inside the revk library
const char *
mqtt_client_callback (int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{                               // MQTT app callback
   const char *ret = NULL;
   if (client || !prefix || target || strcmp (prefix, topiccommand))
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
      daikin.ha_send = 1;
   }
   if (!strcmp (suffix, "send"))
   {
      if (!j)
         return "Specify data to send";
      if (debugsend)
         return "Send pending";
      debugsend = jo_copy (j);
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
               if (!autor && !ble_sensor_enabled ())
                  daikin.remote = 1;    // Hides local automation settings
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
      if (!ble_sensor_connected ())
      {
         if (daikin.env != env)
            daikin.status_changed = 1;
         daikin.env = env;
         daikin.status_known |= CONTROL_env;    // So we report it
      }
      xSemaphoreGive (daikin.mutex);
      return ret ? : "";
   }
   // The following code converts the received MQTT message to our generic format,
   // then passes it to daikin_control()
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
            jo_litf (s, "autot", "%.1f", atof (value));
            revk_settings_store (s, NULL, 1);
            jo_free (&s);
         } else
            jo_lit (s, "temp", value);  // Direct controls
      }
      int checkbool (void)
      {
         return !strcasecmp (value, "ON") || !strcmp (value, "1") || !strcasecmp (value, "true") ? 1 : 0;
      }
      // The following processes commands from HA.
      // Topic suffixes according to auto-discovery we sent in send_ha_config()
      if (!strcmp (suffix, "mode"))
      {
         jo_bool (s, "power", strcmp (value, "off") ? 1 : 0);
         if (!strcmp (value, "heat_cool"))
            jo_string (s, "mode", "A");
         else if (*value != 'o')
            jo_stringf (s, "mode", "%c", toupper (*value));
      }
      if (!strcmp (suffix, "fan"))
      {
         char f = lookup_fan_mode (value);
         jo_stringf (s, "fan", "%c", f);
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
      if (!strcmp (suffix, "sensor"))
         jo_bool (s, "sensor", checkbool ());
      if (!strcmp (suffix, "econo"))
         jo_bool (s, "econo", checkbool ());
      if (!strcmp (suffix, "powerful"))
         jo_bool (s, "powerful", checkbool ());
      if (!strcmp (suffix, "quiet"))
         jo_bool (s, "quiet", checkbool ());
      if (!strcmp (suffix, "comfort"))
         jo_bool (s, "comfort", checkbool ());
      if (!strcmp (suffix, "power"))
         jo_bool (s, "power", checkbool ());
      if (!strcmp (suffix, "streamer"))
         jo_bool (s, "streamer", checkbool ());
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
   if (ble_sensor_connected ())
   {
      jo_object (j, "ble");
      if (bletemp->tempset)
         jo_litf (j, "temp", "%.2f", bletemp->temp / 100.0);
      if (bletemp->humset)
         jo_litf (j, "hum", "%.2f", bletemp->hum / 100.0);
      if (bletemp->batset)
         jo_int (j, "bat", bletemp->bat);
      if (bletemp->voltset)
         jo_litf (j, "volt", "%.3f", bletemp->volt / 1000.0);
      jo_close (j);
   }
   if (ble_sensor_enabled ())
      jo_string (j, "autob", autob);
#endif
   if (daikin.remote)
   {
      jo_bool (j, "remote", 1);
      if (!isnan (daikin.env))
         jo_litf (j, "env", "%.1f", daikin.env);
   } else
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
                  "a.pn{min-height:34px;min-width:34px;border-radius:30px;background-color:#ccc;border:1px solid gray;color:black;box-shadow:3px 3px 3px #0008;margin:3px;padding:3px 10px;font-size:100%%;}"   //
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
      if (noicons)
         revk_web_send (req, "<td align=right style='white-space:pre;vertical-align:middle;'>%s</td>", help);
      else
         revk_web_send (req, "<td title=\"%s\" align=right>%s</td>", help, tag);
      revk_web_send (req,
                     "<td title=\"%s\"><label class=switch><input type=checkbox id=\"%s\" onchange=\"w('%s',this.checked);\"><span class=slider></span></label></td>",
                     help, field, field);
   }
   void addslider (const char *tag, const char *field, int min, int max, const char *step)
   {
      addh (tag);
      revk_web_send (req,
                     "<td colspan=6><a onclick=\"if(+document.F.%s.value>%d)w('%s',+document.F.%s.value-%s);\" class=pn>-</a><input type=range class=temp min=%d max=%d step=%s id=%s onchange=\"w('%s',+this.value);\"><a onclick=\"if(+document.F.%s.value<%d)w('%s',+document.F.%s.value+%s);\" class=pn>+</a><button id=\"T%s\" onclick=\"return false;\"></button></td>",
                     field, min, field, field, step, min, max, step, field, field, field, max, field, field, step, field);
      addf (tag);
   }
   revk_web_send (req, "<tr>");
   addb ("⏼", "power", "Main\npower");
   revk_web_send (req, "</tr>");
   add ("Mode", "mode", "Auto", "A", "Heat", "H", "Cool", "C", "Dry", "D", "Fan", "F", NULL);
   if (have_5_fan_speeds ())
      add ("Fan", "fan", "1", "1", "2", "2", "3", "3", "4", "4", "5", "5", "Night", "Q", "Auto", "A", NULL);
   else if (proto_type () == PROTO_TYPE_CN_WIRED)
      add ("Fan", "fan", "Low", "1", "Mid", "3", "High", "5", "Auto", "A", "Quiet", "Q", NULL);
   else
      add ("Fan", "fan", "Low", "1", "Mid", "3", "High", "5", NULL);
   addslider ("Set", "temp", tmin, tmax, get_temp_step ());
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
   if ((daikin.status_known & CONTROL_env) && !ble_sensor_connected ())
      addt ("Env", "External reference temperature");
#ifdef ELA
   if (ble)
   {
      addt ("BLE", "External BLE temperature");
      addt ("Hum", "External BLE humidity");
   }
#endif
   revk_web_send (req, "</tr>");
   if (daikin.status_known & CONTROL_demand)
      addslider ("Demand", "demand", 30, 100, "5");
   if (daikin.status_known & (CONTROL_econo | CONTROL_powerful | CONTROL_led))
   {
      revk_web_send (req, "<tr>");
      if (daikin.status_known & CONTROL_econo)
         addb ("♻", "econo", "Econo\nmode");
      if (daikin.status_known & CONTROL_powerful)
         addb ("💪", "powerful", "Powerful\nmode");
      if (daikin.status_known & CONTROL_led)
         addb ("💡", "led", "LED\nhigh");
      revk_web_send (req, "</tr>");
   }
   if (daikin.status_known & (CONTROL_swingv | CONTROL_swingh | CONTROL_comfort))
   {
      revk_web_send (req, "<tr>");
      if (daikin.status_known & CONTROL_swingv)
         addb ("↕", "swingv", "Vertical\nSwing");
      if (daikin.status_known & CONTROL_swingh)
         addb ("↔", "swingh", "Horizontal\nSwing");
      if (daikin.status_known & CONTROL_comfort)
         addb ("🧸", "comfort", "Comfort\nmode");
      revk_web_send (req, "</tr>");
   }
   if (daikin.status_known & (CONTROL_streamer | CONTROL_sensor | CONTROL_quiet))
   {
      revk_web_send (req, "<tr>");
      if (daikin.status_known & CONTROL_streamer)
         addb ("🦠", "streamer", "Stream/\nfilter");
      if (daikin.status_known & CONTROL_sensor)
         addb ("🙆", "sensor", "Sensor\nmode");
      if (daikin.status_known & CONTROL_quiet)
         addb ("🤫", "quiet", "Quiet\noutdoor");
      revk_web_send (req, "</tr>");
   }
   revk_web_send (req, "</table>"       //
                  "<p id=offline style='display:none'><b>System is offline.</b></p>"    //
                  "<p id=loopback style='display:none'><b>System is in loopback test.</b></p>"  //
                  "<p id=shutdown style='display:none;color:red;'></p>" //
                  "<p id=slave style='display:none'>❋ Another unit is controlling the mode, so this unit is not operating at present.</p>"    //
                  "<p id=control style='display:none'>✷ Automatic control means some functions are limited.</p>"      //
                  "<p id=antifreeze style='display:none'>❄ System is in anti-freeze now, so cooling is suspended.</p>");

   if (autor || ble_sensor_enabled () || (!nofaikinauto && !daikin.remote))
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
      addslider ("Target", "autot", tmin, tmax, get_temp_step ());
      addnote ("Timed on and off (set other than 00:00)<br>Automated on/off if temp is way off target.");
      revk_web_send (req, "<tr>");
      addtime ("On", "auto1");
      addtime ("Off", "auto0");
      addb ("Auto ⏼", "autop", "Auto\non/off");
      revk_web_send (req, "</tr>");
#ifdef ELA
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
#endif
      revk_web_send (req, "</table></div>");
   }
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
                  "ws=new WebSocket((location.protocol=='https:'?'wss:':'ws:')+'//'+window.location.host+'/status');"   //
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
   jo_string (j, "ret", "OK");
   return j;
}

static char *
legacy_stringify (jo_t * jp)
{
   char *buf = NULL;

   if (jp && *jp)
   {
      jo_t j = *jp;
      int len = jo_len (j);

      buf = mallocspi (len + 40);
      if (buf)
      {
         char *p = buf;
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
      }
      jo_free (jp);
   }
   return buf;
}

static esp_err_t
legacy_send (httpd_req_t * req, jo_t * jp)
{
   char *buf;

   httpd_resp_set_type (req, "text/plain");
   buf = legacy_stringify (jp);
   if (buf)
   {
      httpd_resp_sendstr (req, buf);
      free (buf);
   }
   return ESP_OK;
}

static void
legacy_adv (jo_t j)
{
   jo_int (j, "adv",            //
           daikin.powerful ? 2 :        //
           daikin.econo ? 12 :  //
           daikin.streamer ? 13 :       //
           0);
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
      legacy_adv (j);
   }
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_set_holiday (httpd_req_t * req)
{
   const char *err = NULL;
   jo_t j = revk_web_query (req);
   if (!j)
      err = "Query failed";
   else
   {
      // TODO - ignore for now
      jo_free (&j);
   }
   return legacy_simple_response (req, err);
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

static void
jo_protocol_version (jo_t j)
{
   jo_int (j, "pv", daikin.protocol_ver);       //Conditioner protocol version
   jo_int (j, "cpv", 3);        // Controller protocol version 
   jo_string (j, "cpv_minor", "20");    //
}

static jo_t
legacy_get_basic_info (void)
{
   time_t now = time (0);
   struct tm tm;
   localtime_r (&now, &tm);
   jo_t j = legacy_ok ();
   jo_string (j, "type", "aircon");
   jo_string (j, "reg", region);
   jo_int (j, "dst", tm.tm_isdst);      // Guess
   jo_string (j, "ver", revk_version);
   jo_string (j, "rev", revk_version);
   jo_int (j, "pow", daikin.power);
   jo_int (j, "err", 1 - daikin.online);
   jo_int (j, "location", 0);
   jo_string (j, "name", hostname);
   jo_int (j, "icon", 1);
   jo_string (j, "method", "home only");        // "polling" for Daikin cloud
   jo_int (j, "port", 30050);   // Cloud port ?
   jo_string (j, "id", "");
   jo_string (j, "pw", "");
   jo_int (j, "lpw_flag", 0);
   jo_int (j, "adp_kind", 0);   // Controller HW type, for firmware update. We pretend to be GainSpan.
   jo_protocol_version (j);
   jo_int (j, "led", 1);        // Our LED is always on
   jo_int (j, "en_setzone", 0); // ??
   jo_string (j, "mac", revk_id);
   jo_string (j, "adp_mode", "run");    // Required for Daikin apps to see us
   jo_string (j, "ssid", "");   // SSID in AP mode
   jo_string (j, "ssid1", revk_wifi ());        // SSID in client mode
   jo_string (j, "grp_name", "");
   jo_int (j, "en_grp", 0);     //??
   return j;
}

static esp_err_t
legacy_web_get_basic_info (httpd_req_t * req)
{
   jo_t j = legacy_get_basic_info ();
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_get_model_info (httpd_req_t * req)
{
   int en_fdir = 0;
   int s_fdir = 0;
   int en_spmode = 0;

   if (daikin.status_known & CONTROL_swingh)
   {
      s_fdir |= (1 << 1);
      en_fdir = 1;
   }
   if (daikin.status_known & CONTROL_swingv)
   {
      s_fdir |= 1;
      en_fdir = 1;
   }
   if (daikin.status_known & CONTROL_streamer)
      en_spmode |= (1 << 2);
   if (daikin.status_known & CONTROL_econo)
      en_spmode |= (1 << 1);
   if (daikin.status_known & CONTROL_powerful)
      en_spmode |= 1;

   jo_t j = legacy_ok ();
   jo_string (j, "model", daikin.model);
   jo_protocol_version (j);
   jo_int (j, "en_frate", (daikin.status_known & CONTROL_fan) ? 1 : 0);
   jo_int (j, "en_fdir", en_fdir);
   jo_int (j, "s_fdir", s_fdir);
   jo_int (j, "en_spmode", en_spmode);
   return legacy_send (req, &j);
}

static esp_err_t
legacy_web_get_control_info (httpd_req_t * req)
{
   static float dt[8] = { 20, 20, 20, 20, 20, 20, 20, 20 };     // Used for some of the status
   static char dfr[8] = { 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A' };
   char mode = '0';
   if (daikin.mode <= 7)
      mode = "64370002"[daikin.mode];
   dfr[mode - '0'] = "A34567B"[daikin.fan];
   dt[mode - '0'] = daikin.temp;
   jo_t j = legacy_ok ();
   jo_int (j, "pow", daikin.power);
   jo_stringf (j, "mode", "%c", mode);
   legacy_adv (j);
   jo_litf (j, "stemp", "%.1f", daikin.temp);
   jo_int (j, "shum", 0);
   for (int i = 1; i <= 7; i++)
   {                            // Temp setting in mode
      char tag[4] = { 'd', 't', '0' + i };
      jo_litf (j, tag, "%.1f", dt[i]);
   }
   for (int i = 1; i <= 7; i++)
   {                            // Probably humidity, unknown
      char tag[4] = { 'd', 'h', '0' + i };
      jo_int (j, tag, 0);
   }
   jo_int (j, "dhh", 0);
   if (daikin.mode <= 7)
      jo_stringf (j, "b_mode", "%c", "64370002"[daikin.mode]);
   jo_litf (j, "b_stemp", "%.1f", daikin.temp);
   jo_int (j, "b_shum", 0);
   jo_int (j, "alert", 255);
   if (daikin.fan <= 6)
      jo_stringf (j, "f_rate", "%c", "A34567B"[daikin.fan]);
   jo_int (j, "f_dir", daikin.swingh * 2 + daikin.swingv);
   if (daikin.fan <= 6)
      jo_stringf (j, "b_f_rate", "%c", "A34567B"[daikin.fan]);
   jo_int (j, "b_f_dir", daikin.swingh * 2 + daikin.swingv);
   for (int i = 1; i <= 7; i++)
   {                            // Fan rate
      char tag[5] = { 'd', 'f', 'r', '0' + i };
      jo_stringf (j, tag, "%c", dfr[i]);
   }
   jo_int (j, "dfrh", 0);
   for (int i = 1; i <= 7; i++)
   {                            // Unknown
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
   jo_t j = legacy_ok ();
   jo_string (j, "curr_year_heat", "0/0/0/0/0/0/0/0/0/0/0/0");
   jo_string (j, "prev_year_heat", "0/0/0/0/0/0/0/0/0/0/0/0");
   jo_string (j, "curr_year_cool", "0/0/0/0/0/0/0/0/0/0/0/0");
   jo_string (j, "prevr_year_cool", "0/0/0/0/0/0/0/0/0/0/0/0");
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
   const char *err = NULL;
   jo_t j = revk_web_query (req);
   if (!j)
      err = "Query failed";
   else
   {
      int kind = 0,
         mode = 0;
      if (jo_find (j, "spmode_kind"))
      {
         char *v = jo_strdup (j);
         if (v)
            kind = atoi (v);
         free (v);
      }
      if (jo_find (j, "set_spmode"))
      {
         char *v = jo_strdup (j);
         if (v)
            mode = atoi (v);
         free (v);
      }
      if (jo_find (j, "en_streamer"))
      {
         char *v = jo_strdup (j);
         if (v)
            mode = atoi (v);
         free (v);
         kind = 3;
      }
      switch (kind)
      {
      case 1:                  // powerful
         err = daikin_set_v (powerful, mode);
         break;
      case 2:                  // eco
         err = daikin_set_v (econo, mode);
         break;
      case 3:                  // streamer
         err = daikin_set_v (streamer, mode);
         break;
      default:
         err = "Unknown kind";
      }
      jo_free (&j);
   }
   return legacy_simple_response (req, err);
}

// Daikin's original auto-discovery mechanism. Reverse engineered from
// Daikin online controller app.
static void
legacy_discovery_task (void *pvParameters)
{
   // This is request string
   static const char daikin_udp_req[] = "DAIKIN_UDP/common/basic_info";
   static const size_t daikin_udp_req_len = sizeof (daikin_udp_req) - 1;
   int sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_IP);
   if (sock >= 0)
   {
      int res = 1;
      setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &res, sizeof (res));
      {                         // Bind
         struct sockaddr_in dest_addr_ip4 = {.sin_addr.s_addr = htonl (INADDR_ANY),.sin_family = AF_INET,.sin_port = htons (30050)
         };
         res = bind (sock, (struct sockaddr *) &dest_addr_ip4, sizeof (dest_addr_ip4));
      }
      if (!res)
      {
         ESP_LOGI (TAG, "UDP discovery responder start");
         while (true)           // We don't stop
         {                      // Process
            jo_t basic_info;
            char *response;
            fd_set r;
            FD_ZERO (&r);
            FD_SET (sock, &r);
            struct timeval t = { 1, 0 };
            res = select (sock + 1, &r, NULL, NULL, &t);
            if (res < 0)
               break;
            if (!res)
               continue;
            uint8_t buf[daikin_udp_req_len];
            struct sockaddr_storage source_addr;
            socklen_t socklen = sizeof (source_addr);
            res = recvfrom (sock, buf, daikin_udp_req_len, 0, (struct sockaddr *) &source_addr, &socklen);
            if (res < daikin_udp_req_len)
               continue;        // Too short
            if (memcmp (buf, daikin_udp_req, daikin_udp_req_len))
               continue;        // Wrong data
            // Reply is the same as /common/get_basic_info
            basic_info = legacy_get_basic_info ();
            response = legacy_stringify (&basic_info);
            if (response)
            {
               ((struct sockaddr_in *) &source_addr)->sin_port = htons (30000);
               sendto (sock, response, strlen (response), 0, (struct sockaddr *) &source_addr, socklen);
               free (response);
            }
            ESP_LOGI (TAG, "UDP discovery reply (stack free %d)", uxTaskGetStackHighWaterMark (NULL));
         }
         ESP_LOGI (TAG, "UDP discovery stop");
      } else
         ESP_LOGE (TAG, "UDP discovery could not bind");
      close (sock);
   } else
      ESP_LOGE (TAG, "UDP discovery no socket");
   vTaskDelete (NULL);
}

static void
addmodes (jo_t j, const struct FanMode *modes)
{
   const struct FanMode *f;

   jo_array (j, "fan_modes");
   for (f = modes; f->name; f++)
   {
      // Only list modes, which are valid for current protocol
      if (f->mode)
         jo_string (j, NULL, f->name);
   }
   jo_close (j);
}

// Compose and send HomeAssistant MQTT auto-discovery message
// According to https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
static void
send_ha_config (void)
{
   daikin.ha_send = 0;
   if (!haenable)
      return;
   char *hastatus = revk_topic (topicstate, NULL, NULL);
   char *cmd = revk_topic (topiccommand, NULL, NULL);
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
      jo_stringf (j, "cu", "http://%s.%s/", hostname, hadomain);
      jo_close (j);
      if (icon)
         jo_string (j, "icon", icon);
      jo_string (j, "avty_t", hastatus);
      jo_string (j, "avty_tpl", "{{value_json.up}}");
      jo_bool (j, "pl_avail", 1);
      jo_bool (j, "pl_not_avail", 0);
      return j;
   }
   void addtemp (uint64_t ok, const char *tag, const char *name, const char *icon)
   {
      if (asprintf (&topic, "%s/sensor/%s%s/config", topicha, revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", name);
            jo_string (j, "dev_cla", "temperature");
            jo_string (j, "state_class", "measurement");
            jo_string (j, "stat_t", hastatus);
            jo_string (j, "unit_of_meas", "°C");
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   void addfreq (uint64_t ok, const char *tag, const char *name, const char *unit, const char *icon)
   {
      if (asprintf (&topic, "%s/sensor/%s%s/config", topicha, revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", tag);
            jo_string (j, "dev_cla", "frequency");
            jo_string (j, "state_class", "measurement");
            jo_string (j, "stat_t", hastatus);
            jo_string (j, "unit_of_meas", unit);
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   void addswitch (uint64_t ok, const char *tag, const char *name, const char *icon)
   {
      if (asprintf (&topic, "%s/switch/%s%s/config", topicha, revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", name);
            jo_string (j, "stat_t", hastatus);
            jo_stringf (j, "cmd_t", "%s/%s", cmd, tag);
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            jo_bool (j, "pl_on", 1);
            jo_bool (j, "pl_off", 0);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   if (asprintf (&topic, "%s/climate/%s/config", topicha, revk_id) >= 0)
   {
      jo_t j = make ("", "mdi:thermostat");
      //jo_string (j, "name", hostname);
      //jo_null(j,"name");
      jo_string (j, "~", cmd);
      jo_int (j, "min_temp", tmin);
      jo_int (j, "max_temp", tmax);
      jo_string (j, "temp_unit", "C");
      jo_lit (j, "temp_step", ha1c ? "1" : get_temp_step ());
      jo_string (j, "temp_cmd_t", "~/temp");
      jo_string (j, "temp_stat_t", hastatus);
      jo_string (j, "temp_stat_tpl", "{{value_json.target}}");
      if (daikin.status_known & (CONTROL_inlet | CONTROL_home))
      {
         jo_string (j, "curr_temp_t", hastatus);
         jo_string (j, "curr_temp_tpl", "{{value_json.temp}}");
      }
      if (daikin.status_known & CONTROL_mode)
      {
         jo_string (j, "mode_cmd_t", "~/mode");
         jo_string (j, "mode_stat_t", hastatus);
         jo_string (j, "mode_stat_tpl", "{{value_json.mode}}");
         if (!nohvacaction)
         {
            jo_string (j, "action_topic", hastatus);
            jo_string (j, "action_template", "{{value_json.action}}");
         }
         jo_string (j, "payload_on", "1");
         jo_string (j, "payload_off", "0");
         jo_string (j, "power_command_topic", "~/power");
         jo_array (j, "modes");
         jo_string (j, NULL, "heat_cool");
         jo_string (j, NULL, "off");
         jo_string (j, NULL, "cool");
         jo_string (j, NULL, "heat");
         jo_string (j, NULL, "dry");
         jo_string (j, NULL, "fan_only");
         jo_close (j);
      }
      if (daikin.status_known & CONTROL_fan)
      {
         jo_string (j, "fan_mode_cmd_t", "~/fan");
         jo_string (j, "fan_mode_stat_t", hastatus);
         jo_string (j, "fan_mode_stat_tpl", "{{value_json.fan}}");
         if (have_5_fan_speeds ())
         {
            addmodes (j, fans);
         } else if (proto_type () == PROTO_TYPE_CN_WIRED)
         {
            addmodes (j, cn_wired_fans);
         }
         // [“auto”, “low”, “medium”, “high”] is the default, no need to report
      }
      if (daikin.status_known & (CONTROL_swingh | CONTROL_swingv | CONTROL_comfort))
      {
         jo_string (j, "swing_mode_cmd_t", "~/swing");
         jo_string (j, "swing_mode_stat_t", hastatus);
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
         jo_string (j, "pr_mode_stat_t", hastatus);
         jo_string (j, "pr_mode_val_tpl", "{{value_json.preset}}");
         jo_array (j, "pr_modes");
         if (daikin.status_known & CONTROL_econo)
            jo_string (j, NULL, "eco");
         if (daikin.status_known & CONTROL_powerful)
            jo_string (j, NULL, "boost");
         if (!nohomepreset)
            jo_string (j, NULL, "home");
         jo_close (j);
      }
      revk_mqtt_send (NULL, 1, topic, &j);
      free (topic);
   }
   addtemp ((daikin.status_known & CONTROL_home) && (daikin.status_known & CONTROL_inlet), "inlet", "Inlet", "mdi:thermometer");        // Both defined so we used home as temp, so lets add inlet here
   addtemp (daikin.status_known & CONTROL_outside, "outside", "Outside", "mdi:thermometer");
   addtemp (daikin.status_known & CONTROL_liquid, "liquid", "Liquid", "mdi:coolant-temperature");
   addfreq (daikin.status_known & CONTROL_comp, "comp", "Compressor", hacomprpm ? "rpm" : "Hz", "mdi:sine-wave");
   addfreq (daikin.status_known & CONTROL_fanrpm, "fanfreq", "Fan", hafanrpm ? "rpm" : "Hz", "mdi:fan");
   addswitch (haswitches && (daikin.status_known & CONTROL_power), "power", "Power", "mdi:power");
   addswitch (haswitches && (daikin.status_known & CONTROL_streamer), "streamer", "Streamer", "mdi:air-filter");
   addswitch (haswitches && (daikin.status_known & CONTROL_sensor), "sensor", "Sensor mode", "mdi:motion-sensor");
   addswitch (haswitches && (daikin.status_known & CONTROL_powerful), "powerful", "Powerful", "mdi:arm-flex");
   addswitch (haswitches && (daikin.status_known & CONTROL_comfort), "comfort", "Comfort mode", "mdi:teddy-bear");
   addswitch (haswitches && (daikin.status_known & CONTROL_quiet), "quiet", "Quiet outdoor", "mdi:volume-minus");
   addswitch (haswitches && (daikin.status_known & CONTROL_econo), "econo", "Econo mode", "mdi:home-battery");
#ifdef ELA
   void addhum (uint64_t ok, const char *tag, const char *name, const char *icon)
   {
      if (asprintf (&topic, "%s/sensor/%s%s/config", topicha, revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", name);
            jo_string (j, "dev_cla", "humidity");
            jo_string (j, "state_class", "measurement");
            jo_string (j, "stat_t", hastatus);
            jo_string (j, "unit_of_meas", "%");
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   void addbat (uint64_t ok, const char *tag, const char *name, const char *icon)
   {
      if (asprintf (&topic, "%s/sensor/%s%s/config", topicha, revk_id, tag) >= 0)
      {
         if (!ok)
            revk_mqtt_send_str (topic);
         else
         {
            jo_t j = make (tag, icon);
            jo_string (j, "name", name);
            jo_string (j, "dev_cla", "battery");
            jo_string (j, "state_class", "measurement");
            jo_string (j, "stat_t", hastatus);
            jo_string (j, "unit_of_meas", "%");
            jo_stringf (j, "val_tpl", "{{value_json.%s}}", tag);
            revk_mqtt_send (NULL, 1, topic, &j);
         }
         free (topic);
      }
   }
   addtemp (ble && bletemp && bletemp->tempset, "bletemp", "BLE Temp", "mdi:thermometer");
   addhum (ble && bletemp && bletemp->humset, "blehum", "BLE Humidity", "mdi:water-percent");
   addbat (ble && bletemp && bletemp->batset, "blebat", "BLE Battery", "mdi:battery-bluetooth-variant");
#endif
#if 1
   if (asprintf (&topic, "%s/select/%sdemand/config", topicha, revk_id) >= 0)
   {
      if (!(daikin.status_known & CONTROL_demand))
         revk_mqtt_send_str (topic);
      else
      {
         jo_t j = make ("demand", NULL);
         jo_string (j, "name", "Demand control");
         jo_stringf (j, "cmd_t", "%s/demand", cmd);
         jo_string (j, "stat_t", hastatus);
         jo_string (j, "val_tpl", "{{value_json.demand}}");
         jo_array (j, "options");
         for (int i = 30; i <= 100; i += 5)
            jo_stringf (j, NULL, "%d", i);
         jo_close (j);
         revk_mqtt_send (NULL, 1, topic, &j);
      }
      free (topic);
   }
#endif
   if (asprintf (&topic, "%s/sensor/%senergy/config", topicha, revk_id) >= 0)
   {
      if (!(daikin.status_known & CONTROL_Wh))
         revk_mqtt_send_str (topic);
      else
      {
         jo_t j = make ("energy", NULL);
         jo_string (j, "name", "Lifetime energy");
         jo_string (j, "dev_cla", "energy");
         jo_string (j, "stat_t", hastatus);
         jo_string (j, "unit_of_meas", "kWh");
         jo_string (j, "state_class", "total_increasing");
         jo_stringf (j, "val_tpl", "{{(value_json.Wh|float)/1000}}");
         revk_mqtt_send (NULL, 1, topic, &j);
      }
      free (topic);
   }
   free (cmd);
   free (hastatus);
}

static void
ha_status (void)
{                               // Home assistant message
   if (!haenable)
      return;
   revk_command ("status", NULL);
}

void
revk_state_extra (jo_t j)
{
   if (!haenable)
      return;
   if (b.loopback)
      jo_bool (j, "loopback", 1);
   else if (daikin.status_known & CONTROL_online)
      jo_bool (j, "online", daikin.online);
   if (daikin.status_known & CONTROL_power)
      jo_bool (j, "power", daikin.power);
   //if (daikin.status_known & CONTROL_temp) // HA always expects this
   jo_litf (j, "target", "%.2f", autor ? (float) autot / autot_scale : daikin.temp);    // Target - either internal or what we are using as reference
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
   if (daikin.status_known & CONTROL_demand)
      jo_int (j, "demand", daikin.demand);
   if ((daikin.status_known & CONTROL_Wh) && daikin.Wh)
      jo_int (j, "Wh", daikin.Wh);
   if (daikin.status_known & CONTROL_fanrpm)
   {
      if (hafanrpm)
         jo_int (j, "fanfreq", daikin.fanrpm);
      else
         jo_litf (j, "fanfreq", "%.1f", daikin.fanrpm / 60.0);
   }
   if (daikin.status_known & CONTROL_comp)
      jo_int (j, "comp", (hacomprpm ? 60 : 1) * daikin.comp);
#ifdef ELA
   if (ble && bletemp)
   {
      if (bletemp->tempset)
         jo_litf (j, "bletemp", "%.2f", bletemp->temp / 100.0);
      if (bletemp->humset)
         jo_litf (j, "blehum", "%.2f", bletemp->hum / 100.0);
      if (bletemp->batset)
         jo_int (j, "blebat", bletemp->bat);
   }
#endif
   if (daikin.status_known & CONTROL_mode)
   {
      const char *modes[] = { "fan_only", "heat", "cool", "heat_cool", "4", "5", "6", "dry" };  // FHCA456D
      jo_string (j, "mode", daikin.power ? autor && !lockmode ? "heat_cool" : modes[daikin.mode] : "off");      // If we are controlling, it is auto
   }
   if (!nohvacaction)
      jo_string (j, "action", hvac_action[daikin.action]);
   if (daikin.status_known & CONTROL_fan)
   {
      const struct FanMode *f = get_fan_modes ();
      jo_string (j, "fan", f[daikin.fan].name);
   }
   if (daikin.status_known & CONTROL_streamer)
      jo_bool (j, "streamer", daikin.streamer);
   if (daikin.status_known & CONTROL_quiet)
      jo_bool (j, "quiet", daikin.quiet);
   if (daikin.status_known & CONTROL_econo)
      jo_bool (j, "econo", daikin.econo);
   if (daikin.status_known & CONTROL_comfort)
      jo_bool (j, "comfort", daikin.comfort);
   if (daikin.status_known & CONTROL_powerful)
      jo_bool (j, "powerful", daikin.powerful);
   if (daikin.status_known & CONTROL_sensor)
      jo_bool (j, "sensor", daikin.sensor);
   if (daikin.status_known & (CONTROL_swingh | CONTROL_swingv | CONTROL_comfort))
      jo_string (j, "swing",
                 daikin.comfort ? "C" : daikin.swingh & daikin.swingv ? "H+V" : daikin.swingh ? "H" : daikin.swingv ? "V" : "off");
   if (daikin.status_known & (CONTROL_econo | CONTROL_powerful))
      jo_string (j, "preset", daikin.econo ? "eco" : daikin.powerful ? "boost" : nohomepreset ? "none" : "home");       // Limited modes
}

void
uart_setup (void)
{
   esp_err_t err = 0;
   ESP_LOGI (TAG, "Trying %s Tx %s%d Rx %s%d", proto_name (), (proto & PROTO_TXINVERT) ? "¬" : "",
             tx.num, (proto & PROTO_RXINVERT) ? "¬" : "", rx.num);
   if (!err)
      err = gpio_reset_pin (rx.num);
   if (!err)
      err = gpio_reset_pin (tx.num);
   if (proto_type () == PROTO_TYPE_CN_WIRED)
   {
      err = cn_wired_driver_install (rx.num, tx.num, invert_rx_line (), invert_tx_line ());
   } else
   {
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
         if (invert_rx_line ())
            i |= UART_SIGNAL_RXD_INV;
         if (invert_tx_line ())
            i |= UART_SIGNAL_TXD_INV;
         err = uart_set_line_inverse (uart, i);
      }
      if (!err)
         err = uart_driver_install (uart, 1024, 0, 0, NULL, 0);
      if (!err)
         err = uart_set_rx_full_threshold (uart, 1);
      if (!err)
      {
         sleep (1);
         err = uart_flush (uart);
      }
   }
   if (err)
   {
      jo_t j = jo_object_alloc ();
      jo_string (j, "error", "Failed to set up commmunication port");
      jo_int (j, "uart", uart);
      jo_string (j, "description", esp_err_to_name (err));
      revk_error ("uart", &j);
   }
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
revk_web_extra (httpd_req_t * req, int page)
{
   revk_web_setting (req, "Fahrenheit", "fahrenheit");
   revk_web_setting (req, "Text rather than icons", "noicons");
   revk_web_setting (req, "Home Assistant", "haenable");
   if (haenable)
      revk_web_setting (req, "HA Extra switches", "haswitches");
   revk_web_setting (req, "Dark mode LED", "dark");
   if (!daikin.remote)
   {
      revk_web_setting (req, "No Faikin auto mode", "nofaikinauto");
      if (!nofaikinauto)
         revk_web_setting (req, "BLE Sensors", "ble");
   }
   revk_web_setting (req, "Debug", "debug");
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

   if (udp_discovery)
      revk_task ("daikin_discovery", legacy_discovery_task, NULL, 0);

   b.dumping = dump;
   revk_blink (0, 0, "");

   if (webcontrol || websettings)
   {
      // Web interface
      httpd_config_t config = HTTPD_DEFAULT_CONFIG ();
      config.stack_size += 2048;        // Being on the safe side
      // When updating the code below, make sure this is enough
      // Note that we're also adding revk's own web config handlers
      config.max_uri_handlers = 14 + revk_num_web_handlers ();
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
            register_get_uri ("/aircon/set_holiday", legacy_web_set_holiday);
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
   if (!uart_enabled ())
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
   else
      proto--;                  // We start by moving forward if protocol not set
   while (1)
   {                            // Main loop
      // We're (re)starting comms from scratch, so set "talking" flag.
      // This signals protocol integrity and actually enables communicating with the AC.
      if (!protocol_set && !b.loopback)
      {                         // Scanning protocols - more to next protocol
         proto++;
         if (proto >= PROTO_TYPE_MAX * PROTO_SCALE)
            proto = 0;
         if (proto_type () == PROTO_TYPE_CN_WIRED)
         {
            uint8_t invert_mask;

            if (nocnwired)
               continue;
            // Since CN_WIRED is a passive protocol (receive only, no actual responses),
            // we cannot have idea whether our tx polarity is correct. If we choose a wrong one,
            // the AC won't receive anything, but we'd have no way to detect that.
            // So, here we explicitly ban having different polarities. Invert either all or nothing.
            invert_mask = proto & (PROTO_TXINVERT | PROTO_RXINVERT);
            if (invert_mask == PROTO_TXINVERT || invert_mask == PROTO_RXINVERT)
               continue;
         }
         if ((proto_type () == PROTO_TYPE_S21 && nos21) ||      //
             (proto_type () == PROTO_TYPE_X50A && nox50a) ||    //
             (proto_type () == PROTO_TYPE_ALTHERMA_S && noas) ||        //
             ((proto & PROTO_TXINVERT) && noswaptx) ||  //
             ((proto & PROTO_RXINVERT) && noswaprx))
         {                      // not a protocol we want to scan, so try again
            usleep (1000);      // Yeh, silly, but someone could configure to do nothing
            continue;
         }
      }
      daikin.talking = 1;
      if (uart_enabled ())
      {                         // Poke UART
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
      daikin.ha_send = 1;
      int poll = 0;
      do
      {
         // Polling loop. We exit from here only if we get a protocol error
         poll++;
         if (proto_type () != PROTO_TYPE_CN_WIRED)
         {
            /* wait for next second. For CN_WIRED we don't need to actively poll the
               A/C, so we don't need this delay. We just keep reading, packets should
               come once per second, and that's our timing */
            usleep (1000000LL - (esp_timer_get_time () % 1000000LL));
         }
#ifdef ELA
         if (ble_sensor_enabled ())
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
                     daikin.ha_send = 1;
                     break;
                  }
            }
            if (bletemp && !bletemp->missing && bletemp->tempset)
            {                   // Use temp
               float env = bletemp->temp / 100.0;
               if (daikin.env != env)
                  daikin.status_changed = 1;
               daikin.env = env;
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
         if (uart_enabled ())
         {
            if (proto_type () == PROTO_TYPE_ALTHERMA_S)
            {
#define poll(a)                         \
   static uint8_t a=2;                  \
   if(a){                               \
      int r=daikin_as_poll(*#a); \
      if (r==RES_OK)                          \
         a=100;                         \
      else if(r==RES_NAK)                     \
         a--;                           \
   }                                          \
   if(!daikin.talking)                        \
      a=2;
               poll (U);
               poll (T);
               poll (P);
               poll (S);
#undef poll
            } else if (proto_type () == PROTO_TYPE_CN_WIRED)
            {                   // CN WIRED
               uint8_t buf[CNW_PKT_LEN];
               esp_err_t e = cn_wired_read_bytes (buf, protofix ? 20000 : 5000);

               if (e == ESP_ERR_TIMEOUT)
               {
                  daikin.online = false;
                  comm_timeout (NULL, 0);
               } else if (e == ESP_OK)
               {
                  daikin_cn_wired_incoming_packet (buf);

                  // Send new modes to the AC. We have just received a data packet; CN_WIRED devices
                  // may dislike being interrupted, so we delay for 20 ms in order for the packet
                  // trailer pulse (which we ignore) passes
                  sys_msleep (20);
                  // We send modes as a "response" to every packet from the AC. We know that original
                  // equipment (wall panel, as well as Daichi 3rd party controller) does that too; and
                  // we also know that some ACs (FTN15PV1L) don't take commands on 1st try if we don't
                  // do so. Perhaps they think we are offline.
                  daikin_cn_wired_send_modes ();
               } else
               {
                  daikin.talking = 0;   // Not ready?
               }
            } else if (proto_type () == PROTO_TYPE_S21)
            {                   // Older S21
               char temp[5];
               if (debug)
                  s21debug = jo_object_alloc ();
               // Poll the AC status.
               // Each value has a smart NAK counter (see macro below), which allows
               // for autodetecting unsupported commands
               // Not the do{}while(0); logic is so this works as a single C statement can so can  be used as if()poll(); safely
#define poll(a,b,c,d)                         		\
   do							\
   {							\
   	if(!s21.a##b##d.bad)				\
	{						\
      	    int r=daikin_s21_command(*#a,*#b,c,#d);	\
      	    if (r==RES_OK)                          	\
      	    {                         	 		\
        	s21.a##b##d.ack=1;            		\
        	s21.a##b##d.nak=0;            		\
        	s21.a##b##d.bad=0;            		\
	    }                         	 		\
      	    else if(r==RES_NAK)                     	\
      	    {                         	 		\
         	s21.a##b##d.nak++;                      \
		if(!s21.a##b##d.nak)			\
		    s21.a##b##d.bad=1;			\
	    }                         	 		\
   	}                                          	\
   	if(!daikin.talking)                        	\
      	    s21.a##b##d.ack=s21.a##b##d.nak=s21.a##b##d.bad=0;\
    } while(0)

               poll (F, 1, 0,);
               if (debug)
                  poll (F, 2, 0,);
               if (s21.F6.bad || debug)
                  poll (F, 3, 0,);      // If F6 works we assume we don't need F3
               if (debug)
                  poll (F, 4, 0,);
               poll (F, 5, 0,);
               poll (F, 6, 0,);
               poll (F, 7, 0,);
               if (!s21.F8.ack)
                  poll (F, 8, 0,);      // One time static value
               poll (F, 9, 0,);
               if (debug)
                  poll (F, A, 0,);
               if (debug)
                  poll (F, B, 0,);
               if (!s21.FC.ack)
                  poll (F, C, 0,);      // One time static value
               if (debug)
                  poll (F, G, 0,);
               if (debug)
                  poll (F, K, 0,);
               poll (F, M, 0,);
               if (debug)
                  poll (F, N, 0,);
               if (debug)
                  poll (F, P, 0,);
               if (debug)
                  poll (F, Q, 0,);
               if (debug)
                  poll (F, S, 0,);
               if (debug)
                  poll (F, T, 0,);
               //if(debug)poll (F, U, 2, 02);
               //if(debug)poll (F, U, 2, 04);
               {
                  uint8_t n = ((time (0) / 3600) & 1);
                  if (n != b.hourly)
                  {             // Hourly
                     b.hourly = n;
                     poll (D, H, 4, 1000);
                  }
               }

               static uint8_t rcycle = 0;       // R polling one per cycle
               switch (rcycle++)
               {
               case 0:
                  poll (R, H, 0,);
                  break;
               case 1:
                  poll (R, I, 0,);
                  break;
               case 2:
                  poll (R, a, 0,);
                  break;
               case 3:
                  poll (R, L, 0,);      // Fan speed
                  break;
               case 4:
                  poll (R, d, 0,);      // Compressor
                  break;
               case 5:
                  poll (R, N, 0,);      // Angle
                  break;
               case 6:
                  poll (R, G, 0,);      // Fan
                  if (!debug)
                     rcycle = 0;        // End of normal R polling - the following are debug only
                  break;
               case 7:
                  poll (R, M, 0,);
                  break;
               case 8:
                  poll (R, X, 0,);
                  break;
               case 9:
                  poll (R, D, 0,);
                  rcycle = 0;   // End of debug cycle
                  break;
               }

               if (!s21.RH.bad && !s21.Ra.bad)
                  s21.F9.bad = 1;       // Don't use F9
               if (debugsend)
               {
                  b.dumping = 1;        // Force dumping
                  while (jo_here (debugsend) != JO_END)
                  {
                     if (jo_here (debugsend) == JO_STRING)
                     {
                        char temp[10];
                        ssize_t l = jo_strncpy (debugsend, temp, sizeof (temp));
                        if (l > sizeof (temp))
                        {
                           ESP_LOGE (TAG, "send too long %d", l);
                           l = sizeof (temp);
                        }
                        // JSON is UTF-8 coded, but that makes no sense, so pack to latin 1
                        uint8_t *i = (uint8_t *) temp,
                           *e = i + l,
                           *o = i;
                        while (i < e)
                        {
                           if (*i >= 0xC2 && *i <= 0xC3 && i + 1 < e && (i[1] & 0xC0) == 0x80)
                           {    // 0x80 to 0xFF encoded as UTF-8
                              *o++ = ((*i & 0x03) << 6) | (i[1] & 0x3F);
                              i += 2;
                              l--;
                              continue;
                           }
                           *o++ = *i++;
                        }
                        if (l < 2)
                           ESP_LOGE (TAG, "send too short %d", l);
                        else
                           daikin_s21_command (temp[0], temp[1], l - 2, temp + 2);
                     }
                     jo_next (debugsend);
                  }
                  jo_free (&debugsend);
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
                  if (!s21.F6.bad)
                  {
                     temp[0] = '0' + (daikin.powerful ? 2 : 0) + (daikin.comfort ? 0x40 : 0) + (daikin.quiet ? 0x80 : 0);
                     temp[1] = '0' + (daikin.streamer ? 0x80 : 0);
                     temp[2] = '0';
                     // If sensor, the 8 is sensor, if not, then 4 and 8 are LED, with 4=high, 8=low, 12=off
                     if (noled || !nosensor)
                        temp[3] = '0' + (daikin.sensor ? 0x08 : 0) + (daikin.led ? 0x04 : 0);   // Messy but gives some controls
                     else
                        temp[3] = '0' + (daikin.led ? dark ? 8 : 4 : 12);
                     // FIXME: ATX20K2V1B responds NAK to this command, but also doesn't react on D3.
                     // Looks like it supports something else, we don't know what.
                     // https://github.com/revk/ESP32-Faikin/issues/441
                     daikin_s21_command ('D', '6', S21_PAYLOAD_LEN, temp);
                  } else if (!s21.F3.bad)
                  {             // F3 or F6 depends on model
                     // Actually many ACs (tested on FTXF20D5V1B and ATX20K2V1B) respond to
                     // both F3 and F6, but F3 does not report "powerful" state, so we give
                     // F6 a preference.
                     // The current code assumes that only units, which don't respond to F6
                     // at all, will report the flag in F3, and require D3 to control.
                     // This suggestion must be true, because otherwise commit 0c5f769, which
                     // introduced support for F3, wouldn't have worked, being overriden by F6
                     // due to how poll sequence is organized.
                     temp[0] = '0';
                     temp[1] = '0';
                     temp[2] = '0';
                     temp[3] = '0' + (daikin.powerful ? 2 : 0);
                     daikin_s21_command ('D', '3', S21_PAYLOAD_LEN, temp);
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
         float measured_temp = daikin.env;
         if (isnan (measured_temp))     // No env temp available, so use A/C internal temp
            measured_temp = daikin.home;
         xSemaphoreGive (daikin.mutex);

         // Predict temperature changes
         // Take 2 delta temps of the last 3 measured env temperatures
         // If there is no opposite movement (both cooler or both hotter or at least no change),
         //  push (increase or decrease) the measured env temp by adding the two deltatemps 
         //  multiplied with a factor. 
         // This "predicted" env temp is used for all further calculations.
         // E.g.: temps [19.5, 19.6, 19.8] (with 19.8 as the most recent value)
         //       leads to deltas [0.1, 0.2]
         //       new "predicted" env temp is (19.8+(0.1+0.2)*2)=20.4 (*2 is calculated from tpredictt and tpredicts)
         // tpredicts is the "sample time" for the calculation (it must be taken *2, because the deltas are calculated over 2 cycles)
         // tpredictt is the time in the future where the predicted env temp would be reached.
         if (tpredicts && !isnan (measured_temp))
         {
            static uint32_t lasttime = 0;
            if (now / tpredicts != lasttime / tpredicts)
            {                   // Every minute - predictive
               lasttime = now;
               daikin.env_delta_prev = daikin.env_delta;        // Save last delta
               daikin.env_delta = measured_temp - daikin.env_prev;      // Delta from currently measured temperature and previously measured temperature
               // daikin.env_delta < 0 means the room is cooling down
               // daikin.env_delta > 0 means the room is heating up
               daikin.env_prev = measured_temp; // Save current temperature for next cycle
            }
            // Two subsequent temperature changes in the same direction ("no change" is ok as well)
            if ((daikin.env_delta <= 0 && daikin.env_delta_prev <= 0) || (daikin.env_delta >= 0 && daikin.env_delta_prev >= 0))
               measured_temp += (daikin.env_delta + daikin.env_delta_prev) * tpredictt / (tpredicts * 2);       // Predict
         }
         // Apply adjustment
         if (!thermostat && daikin.control && daikin.power && !isnan (min) && !isnan (max))
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
            report_uint8 (control, 1);
            samplestart ();

            // Switch modes (heating or cooling) depending on currently measured 
            //  temperature related to min/max
            if (!lockmode)
            {
               if (hot && measured_temp > max)
               {
                  hot = 0;
                  daikin_set_e (mode, "C");     // Set cooling as over temp
               } else if (!hot && measured_temp < min)
               {
                  hot = 1;
                  daikin_set_e (mode, "H");     // Set heating as under temp
               }
            }
            // Force high fan at the beginning if not fan in AUTO 
            //  and temperature not close to target temp
            // TODO: Use of switchtemp for different purposes is confusing (ref. min/max a couple of lines above)
            if (!nofanauto && daikin.fan
                && ((hot && measured_temp < min - 2 * (float) switchtemp / switchtemp_scale)
                    || (!hot && measured_temp > max + 2 * (float) switchtemp / switchtemp_scale)))
            {
               daikin.fansaved = daikin.fan;    // Save for when we get to temp
               daikin_set_v (fan, autofmax);    // Max fan at start
            }
         }
         // END OF controlstart()


         void controlstop (void)
         {                      // Stop controlling
            if (!daikin.control)
               return;
            report_uint8 (control, 0);
            if (daikin.fansaved)
            {                   // Restore saved fan setting (if was set, which nofanauto would not do)
               daikin_set_v (fan, daikin.fansaved);
               daikin.fansaved = 0;
            }
            // We were controlling, so set to a non controlling mode, best guess at sane settings for now
            if (!isnan (daikin.mintarget) && !isnan (daikin.maxtarget))
               daikin_set_t (temp, daikin.heat ? daikin.maxtarget : daikin.mintarget);
            daikin.mintarget = NAN;
            daikin.maxtarget = NAN;
         }
         // END OF controlstop()


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
               if (!lockmode && daikin.mode != 3 && !isnan (measured_temp) && !isnan (min) && !isnan (max)
                   && ((hot && measured_temp > max) || (!hot && measured_temp < min)))
                  daikin_set_e (mode, hot ? "C" : "H"); // Swap mode
            }
            last = hhmm;
         }
         // Monitoring and automation
         if (!isnan (measured_temp) && !isnan (min) && !isnan (max) && tsample)
         {                      // Monitoring and automation
            if (daikin.power && daikin.lastheat != hot)
            {                   // If we change mode, start samples again
               daikin.lastheat = hot;
               samplestart ();
            }

            if (!daikin.sample)
            {
               // TODO: Wouldn't this be better in samplestart()?
               daikin.countApproaching = daikin.countApproachingPrev = daikin.countBeyond = daikin.countBeyondPrev = daikin.countTotal = daikin.countTotalPrev = 0;     // Reset sample counts
            } else
            {
               daikin.countTotal++;     // Total
               if ((hot && measured_temp < min) || (!hot && measured_temp > max))
                  daikin.countApproaching++;    // Approaching temp
               else if ((hot && measured_temp > max) || (!hot && measured_temp < min))
                  daikin.countBeyond++; // Beyond
            }

            // New sample Cycle
            if (daikin.sample <= now)
            {                   // New sample, consider some changes
               // daikin.countTotalPrev is Total Counter of previous cycle
               int count_approaching_2_samples = daikin.countApproaching + daikin.countApproachingPrev; // Approaching counter of this and previous cycle
               int countBeyond2Samples = daikin.countBeyond + daikin.countBeyondPrev;   // Beyond counter of this and previous cycle
               int count_total_2_samples = daikin.countTotal + daikin.countTotalPrev;   // Total counter of this and previous cycle (includes neither approaching or beyond, i.e. in range)

               // Prepare reporting structure for "automation"
               jo_t j = jo_object_alloc ();
               jo_bool (j, "hot", hot);
               if (count_total_2_samples)
               {
                  jo_int (j, "approaching", count_approaching_2_samples);
                  jo_int (j, "beyond", countBeyond2Samples);
                  jo_int (j, daikin.countTotalPrev ? "samples" : "initial-samples", count_total_2_samples);
               }
               jo_int (j, "period", tsample);
               jo_litf (j, "temp", "%.2f", measured_temp);
               jo_litf (j, "min", "%.2f", min);
               jo_litf (j, "max", "%.2f", max);

               if (daikin.countTotalPrev)       // Skip first cycle
               {                // Power, mode, fan, automation
                  if (daikin.power)     // Daikin is on
                  {
                     int step = (fanstep ? : (proto_type () == PROTO_TYPE_S21) ? 1 : 2);

                     // A lot more beyond than total counts and no approaching in the last two cycles
                     // Time to switch modes (heating/cooling) and reduce fan to minimum
                     if ((countBeyond2Samples * 2 > count_total_2_samples || daikin.slave) && !count_approaching_2_samples)
                     {          // Mode switch
                        if (!lockmode)
                        {
                           jo_string (j, "set-mode", hot ? "C" : "H");
                           daikin_set_e (mode, hot ? "C" : "H");        // Swap mode

                           if (!nofanauto && step && daikin.fan > 1 && daikin.fan <= 5)
                           {
                              jo_int (j, "set-fan", 1);
                              daikin_set_v (fan, 1);
                           }
                        }
                     }
                     // Less approaching, but still close to min in heating or max in cooling
                     // Time to reduce the fan a bit
                     else if (!nofanauto && count_approaching_2_samples * 10 < count_total_2_samples * 7
                              && step && daikin.fan > 1 && daikin.fan <= 5)
                     {
                        jo_int (j, "set-fan", daikin.fan - step);
                        daikin_set_v (fan, daikin.fan - step);  // Reduce fan
                     }
                     // A lot of approaching means still far away from desired temp
                     // Time to increase the fan speed
                     else if (!nofanauto && !daikin.slave
                              && count_approaching_2_samples * 10 > count_total_2_samples * 9
                              && step && daikin.fan >= 1 && daikin.fan < autofmax)
                     {
                        jo_int (j, "set-fan", daikin.fan + step);
                        daikin_set_v (fan, daikin.fan + step);  // Increase fan
                     }
                     // No Approaching and no Beyond, so it's in desired range (autot +/- autor)
                     // Only affects if autop is active
                     // Turn off as 100% in band for last two period
                     else if ((autop || (daikin.remote && autoptemp)) && !count_approaching_2_samples && !countBeyond2Samples)
                     {          // Auto off
                        jo_bool (j, "set-power", 0);
                        daikin_set_v (power, 0);        // Turn off as 100% in band for last two period
                     }
                  }
                  // Daikin is off
                  else if ((autop || (daikin.remote && autoptemp))      // AutoP Mode only
                           && (daikin.countApproaching == daikin.countTotal || daikin.countBeyond == daikin.countTotal) // full cycle approaching or full cycle beyond
                           && (measured_temp >= max + (float) autoptemp / autoptemp_scale       // temp out of desired range
                               || measured_temp <= min - (float) autoptemp / autoptemp_scale) && (!lockmode || countBeyond2Samples != count_total_2_samples))   // temp out of desired range
                  {             // Auto on (don't auto on if would reverse mode and lockmode)
                     jo_bool (j, "set-power", 1);
                     daikin_set_v (power, 1);   // Turn on as 100% out of band for last two period
                     if (countBeyond2Samples == count_total_2_samples)
                     {
                        jo_string (j, "set-mode", hot ? "C" : "H");
                        daikin_set_e (mode, hot ? "C" : "H");   // Swap mode
                     }
                  }
               }
               if (count_total_2_samples)       // after a cycle, send automation data  
                  revk_info ("automation", &j);
               else
                  jo_free (&j);

               // Next sample
               daikin.countApproachingPrev = daikin.countApproaching;
               daikin.countBeyondPrev = daikin.countBeyond;
               daikin.countTotalPrev = daikin.countTotal;
               daikin.countApproaching = daikin.countBeyond = daikin.countTotal = 0;    // Reset counter
               daikin.sample = now + tsample;   // Set time for next sample cycle
            }
         }
         // End Control due to timeout
         if (daikin.controlvalid && now > daikin.controlvalid)
         {                      // End of auto mode and no env data either
            daikin.controlvalid = 0;
            daikin.status_known &= ~CONTROL_env;
            daikin.status_changed = 1;
            daikin.env = NAN;
            daikin.remote = 0;
            controlstop ();
         }
         // Local auto controls
         if (daikin.power && daikin.controlvalid && !revk_shutting_down (NULL))
         {                      // Local auto controls
            // Get the settings atomically
            if (isnan (min) || isnan (max))
               controlstop ();
            else
            {                   // Control
               controlstart (); // Will do nothing if control already active

               // What the A/C is using as current temperature
               float reference = NAN;
               if ((daikin.status_known & (CONTROL_home | CONTROL_inlet)) == (CONTROL_home | CONTROL_inlet))    // Both values are known
                  reference = (daikin.home * thermref + daikin.inlet * (100 - thermref)) / 100; // thermref is how much inlet and home are used as reference
               else if (daikin.status_known & CONTROL_home)
                  reference = daikin.home;
               else if (daikin.status_known & CONTROL_inlet)
                  reference = daikin.inlet;
               // It looks like the ducted units are using inlet in some way, even when field settings say controller.
               if (daikin.mode == 3)
                  daikin_set_e (mode, hot ? "H" : "C"); // Out of auto
               // Temp set
               float set = (min + max) / 2.0;   // Target temp we will be setting (before adjust for reference error and before limiting)
               if (thermostat)
                  set = (((hot && daikin.hysteresis) || (!hot && !daikin.hysteresis)) ? max : min);
               if (temptrack)
                  set = reference;      // Base target on current Daikin measured temp instead.
               else if (tempadjust)
                  set += reference - measured_temp;     // Adjust for reference not being measured_temp
               if ((hot && measured_temp < (daikin.hysteresis ? max : min))
                   || (!hot && measured_temp > (daikin.hysteresis ? min : max)))
               {                // Apply heat/cool - i.e. force heating or cooling to definitely happen
                  if (thermostat)
                     daikin.hysteresis = 1;     // We're on, so keep going to "beyond"
                  if (hot)
                  {
                     set += heatover;   // Ensure heating by applying A/C offset to force it
                     daikin.action = HVAC_HEATING;
                  } else
                  {
                     set -= coolover;   // Ensure cooling by applying A/C offset to force it
                     daikin.action = HVAC_COOLING;
                  }
                  if (!noled && autolcontrol)
                  {
                     daikin_set_v (led, 1);
                  }
               } else
               {                // At or beyond temp - stop heat/cool - try and ensure it stops heating or cooling
                  daikin.action = HVAC_IDLE;
                  daikin.hysteresis = 0;        // We're off, so keep falling back until "approaching" (default when thermostat not set)
                  if (daikin.fansaved)
                  {
                     daikin_set_v (fan, daikin.fansaved);       // revert fan speed (if set, which nofanauto would not do)
                     daikin.fansaved = 0;
                     samplestart ();    // Initial phase complete, start samples again.
                  }
                  if (hot)
                     set -= heatback;   // Heating mode but apply negative offset to not actually heat any more than this
                  else
                     set += coolback;   // Cooling mode but apply positive offset to not actually cool any more than this
                  if (!noled && autolcontrol)
                  {
                     daikin_set_v (led, 0);
                  }
               }

               // Limit settings to acceptable values
               if (proto_type () == PROTO_TYPE_CN_WIRED)
                  set = roundf (set);   // CN_WIRED only does 1C steps
               else if (proto_type () == PROTO_TYPE_S21)
                  set = roundf (set * 2.0) / 2.0;       // S21 only does 0.5C steps
               if (set < (hot ? tmin : tcoolmin))
                  set = (hot ? tmin : tcoolmin);
               if (set > (hot ? theatmax : tmax))
                  set = (hot ? theatmax : tmax);
               static uint32_t flap = 0;
               static uint8_t lastaction = 0;
               static float lastset = 0;
               if (!isnan (set) && (daikin.action != lastaction || (set != lastset && now > flap)))
               {
                  flap = now + tempnoflap;      // Hold off changes for preset time, unless change of mode
                  lastaction = daikin.action;
                  lastset = set;
                  daikin_set_t (temp, set);     // Apply temperature setting
               }
            }
         } else
         {
            controlstop ();
            // Just based on mode
            daikin.action = (!daikin.power ? HVAC_OFF : daikin.antifreeze ? HVAC_DEFROSTING : daikin.mode == FAIKIN_MODE_HEAT ? HVAC_HEATING :  //
                             daikin.mode == FAIKIN_MODE_COOL ? HVAC_COOLING :   //
                             daikin.mode == FAIKIN_MODE_AUTO ? HVAC_IDLE :      //
                             daikin.mode == FAIKIN_MODE_DRY ? HVAC_DRYING :     //
                             daikin.mode == FAIKIN_MODE_FAN ? HVAC_FAN :        //
                             HVAC_IDLE);
         }
         // End of local auto controls

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
#define	b(name)		if(daikin.status_known&CONTROL_##name){if(!daikin.total##name)jo_bool(j,#name,0);else if(fixstatus||daikin.total##name==daikin.statscount)jo_bool(j,#name,1);else jo_litf(j,#name,"%.2f",(float)daikin.total##name/daikin.statscount);} \
		  	daikin.total##name=0;
#define	t(name)		if(daikin.count##name&&!isnan(daikin.total##name)){if(fixstatus||daikin.min##name==daikin.max##name)jo_litf(j,#name,"%.2f",daikin.min##name);	\
		  	else {jo_array(j,#name);jo_litf(j,NULL,"%.2f",daikin.min##name);jo_litf(j,NULL,"%.2f",daikin.total##name/daikin.count##name);jo_litf(j,NULL,"%.2f",daikin.max##name);jo_close(j);}}	\
		  	daikin.min##name=NAN;daikin.total##name=0;daikin.max##name=NAN;daikin.count##name=0;
#define	r(name)		if(!isnan(daikin.min##name)&&!isnan(daikin.max##name)){if(fixstatus||daikin.min##name==daikin.max##name)jo_litf(j,#name,"%.2f",daikin.min##name);	\
			else {jo_array(j,#name);jo_litf(j,NULL,"%.2f",daikin.min##name);jo_litf(j,NULL,"%.2f",daikin.max##name);jo_close(j);}}
#define	i(name)		if(daikin.status_known&CONTROL_##name){if(fixstatus||daikin.min##name==daikin.max##name)jo_int(j,#name,daikin.total##name/daikin.statscount);     \
                        else {jo_array(j,#name);jo_int(j,NULL,daikin.min##name);jo_int(j,NULL,daikin.total##name/daikin.statscount);jo_int(j,NULL,daikin.max##name);jo_close(j);}       \
                        daikin.min##name=0;daikin.total##name=0;daikin.max##name=0;}
#define e(name,values)  if((daikin.status_known&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
#include "acextras.m"
                  revk_mqtt_send_clients (appname, 0, NULL, &j, 1);
                  daikin.statscount = 0;
                  ha_status ();
               }
            }
         }
         if (poll > 10 && daikin.ha_send && protocol_set && daikin.talking)
         {
            send_ha_config ();
            ha_status ();       // Update status now sent
         }
      }
      while (daikin.talking);
      // We're here if protocol has been broken. We'll reconfigure the UART
      // and restart from scratch, possibly changing the protocol, if we're
      // in detection phase.
      if (proto_type () == PROTO_TYPE_CN_WIRED)
         cn_wired_driver_delete ();
      else
         uart_driver_delete (uart);
   }
}
