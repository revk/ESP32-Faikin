/* Daikin app */
/* Copyright ©2022 Adrian Kennard, Andrews & Arnold Ltd. See LICENCE file for details .GPL 3.0 */

static const char TAG[] = "Daikin";

#include "revk.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include "iec18004.h"
#include <driver/gpio.h>
#include <driver/uart.h>

#define	MAXGPIO	36
#define BITFIELDS "-"
#define PORT_INV 0x40
#define port_mask(p) ((p)&63)
static uint8_t input[MAXGPIO];  //Input GPIOs
static uint8_t output[MAXGPIO]; //Output GPIOs
static uint32_t outputmark[MAXGPIO];    //Output mark time(ms)
static uint32_t outputspace[MAXGPIO];   //Output mark time(ms)
static uint8_t power[MAXGPIO];  //Fixed output GPIOs
int holding = 0;

// Dynamic
static uint64_t volatile outputbits = 0;        // Requested output
static uint64_t volatile outputraw = 0; // Current output
static uint64_t volatile outputoverride = 0;    // Override output (e.g. PWM)
static uint32_t outputremaining[MAXGPIO] = { }; //Output remaining time(ms)
static uint32_t outputcount[MAXGPIO] = { };     //Output count

#define	settings		\
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

const char *app_callback(int client, const char *prefix, const char *target, const char *suffix, jo_t j)
{
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

   return NULL;
}

void app_main()
{
   time_t now = time(0);
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
   while(1)
   {
	   // TODO
	   sleep(1);
   }
}
