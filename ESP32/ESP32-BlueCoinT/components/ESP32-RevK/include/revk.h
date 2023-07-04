// Include file for revk.c

#ifndef	REVK_H
#define	REVK_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#ifdef	CONFIG_REVK_MQTT
#include "lwmqtt.h"
#endif
#ifdef	CONFIG_REVK_APMODE
#include "esp_http_server.h"
#endif
#include "jo.h"

#include "esp8266_httpd_compat.h"
#include "esp8266_netif_compat.h"

// Types

        // MQTT rx callback: Do not consume jo_t! Return error or NULL. Returning "" means handled the command with no error.
        // You will want to check prefix matches prefixcommand
        // Target can be something not for us if extra subscribes done, but if it is for us, or internal, it is passes as NULL
        // Suffix can be NULL
typedef const char *app_callback_t(int client, const char *prefix, const char *target, const char *suffix, jo_t);
typedef uint8_t mac_t[6];

// Data
extern const char *revk_app;    // App name
extern const char *revk_version;        // App version
extern const char revk_build_suffix[];  // App build suffix
extern char revk_id[13];        // Chip ID hex (from MAC)
extern mac_t revk_mac;          // Our mac
extern uint64_t revk_binid;     // Chip ID binary
extern char *prefixcommand;
extern char *prefixsetting;
extern char *prefixstate;
extern char *prefixevent;
extern char *prefixinfo;
extern char *prefixerror;
extern char *appname;
extern char *hostname;
extern char *nodename;          // Node name
extern esp_netif_t *sta_netif;
extern esp_netif_t *ap_netif;

jo_t jo_make(const char *nodename);     // Start object with node name

typedef struct {                // Dynamic binary data
   uint16_t len;
   uint8_t data[];
} revk_bindata_t;

#define freez(x) do{if(x){free((void*)x);x=NULL;}}while(0)      // Just useful - yes free(x) is valid when x is NULL, but this sets x NULL as a result as well

// Calls
void revk_boot(app_callback_t * app_callback);
void revk_start(void);
// Register a setting, call from init (i.e. this is not expecting to be thread safe) - sets the value when called and on revk_setting/MQTT changes
// Note, a setting that is SECRET that is a root name followed by sub names creates parent/child. Only shown if parent has value or default value (usually overlap a key child)
void revk_register(const char *name,    // Setting name (note max 15 characters inc any number suffix)
                   uint8_t array,       // If non zero then settings are suffixed numerically 1 to array
                   uint16_t size,       // Base setting size, -8/-4/-2/-1 signed, 1/2/4/8 unsigned, 0=null terminated string.
                   void *data,  // The setting itself (for string this points to a char* pointer)
                   const char *defval,  // default value (default value text, or bitmask[space]default)
                   int flags);      // Setting flags
#define	SETTING_LIVE		1       // Setting update live (else reboots shortly after any change)
#define	SETTING_BINDATA		2       // Binary block (text is base64 or hex) rather than numeric. Fixed is just the data (malloc), variable is pointer to revk_bin_t
#define	SETTING_SIGNED		4       // Numeric is signed
#define	SETTING_BOOLEAN		8       // Boolean value (array sets bits)
#define	SETTING_BITFIELD	16      // Numeric value has bit field prefix (from defval string)
#define	SETTING_HEX		32      // Source string is hex coded
#define	SETTING_SET		64      // Set top bit of numeric if a value is present at all
#define	SETTING_SECRET		128     // Don't dump setting
#define	SETTING_FIX		256	// Store in flash regardless, so default only used on initial s/w run

#if CONFIG_LOG_DEFAULT_LEVEL > 2
esp_err_t revk_err_check(esp_err_t, const char *file, int line, const char *func, const char *cmd);     // Log if error
#define	REVK_ERR_CHECK(x) revk_err_check(x,__FILE__,__LINE__,__FUNCTION__,#x)
#else
esp_err_t revk_err_check(esp_err_t e);
#define	REVK_ERR_CHECK(x) revk_err_check(x)
#endif

// Make a task
TaskHandle_t revk_task(const char *tag, TaskFunction_t t, const void *param,int kstack);

// reporting via main MQTT, copy option is how many additional MQTT to copy, normally 0 or 1. Setting -N means send only to specific additional MQTT, return NULL for no error
const char* revk_mqtt_send_raw(const char *topic, int retain, const char *payload, uint8_t clients);
const char* revk_mqtt_send_payload_clients(const char *prefix, int retain, const char *suffix, const char *payload, uint8_t clients);
const char* revk_mqtt_send_str_clients(const char *str, int retain, uint8_t clients);
#define	revk_mqtt_send_str(s) revk_mqtt_send_str_clients(s,0,1);
const char* revk_state_clients(const char *suffix, jo_t *, uint8_t clients);
#define revk_state(t,j) revk_state_clients(t,j,1)
const char* revk_event_clients(const char *suffix, jo_t *, uint8_t clients);
#define revk_event(t,j) revk_event_clients(t,j,1)
const char* revk_error_clients(const char *suffix, jo_t *, uint8_t clients);
#define revk_error(t,j) revk_error_clients(t,j,1)
const char* revk_info_clients(const char *suffix, jo_t *, uint8_t clients);
#define revk_info(t,j) revk_info_clients(t,j,1)

const char* revk_mqtt_send_clients(const char *prefix, int retain, const char *suffix, jo_t * jp, uint8_t clients);
#define revk_mqtt_send(p,r,t,j) revk_mqtt_send_clients(p,r,t,j,1)

const char *revk_setting(jo_t); // Store settings
const char *revk_command(const char *tag, jo_t);        // Do an internal command
const char *revk_restart(const char *reason, int delay);        // Restart cleanly
const char *revk_ota(const char *host, const char *target);     // OTA and restart cleanly (target NULL for self as root node)
uint32_t revk_shutting_down(const char **); // If we are shutting down (how many seconds to go) - sets reason if not null
const char *revk_build_date(char d[20]); // Get build date ISO formatted

#ifdef	CONFIG_REVK_MQTT
void revk_mqtt_init(void);
lwmqtt_t revk_mqtt(int);
void revk_mqtt_close(const char *reason);       // Clean close MQTT
int revk_wait_mqtt(int seconds);
#endif
#if	defined(CONFIG_REVK_WIFI) || defined(CONFIG_REVK_MESH)
uint32_t revk_link_down(void);  // How long link down (no IP or no parent if mesh)
#define MESH_PAD        32      // Max extra allocated bytes required on data
const char *revk_wifi(void); // Return the wifi SSID
void revk_wifi_close(void); // Close wifi
int revk_wait_wifi(int seconds); // Wait for wifi
#endif
#ifdef	CONFIG_REVK_MESH
extern uint16_t meshmax;
void revk_send_sub(int client, const mac_t);
void revk_send_unsub(int client, const mac_t);
void revk_mesh_send_json(const mac_t mac, jo_t * jp);
#endif
void revk_blink(uint8_t on, uint8_t off, const char *colours);  // Set LED blink rate and colour sequence for on state (for RGB LED)

#ifdef  CONFIG_REVK_APMODE
esp_err_t revk_web_config_start(httpd_handle_t webserver); // Add URLs
esp_err_t revk_web_config_stop(httpd_handle_t webserver); // Remove URLs
esp_err_t revk_web_config(httpd_req_t * req);   // Call for web config for SSID/password/mqtt
esp_err_t revk_web_wifilist(httpd_req_t * req); // WS for list of SSIDs
#endif
#endif
