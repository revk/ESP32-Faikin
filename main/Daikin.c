/* Daikin app */
/* Copyright ©2022 Adrian Kennard, Andrews & Arnold Ltd. See LICENCE file for details .GPL 3.0 */

static __attribute__((unused))
const char TAG[] = "Daikin";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include <driver/gpio.h>
#include <driver/uart.h>

#define PORT_INV 0x40
#define port_mask(p) ((p)&63)

// Settings
#define	settings		\
	b(debug)	\
	u8(uart,1)	\
	io(tx,)	\
	io(rx,)	\

#define u32(n,d)        uint32_t n;
#define s8(n,d) int8_t n;
#define u8(n,d) uint8_t n;
#define b(n) uint8_t n;
#define s(n) char * n;
#define io(n,d)           uint8_t n;
settings
#undef io
#undef u32
#undef s8
#undef u8
#undef b
#undef s
const char modes[] = "FHCA456D";
// Status of unit
struct {                        // The current status based on messages received
   char model[20];              // Model number of attached unit
   char mode;                   // Current mode
   char fan;                    // Current fan speed
   uint8_t on:1;                // Currently on
   uint8_t talking:1;           // Currently communicating
   uint8_t changed:1;           // Has changed, report
} status;

struct {                        // The command status we wish to send
   float temp;                  // Target temperature
   char mode;                   // Mode to set ('A'=auto, 'H'=heat, 'C'=cool, 'D'=Dry, 'F'=Fan)
   char fan;                    // Fan speed '1' to '5'
   uint8_t on:1;                // Switched on
   uint8_t changed:1;           // Send changes
} command;

void daikin_response(uint8_t cmd, int len, uint8_t * payload)
{
   // TODO
}

void daikin_command(uint8_t cmd, int len, uint8_t * payload)
{
   if (!status.talking)
      return;                   // Failed
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
   if (debug)
   {
      jo_t j = jo_object_alloc();
      jo_base16(j, "tx", buf, len + 6);
      revk_info("debug", &j);
   }
   uart_write_bytes(uart, buf, 6 + len);
   // Wait for reply
   len = uart_read_bytes(uart, buf, sizeof(buf), 100 / portTICK_PERIOD_MS);
   if (len <= 0)
   {
      status.talking = 0;
      jo_t j = jo_object_alloc();
      jo_bool(j, "timeout", 1);
      revk_error("comms", &j);
      return;
   }
   if (debug)
   {
      jo_t j = jo_object_alloc();
      jo_base16(j, "rx", buf, len);
      revk_info("debug", &j);
   }
   // Check checksum
   c = 0;
   for (int i = 0; i < len; i++)
      c += buf[i];
   if (c != 0xFF)
   {
      status.talking = 0;
      jo_t j = jo_object_alloc();
      jo_bool(j, "badsum", 1);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   // Process response
   if (buf[1] == 0xFF)
   {                            // Error report
      status.talking = 0;
      jo_t j = jo_object_alloc();
      jo_bool(j, "fault", 1);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   if (buf[0] != 0x06 || buf[1] != cmd || buf[2] != len || buf[3] != 1)
   {                            // Basic checks
      status.talking = 0;
      jo_t j = jo_object_alloc();
      if (buf[0] != 0x06)
         jo_bool(j, "badhead", 1);
      if (buf[1] != cmd)
         jo_bool(j, "mismatch", 1);
      if (buf[2] != len)
         jo_bool(j, "badlen", 1);
      if (buf[3] != 1)
         jo_bool(j, "badform", 1);
      jo_base16(j, "data", buf, len);
      revk_error("comms", &j);
      return;
   }
   daikin_response(cmd, len - 6, buf + 5);
}

// --------------------------------------------------------------------------------
const char *app_callback(int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{                               // MQTT app callback
   if (client || !prefix || target || strcmp(prefix, prefixcommand) || !suffix)
      return NULL;              //Not for us or not a command from main MQTT
   char value[1000];
   int len = 0;
   if (j)
   {
      len = jo_strncpy(j, value, sizeof(value));
      if (len < 0)
         return "Expecting JSON string";
      if (len > sizeof(value))
         return "Too long";
   }
   if (!strcmp(suffix, "reconnect"))
   {
      status.talking = 0;       // Disconnect and reconnect
      return "";
   }
   if (!strcmp(suffix, "on"))
   {
      command.on = 1;
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "off"))
   {
      command.on = 0;
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "auto"))
   {
      command.mode = 'A';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "heat"))
   {
      command.mode = 'H';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "cool"))
   {
      command.mode = 'C';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "dry"))
   {
      command.mode = 'D';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "fan"))
   {
      command.mode = 'F';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "low"))
   {
      command.fan = '1';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "medium"))
   {
      command.fan = '3';
      command.changed = 1;
      return "";
   }
   if (!strcmp(suffix, "high"))
   {
      command.fan = '5';
      command.changed = 1;
      return "";
   }

   return NULL;
}

void app_main()
{
   revk_boot(&app_callback);
#define io(n,d)           revk_register(#n,0,sizeof(n),&n,"- "#d,SETTING_SET|SETTING_BITFIELD);
#define b(n) revk_register(#n,0,sizeof(n),&n,NULL,SETTING_BOOLEAN);
#define u32(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s8(n,d) revk_register(#n,0,sizeof(n),&n,#d,SETTING_SIGNED);
#define u8(n,d) revk_register(#n,0,sizeof(n),&n,#d,0);
#define s(n) revk_register(#n,0,0,&n,NULL,0);
   settings
#undef io
#undef u32
#undef s8
#undef u8
#undef b
#undef s
       revk_start();
   {                            // Init uart
      esp_err_t err = 0;
      // Init UART for Mobile
      uart_config_t uart_config = {
         .baud_rate = 9600,
         .data_bits = UART_DATA_8_BITS,
         .parity = UART_PARITY_EVEN,
         .stop_bits = UART_STOP_BITS_1,
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

   while (1)
   {                            // Main loop
      sleep(1);
      status.talking = 1;
      // Startup
      daikin_command(0xAA, 1, (uint8_t[])
                     {
                     0x01}
      );
      daikin_command(0xBA, 0, NULL);
      daikin_command(0xBB, 0, NULL);

      while (status.talking)
      {                         // Polling loop
         daikin_command(0xB7, 0, NULL);
         daikin_command(0xBD, 0, NULL);
         daikin_command(0xBE, 0, NULL);
         uint8_t ca[17] = { };
         uint8_t cb[2] = { };
         if (command.changed)
         {
            command.changed = 0;
            ca[0] = 2 + command.on;
            ca[1] = 0x40 + strchr(modes, command.mode) - modes;
            if (command.mode == 'H')
               cb[0] = 1;
            else if (command.mode == 'C')
               cb[0] = 2;
            else
               cb[0] = 6;
            cb[2] = 0x80 + ((command.fan & 7) << 4);
         }
         daikin_command(0xCA, sizeof(ca), ca);
         daikin_command(0xCB, sizeof(cb), cb);
      }
      if (status.changed)
      {
         status.changed = 0;
         // TODO report
      }
   }
}
