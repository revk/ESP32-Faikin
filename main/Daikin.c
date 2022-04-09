/* Daikin app */
/* Copyright ©2022 Adrian Kennard, Andrews & Arnold Ltd. See LICENCE file for details .GPL 3.0 */

static __attribute__((unused))
const char TAG[] = "Daikin";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include "esp_http_server.h"
#include <math.h>

// The following define the controls and status for the Daikin, using macros
// e(name,tags) Enumerated value, uses single character from tags, e.g. FHCA456D means "F" is 0, "H" is 1, etc
// b(name)      Boolean
// t(name)      Temperature
// s(name,len)  String (for status only, e.g. model)

// Daikin controls
#define	accontrol		\
	b(power)		\
	e(mode,FHCA456D)	\
	t(temp)			\
	e(fan,A12345Q)		\
	b(swingh)		\
	b(swingv)		\
	b(econo)		\
	b(powerful)		\

// Daikin status
#define acstatus		\
	b(online)		\
	s(model,20)		\
	t(home)			\
	b(heat)			\
	b(slave)		\
	t(outside)		\
	t(inlet)		\
	t(liquid)		\

// Macros for setting values
#define	daikin_set_v(name,value)	daikin_set_value(#name,&daikin.name,CONTROL_##name,value)
#define	daikin_set_e(name,value)	daikin_set_enum(#name,&daikin.name,CONTROL_##name,value,CONTROL_##name##_VALUES)
#define	daikin_set_t(name,value)	daikin_set_temp(#name,&daikin.name,CONTROL_##name,value)

// Settings (RevK library used by MQTT setting command)
#define	settings		\
	bl(debug)		\
	bl(dump)		\
	b(s21)			\
	u8(uart,1)		\
	u8l(coolbias10,15)	\
	u8l(heatbias10,20)	\
	u8l(switch10,5)		\
	u32(switchtime,3600)	\
	u32(switchdelay,900)	\
	u32(controltime,600)	\
	u32(fantime,3600)	\
	u8(fanstep,2)		\
	u32(reporting,60)	\
	io(tx,CONFIG_DAIKIN_TX)	\
	io(rx,CONFIG_DAIKIN_RX)	\

#define u32(n,d) uint32_t n;
#define s8(n,d) int8_t n;
#define u8(n,d) uint8_t n;
#define u8l(n,d) uint8_t n;
#define b(n) uint8_t n;
#define bl(n) uint8_t n;
#define s(n) char * n;
#define io(n,d)           uint8_t n;
settings
#undef io
#undef u32
#undef s8
#undef u8
#undef u8l
#undef b
#undef bl
#undef s
#define PORT_INV 0x40
#define port_mask(p) ((p)&63)
enum {                          // Number the control fields
#define	b(name)		CONTROL_##name##_pos,
#define	t(name)		b(name)
#define	e(name,values)	b(name)
#define	s(name,len)	b(name)
   accontrol acstatus
#undef	b
#undef	t
#undef	e
#undef	s
};
#define	b(name)		const uint64_t CONTROL_##name=(1ULL<<CONTROL_##name##_pos);
#define	t(name)		b(name)
#define	e(name,values)	b(name) const char CONTROL_##name##_VALUES[]=#values;
#define	s(name,len)	b(name)
accontrol acstatus
#undef	b
#undef	t
#undef	e
#undef	s
// Globals
static httpd_handle_t webserver = NULL;

// The current aircon state
struct {
   SemaphoreHandle_t mutex;     // Control changes
   uint64_t control_changed;    // Which control fields are being set
   uint64_t status_known;       // Which fields we know, and hence can control
   uint8_t control_count;       // How many times we have tried to change control and not worked yet
#define	b(name)		uint8_t	name;
#define	t(name)		float name;
#define	e(name,values)	uint8_t name;
#define	s(name,len)	char name[len];
   accontrol acstatus
#undef	b
#undef	t
#undef	e
#undef	s
   float acmin;                 // Min (heat to this) - NAN to leave to ac
   float acmax;                 // Max (cool to this) - NAN to leave to ac
   float achome;                // Reported home temp from external source
   uint32_t controlvalid;       // uptime to which auto mode is valid
   uint32_t acswitch;           // Last time we switched hot/cold
   uint32_t acapproaching;      // Last time we were approaching target temp
   uint32_t acbeyond;           // Last time we were at or beyond target temp
   uint8_t talking:1;           // We are getting answers
   uint8_t status_changed:1;    // Status has changed

} daikin;

const char *daikin_set_value(const char *name, uint8_t * ptr, uint64_t flag, uint8_t value)
{                               // Setting a value (uint8_t)
   if (*ptr == value)
      return NULL;              // No change
   if (!(daikin.status_known & flag))
      return "Setting cannot be controlled";
   xSemaphoreTake(daikin.mutex, portMAX_DELAY);
   *ptr = value;
   daikin.control_changed |= flag;
   xSemaphoreGive(daikin.mutex);
   return NULL;
}

const char *daikin_set_enum(const char *name, uint8_t * ptr, uint64_t flag, char *value, const char *values)
{                               // Setting a value (uint8_t) based on string which is expected to be single character from values set
   if (!value || !*value)
      return "No value";
   if (value[1])
      return "Value is meant to be one character";
   char *found = strchr(values, *value);
   if (!found)
      return "Value is not a valid value";
   daikin_set_value(name, ptr, flag, (int) (found - values));
   return NULL;
}

const char *daikin_set_temp(const char *name, float *ptr, uint64_t flag, float value)
{                               // Setting a value (float)
   if (*ptr == value)
      return NULL;              // No change
   xSemaphoreTake(daikin.mutex, portMAX_DELAY);
   *ptr = value;
   daikin.control_changed |= flag;
   xSemaphoreGive(daikin.mutex);
   return NULL;
}

void set_uint8(const char *name, uint8_t * ptr, uint64_t flag, uint8_t val)
{
   xSemaphoreTake(daikin.mutex, portMAX_DELAY);
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
   }
   xSemaphoreGive(daikin.mutex);
}

void set_float(const char *name, float *ptr, uint64_t flag, float val)
{
   xSemaphoreTake(daikin.mutex, portMAX_DELAY);
   if (!(daikin.status_known & flag))
   {
      daikin.status_known |= flag;
      daikin.status_changed = 1;
   }
   if (lroundf(*ptr * 10) == lroundf(val * 10))
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
   }
   xSemaphoreGive(daikin.mutex);
}

#define set_val(name,val) set_uint8(#name,&daikin.name,CONTROL_##name,val)
#define set_temp(name,val) set_float(#name,&daikin.name,CONTROL_##name,val)

void daikin_s21_response(uint8_t cmd, uint8_t cmd2, int len, uint8_t * payload)
{
   if (debug && len)
   {
      jo_t j = jo_object_alloc();
      jo_stringf(j, "cmd", "%c%c", cmd, cmd2);
      jo_base16(j, "payload", payload, len);
      jo_stringn(j, "text", (char *) payload, len);
      revk_info("rx", &j);
   }
   if (cmd == 'G' && len == 4)
      switch (cmd2)
      {
      case '1':
         set_val(online, 1);
         set_val(power, (payload[0] == '1') ? 1 : 0);
         set_val(mode, "03721003"[payload[1] & 0x7] - '0');     // FHCA456D mapped to XADCHXF
         set_temp(temp, 18.0 + 0.5 * (payload[2] - '@'));
         if (payload[3] == 'A')
            set_val(fan, 0);    // Auto
         else if (payload[3] == 'B')
            set_val(fan, 6);    // Quiet
         else
            set_val(fan, "00012345"[payload[3] & 0x7] - '0');   // XXX12345 mapped to A12345Q
         break;
      case '5':
         set_val(swingv, (payload[0] & 1) ? 1 : 0);
         set_val(swingh, (payload[0] & 2) ? 1 : 0);
         break;
      case '6':
         set_val(powerful, payload[0] == '2' ? 1 : 0);
         break;
      case '7':
         set_val(econo, payload[1] == '2' ? 1 : 0);
         break;
      }
   if (cmd == 'S' && len == 4)
   {
      float t = (payload[0] - '0') * 0.1 + (payload[1] - '0') + (payload[2] - '0') * 10;
      if (payload[3] == '-')
         t = -t;
      switch (cmd2)
      {                         // Temperatures
      case 'H':                // Guess
         set_temp(home, t);
         break;
      case 'a':                // Guess
         set_temp(outside, t);
         break;
      case 'I':                // Guess
         set_temp(liquid, t);
         break;
      }
   }
}

void daikin_response(uint8_t cmd, int len, uint8_t * payload)
{                               // Process response
   if (debug && len)
   {
      jo_t j = jo_object_alloc();
      jo_stringf(j, "cmd", "%02X", cmd);
      jo_base16(j, "payload", payload, len);
      revk_info("rx", &j);
   }
   if (cmd == 0xAA && len >= 1)
   {
      if (!*payload)
         daikin.talking = 0;    // Not ready
   }
   if (cmd == 0xBA && len >= 20)
   {
      strncpy(daikin.model, (char *) payload, sizeof(daikin.model));
      daikin.status_changed = 1;
   }
   if (cmd == 0xCA && len >= 7)
   {                            // Main status settings
      set_val(online, 1);
      set_val(power, payload[0]);
      set_val(mode, payload[1]);
      set_val(heat, payload[2] == 1);
      set_val(slave, payload[9]);
      set_val(fan, (payload[6] >> 4) & 7);
   }
   if (cmd == 0xCB && len >= 2)
   {                            // We get all this from CA
   }
   if (cmd == 0xBD && len >= 29)
   {                            // Looks like temperatures - we assume 0000 is not set
      float t;
      if ((t = (int16_t) (payload[0] + (payload[1] << 8)) / 128.0))
         set_temp(inlet, t);
      if ((t = (int16_t) (payload[2] + (payload[3] << 8)) / 128.0))
         set_temp(home, t);
      if ((t = (int16_t) (payload[4] + (payload[5] << 8)) / 128.0))
         set_temp(liquid, t);
      if ((t = (int16_t) (payload[8] + (payload[9] << 8)) / 128.0))
         set_temp(temp, t);
#if 0
      if (debug)
      {
         jo_t j = jo_object_alloc();    // Debug dump
         jo_litf(j, "a", "%.2f", (int16_t) (payload[0] + (payload[1] << 8)) / 128.0);   // inlet
         jo_litf(j, "b", "%.2f", (int16_t) (payload[2] + (payload[3] << 8)) / 128.0);   // Room
         jo_litf(j, "c", "%.2f", (int16_t) (payload[4] + (payload[5] << 8)) / 128.0);   // liquid
         jo_litf(j, "d", "%.2f", (int16_t) (payload[6] + (payload[7] << 8)) / 128.0);   // same as inlet?
         jo_litf(j, "e", "%.2f", (int16_t) (payload[8] + (payload[9] << 8)) / 128.0);   // Target
         jo_litf(j, "f", "%.2f", (int16_t) (payload[10] + (payload[11] << 8)) / 128.0); // same as inlet
         revk_info("temps", &j);
      }
#endif
   }
   if (cmd == 0xBE && len >= 9)
   {                            // Unknown
   }
}

#undef set_val
#undef set_temp

void daikin_s21_command(uint8_t cmd, uint8_t cmd2, int len, char *payload)
{
   if (!daikin.talking)
      return;                   // Failed
   if (debug && len)
   {
      jo_t j = jo_object_alloc();
      jo_stringf(j, "cmd", "%c%c", cmd, cmd2);
      jo_base16(j, "payload", payload, len);
      jo_stringn(j, "text", (char *) payload, len);
      revk_info("tx", &j);
   }
   uint8_t buf[256],
    temp;
   buf[0] = 2;
   buf[1] = cmd;
   buf[2] = cmd2;
   if (len)
      memcpy(buf + 3, payload, len);
   uint8_t c = 0;
   for (int i = 1; i < 3 + len; i++)
      c += buf[i];
   buf[3 + len] = c;
   buf[4 + len] = 3;
   if (dump)
   {
      jo_t j = jo_object_alloc();
      jo_base16(j, "dump", buf, len + 5);
      revk_info("tx", &j);
   }
   uart_write_bytes(uart, buf, 5 + len);
   // Wait ACK
   len = uart_read_bytes(uart, &temp, 1, 100 / portTICK_PERIOD_MS);
   if (len != 1 || temp != 6)
   {
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      jo_bool(j, "noack", 1);
      if (len)
         jo_stringf(j, "value", "%02X", temp);
      revk_error("comms", &j);
      return;
   }
   if (cmd == 'D')
      return;                   // No response expected
   while (1)
   {
      len = uart_read_bytes(uart, buf, 1, 100 / portTICK_PERIOD_MS);
      if (len != 1)
      {
         daikin.talking = 0;
         jo_t j = jo_object_alloc();
         jo_bool(j, "timeout", 1);
         revk_error("comms", &j);
         return;
      }
      if (*buf == 2)
         break;
   }
   while (len < sizeof(buf))
   {
      if (uart_read_bytes(uart, buf + len, 1, 10 / portTICK_PERIOD_MS) != 1)
      {
         daikin.talking = 0;
         jo_t j = jo_object_alloc();
         jo_bool(j, "timeout", 1);
         revk_error("comms", &j);
         return;
      }
      len++;
      if (buf[len - 1] == 3)
         break;
   }
   // Send ACK
   temp = 6;
   uart_write_bytes(uart, &temp, 1);
   if (dump)
   {
      jo_t j = jo_object_alloc();
      jo_base16(j, "dump", buf, len);
      revk_info("rx", &j);
   }
   // Check checksum
   c = 0;
   for (int i = 1; i < len - 2; i++)
      c += buf[i];
   if (c != buf[len - 2])
   {
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      jo_stringf(j, "badsum", "%02X", c);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   if (len < 5 || buf[0] != 2 || buf[len - 1] != 3 || buf[1] != cmd + 1 || buf[2] != cmd2)
   {
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      if (buf[0] != 2)
         jo_bool(j, "badhead", 1);
      if (buf[1] != cmd + 1 || buf[2] != cmd2)
         jo_bool(j, "mismatch", 1);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   daikin_s21_response(buf[1], buf[2], len - 5, buf + 3);
}

void daikin_command(uint8_t cmd, int len, uint8_t * payload)
{                               // Send a command and get response
   if (!daikin.talking)
      return;                   // Failed
   if (debug && len)
   {
      jo_t j = jo_object_alloc();
      jo_stringf(j, "cmd", "%02X", cmd);
      jo_base16(j, "payload", payload, len);
      revk_info("tx", &j);
   }
   uint8_t buf[256];
   buf[0] = 0x06;
   buf[1] = cmd;
   buf[2] = len + 6;
   buf[3] = 1;
   buf[4] = 0;
   if (len)
      memcpy(buf + 5, payload, len);
   uint8_t c = 0;
   for (int i = 0; i < 5 + len; i++)
      c += buf[i];
   buf[5 + len] = 0xFF - c;
   if (dump)
   {
      jo_t j = jo_object_alloc();
      jo_base16(j, "dump", buf, len + 6);
      revk_info("tx", &j);
   }
   uart_write_bytes(uart, buf, 6 + len);
   // Wait for reply
   len = uart_read_bytes(uart, buf, sizeof(buf), 100 / portTICK_PERIOD_MS);
   if (len <= 0)
   {
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      jo_bool(j, "timeout", 1);
      revk_error("comms", &j);
      return;
   }
   if (dump)
   {
      jo_t j = jo_object_alloc();
      jo_base16(j, "dump", buf, len);
      revk_info("rx", &j);
   }
   // Check checksum
   c = 0;
   for (int i = 0; i < len; i++)
      c += buf[i];
   if (c != 0xFF)
   {
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      jo_stringf(j, "badsum", "%02X", c);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   // Process response
   if (buf[1] == 0xFF)
   {                            // Error report
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      jo_bool(j, "fault", 1);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   if (len < 6 || buf[0] != 0x06 || buf[1] != cmd || buf[2] != len || buf[3] != 1)
   {                            // Basic checks
      daikin.talking = 0;
      jo_t j = jo_object_alloc();
      if (buf[0] != 0x06)
         jo_bool(j, "badhead", 1);
      if (buf[1] != cmd)
         jo_bool(j, "mismatch", 1);
      if (buf[2] != len || len < 6)
         jo_bool(j, "badlen", 1);
      if (buf[3] != 1)
         jo_bool(j, "badform", 1);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   daikin_response(cmd, len - 6, buf + 5);
}

const char *daikin_control(jo_t j)
{                               // Control settings as JSON
   jo_type_t t = jo_next(j);    // Start object
   while (t == JO_TAG)
   {
      const char *err = NULL;
      char tag[20] = "",
          val[20] = "";
      jo_strncpy(j, tag, sizeof(tag));
      t = jo_next(j);
      jo_strncpy(j, val, sizeof(val));
#define	b(name)		if(!strcmp(tag,#name)){if(t!=JO_TRUE&&t!=JO_FALSE)err= "Expecting boolean";else err=daikin_set_v(name,t==JO_TRUE?1:0);}
#define	t(name)		if(!strcmp(tag,#name)){if(t!=JO_NUMBER)err= "Expecting number";else err=daikin_set_t(name,jo_read_float(j));}
#define	e(name,values)	if(!strcmp(tag,#name)){if(t!=JO_STRING)err= "Expecting string";else err=daikin_set_e(name,val);}
      accontrol
#undef	b
#undef	t
#undef	e
          if (err)
      {                         // Error report
         jo_t j = jo_object_alloc();
         jo_string(j, "field", tag);
         jo_string(j, "error", err);
         revk_error("control", &j);
         return err;
      }
      t = jo_skip(j);
   }
   return "";
}

// --------------------------------------------------------------------------------
const char *app_callback(int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{                               // MQTT app callback
   if (client || !prefix || target || strcmp(prefix, prefixcommand))
      return NULL;              // Not for us or not a command from main MQTT
   if (!suffix)
      return daikin_control(j); // General setting
   if (!strcmp(suffix, "reconnect"))
   {
      daikin.talking = 0;       // Disconnect and reconnect
      return "";
   }
   if (!strcmp(suffix, "connect") || !strcmp(suffix, "status"))
      daikin.status_changed = 1;        // Report status on connect
   if (!strcmp(suffix, "control"))
   {                            // Control, e.g. from environmental monitor
      float home = NAN;
      float min = NAN;
      float max = NAN;
      jo_type_t t = jo_next(j); // Start object
      while (t == JO_TAG)
      {
         char tag[20] = "",
             val[20] = "";
         jo_strncpy(j, tag, sizeof(tag));
         t = jo_next(j);
         jo_strncpy(j, val, sizeof(val));
         if (!strcmp(tag, "home"))
            home = strtof(val, NULL);
         if (!strcmp(tag, "min"))
            min = strtof(val, NULL);
         if (!strcmp(tag, "max"))
            max = strtof(val, NULL);
         if (!strcmp(tag, "temp"))
            min = max = strtof(val, NULL);
         t = jo_skip(j);
      }
      xSemaphoreTake(daikin.mutex, portMAX_DELAY);
      daikin.controlvalid = uptime() + controltime;
      daikin.achome = home;
      daikin.acmin = min;
      daikin.acmax = max;
      xSemaphoreGive(daikin.mutex);
      return "";
   }
   jo_t s = jo_object_alloc();
   char value[20] = "";
   jo_strncpy(j, value, sizeof(value));
   // Crude commands - setting one thing
   if (!strcmp(suffix, "on"))
      jo_bool(s, "power", 1);
   if (!strcmp(suffix, "off"))
      jo_bool(s, "power", 0);
   if (!strcmp(suffix, "auto"))
      jo_string(s, "mode", "A");
   if (!strcmp(suffix, "heat"))
      jo_string(s, "mode", "H");
   if (!strcmp(suffix, "cool"))
      jo_string(s, "mode", "C");
   if (!strcmp(suffix, "dry"))
      jo_string(s, "mode", "D");
   if (!strcmp(suffix, "fan"))
      jo_string(s, "mode", "F");
   if (!strcmp(suffix, "low"))
      jo_string(s, "fan", "1");
   if (!strcmp(suffix, "medium"))
      jo_string(s, "fan", "3");
   if (!strcmp(suffix, "high"))
      jo_string(s, "fan", "5");
   if (!strcmp(suffix, "temp"))
      jo_lit(s, "temp", value);
   jo_close(s);
   jo_rewind(s);
   const char *ret = NULL;
   if (jo_next(s) == JO_TAG)
   {
      jo_rewind(s);
      ret = daikin_control(s);
   }
   jo_free(&s);
   return ret;
}

jo_t daikin_status(void)
{
   xSemaphoreTake(daikin.mutex, portMAX_DELAY);
   jo_t j = jo_object_alloc();
#define b(name)         if(daikin.status_known&CONTROL_##name)jo_bool(j,#name,daikin.name);
#define t(name)         if(daikin.status_known&CONTROL_##name)jo_litf(j,#name,"%.1f",daikin.name);
#define e(name,values)  if((daikin.status_known&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
#define s(name,len)     if((daikin.status_known&CONTROL_##name)&&*daikin.name)jo_string(j,#name,daikin.name);
   accontrol acstatus
#undef  b
#undef  t
#undef  e
#undef  s
    jo_bool(j, "control", daikin.controlvalid ? 1 : 0);
   xSemaphoreGive(daikin.mutex);
   return j;
}

// --------------------------------------------------------------------------------
// Web
static void web_head(httpd_req_t * req, const char *title)
{
   httpd_resp_set_type(req, "text/html; charset=utf-8");
   httpd_resp_sendstr_chunk(req, "<meta name='viewport' content='width=device-width, initial-scale=1'>");
   httpd_resp_sendstr_chunk(req, "<html><head><title>");
   if (title)
      httpd_resp_sendstr_chunk(req, title);
   httpd_resp_sendstr_chunk(req, "</title></head><style>"       //
                            "body {font-family:sans-serif;background:#8cf;}"    //
                            ".switch,.box{position:relative;display:inline-block;width:64px;height:34px;margin:3px;}"   //
                            ".switch input,.box input{opacity:0;width:0;height:0;}"     //
                            ".slider,.button{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#ccc;-webkit-transition:.4s;transition:.4s;}"      //
                            ".slider:before{position:absolute;content:\"\";height:26px;width:26px;left:4px;bottom:3px;background-color:white;-webkit-transition:.4s;transition:.4s;}"   //
                            "input:checked+.slider,input:checked+.button{background-color:#12bd20;}"    //
                            "input:checked+.slider:before{-webkit-transform:translateX(30px);-ms-transform:translateX(30px);transform:translateX(30px);}"       //
                            "span.slider:before{border-radius:50%;}"    //
                            "span.slider,span.button{border-radius:34px;padding-top:8px;padding-left:10px;border:1px solid gray;box-shadow:3px 3px 3px #0008;}" //
                            "</style><body><h1>");
   if (title)
      httpd_resp_sendstr_chunk(req, title);
   httpd_resp_sendstr_chunk(req, "</h1>");
}

static esp_err_t web_foot(httpd_req_t * req)
{
   httpd_resp_sendstr_chunk(req, "<hr><address>");
   char temp[20];
   snprintf(temp, sizeof(temp), "%012llX", revk_binid);
   httpd_resp_sendstr_chunk(req, temp);
   httpd_resp_sendstr_chunk(req, "</address></body></html>");
   return ESP_OK;
}

static esp_err_t web_root(httpd_req_t * req)
{
   // TODO cookies
   if (revk_link_down())
      return revk_web_config(req);      // Direct to web set up
   web_head(req, *hostname ? hostname : appname);
   httpd_resp_sendstr_chunk(req, "<form name=F><table id=live>");
   void addh(const char *tag) {
      httpd_resp_sendstr_chunk(req, "<tr><td>");
      httpd_resp_sendstr_chunk(req, tag);
      httpd_resp_sendstr_chunk(req, "</td><td>");
   }
   void add(const char *tag, const char *field, ...) {
      addh(tag);
      if (daikin.online)
      {
         va_list ap;
         va_start(ap, field);
         while (1)
         {
            tag = va_arg(ap, char *);
            if (!tag)
               break;
            const char *value = va_arg(ap, char *);
            httpd_resp_sendstr_chunk(req, "<label class=box><input type=radio name=");
            httpd_resp_sendstr_chunk(req, field);
            httpd_resp_sendstr_chunk(req, " value=");
            httpd_resp_sendstr_chunk(req, value);
            httpd_resp_sendstr_chunk(req, " id=");
            httpd_resp_sendstr_chunk(req, field);
            httpd_resp_sendstr_chunk(req, value);
            httpd_resp_sendstr_chunk(req, " onchange=\"if(this.checked)w('");
            httpd_resp_sendstr_chunk(req, field);
            httpd_resp_sendstr_chunk(req, "','");
            httpd_resp_sendstr_chunk(req, value);
            httpd_resp_sendstr_chunk(req, "');\"><span class=button>");
            httpd_resp_sendstr_chunk(req, tag);
            httpd_resp_sendstr_chunk(req, "</span></label>");
         }
         va_end(ap);
      }
      httpd_resp_sendstr_chunk(req, "</td></tr>");
   }
   void addb(const char *tag, const char *field) {
      addh(tag);
      httpd_resp_sendstr_chunk(req, "<label class=switch><input type=checkbox id=");
      httpd_resp_sendstr_chunk(req, field);
      httpd_resp_sendstr_chunk(req, " onchange=\"w('");
      httpd_resp_sendstr_chunk(req, field);
      httpd_resp_sendstr_chunk(req, "',this.checked);\"><span class=slider></span></label></td></tr>");
   }
   void addt(const char *tag, const char *field, uint8_t change) {
      addh(tag);
      if (change)
      {
         void pm(const char *d) {
            httpd_resp_sendstr_chunk(req, "<label class=box><input type=checkbox onchange=\"if(this.checked)w('");
            httpd_resp_sendstr_chunk(req, field);
            httpd_resp_sendstr_chunk(req, "',");
            httpd_resp_sendstr_chunk(req, field);
            httpd_resp_sendstr_chunk(req, d);
            httpd_resp_sendstr_chunk(req, "0.5);this.checked=false;\"><span class=button>");
            httpd_resp_sendstr_chunk(req, d);
            httpd_resp_sendstr_chunk(req, "</span></label>");
         }
         pm("-");
         pm("+");
      }
      httpd_resp_sendstr_chunk(req, "<label class=box><input type=checkbox><span class=button id=");
      httpd_resp_sendstr_chunk(req, field);
      httpd_resp_sendstr_chunk(req, " onchange=\"this.checked=false;\"></span></label>");
      httpd_resp_sendstr_chunk(req, "</td></tr>");
   }
   addb("Power", "power");
   add("Mode", "mode", "Auto", "A", "Heat", "H", "Cool", "C", "Dry", "D", "Fan", "F", NULL);
   if (fanstep == 1)
      add("Fan", "fan", "Night", "Q", "1", "1", "2", "2", "3", "3", "4", "4", "5", "5", "Auto", "A", NULL);
   else
      add("Fan", "fan", "Low", "1", "Medium", "3", "High", "5", NULL);
   addt("Target", "temp", 1);
   addt("Temp", "home", 0);
   if (daikin.status_known & CONTROL_powerful)
      addb("Powerful", "powerful");
   if (daikin.status_known & CONTROL_econo)
      addb("Econo", "econo");
   if (daikin.status_known & CONTROL_swingv)
      addb("Swing&nbsp;Vert", "swingv");
   if (daikin.status_known & CONTROL_swingh)
      addb("Swing&nbsp;Horz", "swingh");
   httpd_resp_sendstr_chunk(req, "</table></form>");
   httpd_resp_sendstr_chunk(req, "<p id=slave style='display:none'>Another unit is controlling the mode, so this unit is not operating at present.</p>");
   httpd_resp_sendstr_chunk(req, "<p id=control style='display:none'>Automatic control means some functions are limited.</p>");
   httpd_resp_sendstr_chunk(req, "<p id=offline style='display:none'>System is off line.</p>");
   httpd_resp_sendstr_chunk(req, "<p><a href='wifi'>WiFi Setup</a></p>");
   httpd_resp_sendstr_chunk(req, "<script>"     //
                            "var ws = new WebSocket('ws://'+window.location.host+'/status');"   //
                            "var temp=0;"       //
                            "function b(n,v){var d=document.getElementById(n);if(d)d.checked=v;}"       //
                            "function h(n,v){var d=document.getElementById(n);if(d)d.style.display=v?'block':'none';}"  //
                            "function s(n,v){var d=document.getElementById(n);if(d)d.textContent=v;}"   //
                            "function e(n,v){var d=document.getElementById(n+v);if(d)d.checked=true;}"  //
                            "function w(n,v){var m=new Object();m[n]=v;ws.send(JSON.stringify(m))}"     //
                            "ws.onclose=function(v){document.getElementById('live').style.visibility='hidden';};"       //
                            "ws.onmessage=function(v){" //
                            "o=JSON.parse(v.data);"     //
                            "b('power',o.power);"       //
                            "h('offline',!o.online);"   //
                            "h('control',o.control);"   //
                            "h('slave',o.slave);"       //
                            "b('powerful',o.powerful);" //
                            "b('swingh',o.swingh);"     //
                            "b('swingv',o.swingv);"     //
                            "b('econo',o.econo);"       //
                            "e('mode',o.mode);" //
                            "s('home',(o.home+'℃').replace('.5','½'));"      //
                            "s('temp',(o.temp+'℃').replace('.5','½'));"      //
                            "e('fan',o.fan);"   //
                            "temp=o.temp;"      //
                            "};"        //
                            "setInterval(function() {ws.send('');},1000);"      //
                            "</script>");
   httpd_resp_sendstr_chunk(req, NULL);
   return web_foot(req);;
}

static esp_err_t web_status(httpd_req_t * req)
{                               // Web socket status report
   // TODO cookies
   int fd = httpd_req_to_sockfd(req);
   void wsend(jo_t * jp) {
      char *js = jo_finisha(jp);
      if (js)
      {
         httpd_ws_frame_t ws_pkt;
         memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
         ws_pkt.payload = (uint8_t *) js;
         ws_pkt.len = strlen(js);
         ws_pkt.type = HTTPD_WS_TYPE_TEXT;
         httpd_ws_send_frame_async(req->handle, fd, &ws_pkt);
         free(js);
      }
   }
   esp_err_t status(void) {
      jo_t j = daikin_status();
      wsend(&j);
      return ESP_OK;
   }
   if (req->method == HTTP_GET)
      return status();          // Send status on initial connect
   // received packet
   httpd_ws_frame_t ws_pkt;
   uint8_t *buf = NULL;
   memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
   ws_pkt.type = HTTPD_WS_TYPE_TEXT;
   esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
   if (ret)
      return ret;
   if (!ws_pkt.len)
      return status();          // Empty string
   buf = calloc(1, ws_pkt.len + 1);
   if (!buf)
      return ESP_ERR_NO_MEM;
   ws_pkt.payload = buf;
   ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
   if (!ret)
   {
      jo_t j = jo_parse_mem(buf, ws_pkt.len);
      if (j)
      {
         daikin_control(j);
         jo_free(&j);
      }
   }
   free(buf);
   return status();
}

// --------------------------------------------------------------------------------
// Main
void app_main()
{
   daikin.mutex = xSemaphoreCreateMutex();
   daikin.status_known = CONTROL_online;
   daikin.achome = NAN;
   daikin.acmin = NAN;
   daikin.acmax = NAN;
   revk_boot(&app_callback);
#define io(n,d)           revk_register(#n,0,sizeof(n),&n,"- "#d,SETTING_SET|SETTING_BITFIELD);
#define b(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN);
#define bl(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN|SETTING_LIVE);
#define u32(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s8(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_SIGNED);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define u8l(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_LIVE);
#define s(n) revk_register(#n,0,0,&n,NULL,0);
   settings
#undef io
#undef u32
#undef s8
#undef u8
#undef u8l
#undef b
#undef bl
#undef s
       revk_start();
   {                            // Init uart
      esp_err_t err = 0;
      // Init UART for Mobile
      uart_config_t uart_config = {
         .baud_rate = s21 ? 2400 : 9600,
         .data_bits = UART_DATA_8_BITS,
         .parity = UART_PARITY_EVEN,
         .stop_bits = s21 ? UART_STOP_BITS_2 : UART_STOP_BITS_1,
         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      };
      if (!err)
         err = uart_param_config(uart, &uart_config);
      if (!err)
         err = uart_set_pin(uart, port_mask(tx), port_mask(rx), -1, -1);
      if (!err && ((tx & PORT_INV) || (rx & PORT_INV)))
         err = uart_set_line_inverse(uart, ((rx & PORT_INV) ? UART_SIGNAL_RXD_INV : 0) | ((tx & PORT_INV) ? UART_SIGNAL_TXD_INV : 0));
      if (!err)
         err = uart_driver_install(uart, 1024, 0, 0, NULL, 0);
      if (!err)
         err = uart_set_rx_full_threshold(uart, 1);
      if (err)
      {
         jo_t j = jo_object_alloc();
         jo_string(j, "error", "Failed to uart");
         jo_int(j, "uart", uart);
         jo_int(j, "gpio", port_mask(rx));
         jo_string(j, "description", esp_err_to_name(err));
         revk_error("uart", &j);
         return;
      }
   }

   // Web interface
   httpd_config_t config = HTTPD_DEFAULT_CONFIG();
   if (!httpd_start(&webserver, &config))
   {
      {
         httpd_uri_t uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = web_root,
            .user_ctx = NULL
         };
         REVK_ERR_CHECK(httpd_register_uri_handler(webserver, &uri));
      }
      {
         httpd_uri_t uri = {
            .uri = "/wifi",
            .method = HTTP_GET,
            .handler = revk_web_config,
            .user_ctx = NULL
         };
         REVK_ERR_CHECK(httpd_register_uri_handler(webserver, &uri));
      }
      {
         httpd_uri_t uri = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = web_status,
            .is_websocket = true,
         };
         REVK_ERR_CHECK(httpd_register_uri_handler(webserver, &uri));
      }

      revk_web_config_start(webserver);
   }

   while (1)
   {                            // Main loop
      sleep(1);
      uart_flush(uart);         // Clean start
      daikin.talking = 1;
      if (!s21)
      {                         // Startup
         daikin_command(0xAA, 1, (uint8_t[])
                        {
                        0x01}
         );
         daikin_command(0xBA, 0, NULL);
         daikin_command(0xBB, 0, NULL);
      }
      if (daikin.online != daikin.talking)
      {
         daikin.online = daikin.talking;
         daikin.status_changed = 1;
      }
      do
      {                         // Polling loop
         if (s21)
         {                      // Older S21
            char temp[5];
            daikin_s21_command('F', '2', 0, NULL);
            daikin_s21_command('F', '1', 0, NULL);
            daikin_s21_command('F', '3', 0, NULL);
            daikin_s21_command('F', '4', 0, NULL);
            daikin_s21_command('F', '5', 0, NULL);
            daikin_s21_command('F', '8', 0, NULL);
            daikin_s21_command('F', '9', 0, NULL);
            daikin_s21_command('F', '6', 0, NULL);
            daikin_s21_command('F', '7', 0, NULL);
            daikin_s21_command('F', 'B', 0, NULL);
            daikin_s21_command('F', 'G', 0, NULL);
            daikin_s21_command('F', 'K', 0, NULL);
            daikin_s21_command('F', 'M', 0, NULL);
            daikin_s21_command('F', 'N', 0, NULL);
            daikin_s21_command('F', 'P', 0, NULL);
            daikin_s21_command('F', 'Q', 0, NULL);
            daikin_s21_command('F', 'S', 0, NULL);
            daikin_s21_command('F', 'T', 0, NULL);
            daikin_s21_command('F', 'U', 2, "02");
            daikin_s21_command('F', 'U', 2, "04");
            daikin_s21_command('R', 'H', 0, NULL);
            daikin_s21_command('R', 'N', 0, NULL);
            daikin_s21_command('R', 'I', 0, NULL);
            daikin_s21_command('R', 'a', 0, NULL);
            daikin_s21_command('R', 'X', 0, NULL);
            daikin_s21_command('R', 'D', 0, NULL);
            daikin_s21_command('R', 'L', 0, NULL);
            if (daikin.control_changed & (CONTROL_power | CONTROL_mode | CONTROL_temp | CONTROL_fan))
            {                   // D1
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               temp[0] = daikin.power ? '1' : '0';
               temp[1] = ("64310002"[daikin.mode]);
               temp[2] = 0x40 + lroundf((daikin.temp - 18.0) * 2);
               temp[3] = ("A34567Q"[daikin.fan]);
               daikin_s21_command('D', '1', 4, temp);
               xSemaphoreGive(daikin.mutex);
            }
            if (daikin.control_changed & (CONTROL_swingh | CONTROL_swingv))
            {                   // D5
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               temp[0] = '0' + (daikin.swingh ? 2 : 0) + (daikin.swingv ? 1 : 0) + (daikin.swingh && daikin.swingv ? 4 : 0);
               temp[1] = (daikin.swingh || daikin.swingv ? '?' : '0');
               temp[2] = '0';
               temp[3] = '0';
               daikin_s21_command('D', '5', 4, temp);
               xSemaphoreGive(daikin.mutex);
            }
            if (daikin.control_changed & CONTROL_powerful)
            {                   // D6
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               temp[0] = '0' + (daikin.powerful ? 2 : 0);
               temp[1] = '0';
               temp[2] = '0';
               temp[3] = '0';
               daikin_s21_command('D', '6', 4, temp);
               xSemaphoreGive(daikin.mutex);
            }
            if (daikin.control_changed & CONTROL_econo)
            {                   // D7
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               temp[0] = '0';
               temp[1] = '0' + (daikin.econo ? 2 : 0);
               temp[2] = '0';
               temp[3] = '0';
               daikin_s21_command('D', '7', 4, temp);
               xSemaphoreGive(daikin.mutex);
            }
         } else
         {                      // Newer protocol
            //daikin_command(0xB7, 0, NULL);       // Not sure this is actually meaningful
            daikin_command(0xBD, 0, NULL);
            daikin_command(0xBE, 0, NULL);
            uint8_t ca[17] = { };
            uint8_t cb[2] = { };
            if (daikin.control_changed)
            {
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               ca[0] = 2 + daikin.power;
               ca[1] = 0x10 + daikin.mode;
               if (daikin.mode >= 1 && daikin.mode <= 3)
               {                // Temp
                  int t = lroundf(daikin.temp * 10);
                  ca[3] = t / 10;
                  ca[4] = 0x80 + (t % 10);
               }
               if (daikin.mode == 1 || daikin.mode == 2)
                  cb[0] = daikin.mode;
               else
                  cb[0] = 6;
               cb[1] = 0x80 + ((daikin.fan & 7) << 4);
               xSemaphoreGive(daikin.mutex);
            }
            daikin_command(0xCA, sizeof(ca), ca);
            daikin_command(0xCB, sizeof(cb), cb);
         }
         if (!daikin.control_changed && daikin.status_changed)
         {
            daikin.status_changed = 0;
            jo_t j = daikin_status();
            revk_state("status", &j);
         }
         if (!daikin.control_changed)
            daikin.control_count = 0;
         else if (daikin.control_count++ > 10)
         {                      // Tried a lot
            // Report failed settings
            jo_t j = jo_object_alloc();
#define b(name)         if(daikin.control_changed&CONTROL_##name)jo_bool(j,#name,daikin.name);
#define t(name)         if(daikin.control_changed&CONTROL_##name)jo_litf(j,#name,"%.1f",daikin.name);
#define e(name,values)  if((daikin.control_changed&CONTROL_##name)&&daikin.name<sizeof(CONTROL_##name##_VALUES)-1)jo_stringf(j,#name,"%c",CONTROL_##name##_VALUES[daikin.name]);
            accontrol
#undef  b
#undef  t
#undef  e
                revk_error("failed-set", &j);
            daikin.control_changed = 0; // Give up on changes
         }
         uint32_t now = uptime();
         revk_blink(0, 0, !daikin.online ? "M" : !daikin.power ? "Y" : daikin.heat ? "R" : "B");
         if (daikin.power && daikin.controlvalid && !daikin.control_changed)
         {                      // Local auto controls
            if (now > daikin.controlvalid)
            {                   // End of auto mode
               daikin.controlvalid = 0;
               daikin_set_e(mode, "A");
               if (!isnan(daikin.acmin) && !isnan(daikin.acmax))
                  daikin_set_t(temp, (daikin.acmin + daikin.acmax) / 2);
            } else
            {                   // Auto mode
               // Get the settings atomically
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               float min = daikin.acmin;
               float max = daikin.acmax;
               float current = daikin.achome;
               xSemaphoreGive(daikin.mutex);
               uint8_t hot = daikin.heat;       // Are we in heating mode?
               // Current temperature
               if (isnan(current))      // We don't have one, so treat as same as A/C view of current temp
                  current = daikin.home;
               // What the A/C is using as current temperature
               float reference = daikin.home;   // Reference for what we set - we are assuming the A/C is using this (what if it is not?)
               // Sensible limits in case some not set
               if (isnan(min))
                  min = 16;
               if (isnan(max))
                  max = 32;
               // Apply hysteresis
               if (hot)
                  max += switch10 / 10.0;       // Overshoot for switching (heating)
               else
                  min -= switch10 / 10.0;       // Overshoot for switching (cooling)
               if (daikin.mode == 3)
                  daikin_set_e(mode, hot ? "H" : "C");  // Out of auto
               // What do we want to set to
               float set = min + reference - current;   // Where we will set the temperature
               // Consider beyond limits - remember the limits have hysteresis applied
               if (min > current)
               {                // Below min means we should be heating, if we are not then min was already reduced so time to switch to heating as well.
                  if (daikin.slave || ((!daikin.acswitch || daikin.acswitch + switchtime < now) && (!daikin.acapproaching || daikin.acapproaching + switchdelay < now)))
                  {             // Can we switch to heating - time limits applied
                     daikin.acswitch = now;     // Switched
                     daikin_set_e(mode, "H");
                  }
                  set = max + reference - current + heatbias10 / 10.0;  // Ensure heating by applying A/C offset to force it
               } else if (max < current)
               {                // Above max means we should be cooling, if we are not then max was already increased so time to switch to cooling as well
                  if (daikin.slave || ((!daikin.acswitch || daikin.acswitch + switchtime < now) && (!daikin.acapproaching || daikin.acapproaching + switchdelay < now)))
                  {             // Can we switch to cooling - time limits applied
                     daikin.acswitch = now;     // Switched
                     daikin_set_e(mode, "C");
                  }
                  set = max + reference - current - coolbias10 / 10.0;  // Ensure cooling by applying A/C offset to force it
               } else if (hot)
                  set = min + reference - current - heatbias10 / 10.0;  // Heating mode but apply negative offset to not actually heat any more than this
               else
                  set = max + reference - current + coolbias10 / 10.0;  // Cooling mode but apply positive offset to not actually cool any more than this
               // Check if we are approaching target or beyond it
               if ((hot && current <= min) || (!hot && current >= max))
               {                // Approaching target - if we have been doing this too long, increase the fan
                  daikin.acapproaching = now;
                  if (fanstep && fantime && daikin.acbeyond + fantime < now && daikin.fan && daikin.fan < 5)
                  {
                     daikin.acbeyond = now;     // Delay next fan
                     daikin_set_v(fan, daikin.fan + fanstep);
                  }
               } else
               {                // Beyond target, but not yet switched - if we have been here too long and not switched we may reduce fan
                  daikin.acbeyond = now;
                  if (fanstep && fantime && daikin.acapproaching + fantime < now && daikin.fan && daikin.fan > 1)
                  {
                     daikin.acapproaching = now;        // Delay next fan
                     daikin_set_v(fan, daikin.fan - fanstep);
                  }
               }
               // Limit settings to acceptable values
               if (set < 16)
                  set = 16;
               if (set > 32)
                  set = 32;
               daikin_set_t(temp, set); // Apply temperature setting
            }
         }
         if (reporting && !revk_link_down())
         {                      // Environment logging
            time_t clock = time(0);
            static time_t last = 0;
            if (clock / reporting != last / reporting)
            {
               last = clock;
               jo_t j = jo_object_alloc();
               {                // Timestamp
                  struct tm tm;
                  gmtime_r(&clock, &tm);
                  jo_stringf(j, "ts", "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
               }
               uint8_t hot = daikin.heat;
               xSemaphoreTake(daikin.mutex, portMAX_DELAY);
               uint32_t valid = daikin.controlvalid;
               float min = daikin.acmin;
               float max = daikin.acmax;
               float home = daikin.achome;
               xSemaphoreGive(daikin.mutex);
               float temp = home;
               if (now > reporting + 30 && (!valid || isnan(temp)))
                  temp = daikin.home;
               if (!isnan(temp))
                  jo_litf(j, "temp", "%.3f", temp);
               jo_bool(j, "heat", hot && daikin.power && !daikin.slave);
               if (valid)
               {                // Our control...
                  float target = (hot ? min : max);
                  jo_litf(j, "temp-target", "%.3f", target);
               } else if (now > reporting + 30 && !isnan(daikin.temp))
                  jo_litf(j, "temp-target", "%.1f", daikin.temp);       // reference temp
               char topic[100];
               snprintf(topic, sizeof(topic), "state/Env/%s/data", hostname);
               revk_mqtt_send_clients(NULL, 1, topic, &j, 1);
            }
         }
      }
      while (daikin.talking);
   }
}
