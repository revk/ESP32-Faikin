/* BlueCoinT app */
/* Copyright ©2022 - 2023 Adrian Kennard, Andrews & Arnold Ltd.See LICENCE file for details .GPL 3.0 */

static __attribute__((unused))
const char TAG[] = "BlueCoinT";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "esp_bt.h"
#include "host/util/util.h"
#include "console/console.h"
#include <driver/gpio.h>
#include "ela.h"

#ifndef	CONFIG_SOC_BLE_SUPPORTED
#error	You need CONFIG_SOC_BLE_SUPPORTED
#endif

#define	MAXGPIO	36
#define BITFIELDS "-"
#define PORT_INV 0x40
#define port_mask(p) ((p)&0x3F)

httpd_handle_t webserver = NULL;

#define	settings		\
	u32(missingtime,30)	\
	u32(reporting,60)	\
	u8(temprise,50)		\

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
const char *app_callback(int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{
   if (j && target && !strcmp(prefix, "info") && !strcmp(suffix, "report") && strlen(target) <= 12)
   {                            // Other reports
      ble_addr_t a = {.type = BLE_ADDR_PUBLIC };
      if (jo_find(j, "rssi"))
      {
         int rssi = jo_read_int(j);
         if (jo_find(j, "address"))
         {
            uint8_t add[18] = { 0 };
            jo_strncpy(j, add, sizeof(add));
            for (int i = 0; i < 6; i++)
               a.val[5 - i] = (((isalpha(add[i * 3]) ? 9 : 0) + (add[i * 3] & 0xF)) << 4) + (isalpha(add[i * 3 + 1]) ? 9 : 0) + (add[i * 3 + 1] & 0xF);
            ela_t *d = ela_find(&a, 0);
            if (d)
            {
               int c = strcmp(target, d->better);
               if (!c || !*d->better || rssi > d->rssi || (rssi == d->rssi && c > 0))
               {                // Record best
                  if (c)
                  {
                     strcpy(d->better, target);
                     ESP_LOGI(TAG, "Found possibly better \"%s\" %s %d", d->name, target, rssi);
                  }
               }
               d->betterrssi = rssi;
               d->lastbetter = uptime();
            }
         }
      }
   }
   if (client || !prefix || target || strcmp(prefix, prefixcommand) || !suffix)
      return NULL;              //Not for us or not a command from main MQTT
   if (!strcmp(suffix, "connect"))
   {
      lwmqtt_subscribe(revk_mqtt(0), "info/BlueCoinT/#");
   }
   if (!strcmp(suffix, "shutdown"))
      httpd_stop(webserver);
   return NULL;
}

/* MAIN */
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

   revk_wait_mqtt(60);

   ela_run();

   /* main loop */
   while (1)
   {
      usleep(100000);
      uint32_t now = uptime();
      ela_expire(missingtime);
      for (ela_t * d = ela; d; d = d->next)
         if (*d->better && d->lastbetter + reporting * 3 / 2 < now)
            *d->better = 0;     // Not seeing better
      // Reporting
      for (ela_t * d = ela; d; d = d->next)
         if (!d->missing && (d->lastreport + reporting <= now || d->tempreport + temprise < d->temp))
         {
            d->lastreport = now;
            d->tempreport = d->temp;
            if (*d->better && (d->betterrssi > d->rssi || (d->betterrssi == d->rssi && strcmp(d->better, revk_id) > 0)))
            {
               ESP_LOGI(TAG, "Not reporting \"%s\" %d as better %s %d", d->name, d->rssi, d->better, d->betterrssi);
               continue;
            }
            jo_t j = jo_object_alloc();
            jo_string(j, "address", ble_addr_format(&d->addr));
            jo_string(j, "name", d->name);
            if (d->temp < 0)
               jo_litf(j, "temp", "-%d.%02d", (-d->temp) / 100, (-d->temp) % 100);
            else
               jo_litf(j, "temp", "%d.%02d", d->temp / 100, d->temp % 100);
            if (d->bat)
               jo_litf(j, "bat", "%d", d->bat);
            if (d->volt)
               jo_litf(j, "voltage", "%d.%03d", d->volt / 1000, d->volt % 1000);
            jo_int(j, "rssi", d->rssi);
            revk_info("report", &j);
            ESP_LOGI(TAG, "Report %s \"%s\" %d (%s %d)", ble_addr_format(&d->addr), d->name, d->rssi, d->better, d->betterrssi);
         }

      ela_clean();
   }
   return;
}
