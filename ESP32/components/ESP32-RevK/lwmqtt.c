// Light weight MQTT client
// QoS 0 only, no queuing or resending (using TCP to do that for us)
// Live sending to TCP for outgoing messages
// Simple callback for incoming messages
// Automatic reconnect
static const char __attribute__((unused)) * TAG = "LWMQTT";

#include "config.h"
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
#include "esp_log.h"
#include "esp_tls.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_tls.h"
#ifdef  CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "lwmqtt.h"
#include "esp8266_tls_compat.h"

#ifdef	CONFIG_REVK_MQTT_SERVER
#warning MQTT server code is not complete
#endif

uint32_t
uptime (void)
{
   return esp_timer_get_time () / 1000000LL ? : 1;
}

struct lwmqtt_s
{                               // mallocd copies
   lwmqtt_callback_t *callback;
   void *arg;
   char *hostname;
   char *tlsname;
   unsigned short port;
   unsigned short connectlen;
   unsigned char *connect;
   SemaphoreHandle_t mutex;     // atomic send mutex
   esp_tls_t *tls;              // Connection handle
   int sock;                    // Connection socket
   unsigned short keepalive;
   unsigned short seq;
   uint32_t ka;                 // Keep alive next ping
   uint8_t backoff;             // Reconnect backoff
   uint8_t running:1;           // Should still run
   uint8_t server:1;            // This is a server
   uint8_t connected:1;         // Login sent/received
   uint8_t failed:1;            // Login sent/received
   uint8_t hostname_ref;        // The buf below is not malloc'd
   uint8_t tlsname_ref;         // The buf below is not malloc'd
   uint8_t ca_cert_ref:1;       // The _buf below is not malloc'd
   uint8_t our_cert_ref:1;      // The _buf below is not malloc'd
   uint8_t our_key_ref:1;       // The _buf below is not malloc'd
   void *ca_cert_buf;           // For checking server
   int ca_cert_bytes;
   void *our_cert_buf;          // For auth
   int our_cert_bytes;
   void *our_key_buf;           // For auth
   int our_key_bytes;
     esp_err_t (*crt_bundle_attach) (void *conf);
};

#define	hread(handle,buf,len)	(handle->tls?esp_tls_conn_read(handle->tls,buf,len):read(handle->sock,buf,len))

static int
hwrite (lwmqtt_t handle, uint8_t * buf, int len)
{                               // Send (all of) a block
   int pos = 0;
   while (pos < len)
   {
      int sent =
         (handle->tls ? esp_tls_conn_write (handle->tls, buf + pos, len - pos) : write (handle->sock, buf + pos, len - pos));
      if (sent <= 0)
         return sent;
      pos += sent;
   }
   return pos;
}

#define freez(x) do{if(x){free(x);x=NULL;}}while(0)
static void *
handle_free (lwmqtt_t handle)
{
   if (handle)
   {
      freez (handle->connect);
      if (!handle->hostname_ref)
         freez (handle->hostname);
      if (!handle->tlsname_ref)
         freez (handle->tlsname);
      if (!handle->ca_cert_ref)
         freez (handle->ca_cert_buf);
      if (!handle->our_cert_ref)
         freez (handle->our_cert_buf);
      if (!handle->our_key_ref)
         freez (handle->our_key_buf);
      if (handle->mutex)
         vSemaphoreDelete (handle->mutex);
      freez (handle);
   }
   return NULL;
}

void
handle_close (lwmqtt_t handle)
{
   esp_tls_t *tls = handle->tls;
   int sock = handle->sock;
   handle->tls = NULL;
   handle->sock = -1;
   if (tls)
   {                            // TLS
      if (handle->server)
      {
#ifdef CONFIG_ESP_TLS_SERVER
         esp_tls_server_session_delete (tls);
#endif
         close (sock);
      } else
         esp_tls_conn_destroy (tls);
   } else if (sock >= 0)
      close (sock);
}

static int
handle_certs (lwmqtt_t h, uint8_t ca_cert_ref, int ca_cert_bytes, void *ca_cert_buf, uint8_t our_cert_ref, int our_cert_bytes,
              void *our_cert_buf, uint8_t our_key_ref, int our_key_bytes, void *our_key_buf)
{
   int fail = 0;
   if (ca_cert_bytes && ca_cert_buf)
   {
      h->ca_cert_bytes = ca_cert_bytes;
      if ((h->ca_cert_ref = ca_cert_ref))
         h->ca_cert_buf = ca_cert_buf;  // No malloc as reference will stay valid
      else if (!(h->ca_cert_buf = malloc (ca_cert_bytes)))
         fail++;                // malloc failed
      else
         memcpy (h->ca_cert_buf, ca_cert_buf, ca_cert_bytes);
   }
   if (our_cert_bytes && our_cert_buf)
   {
      h->our_cert_bytes = our_cert_bytes;
      if ((h->our_cert_ref = our_cert_ref))
         h->our_cert_buf = our_cert_buf;        // No malloc as reference will stay valid
      else if (!(h->our_cert_buf = malloc (our_cert_bytes)))
         fail++;                // malloc failed
      else
         memcpy (h->our_cert_buf, our_cert_buf, our_cert_bytes);
   }
   if (our_key_bytes && our_key_buf)
   {
      h->our_key_bytes = our_key_bytes;
      if ((h->our_key_ref = our_cert_ref))
         h->our_key_buf = our_key_buf;  // No malloc as reference will stay valid
      else if (!(h->our_key_buf = malloc (our_key_bytes)))
         fail++;                // malloc failed
      else
         memcpy (h->our_key_buf, our_key_buf, our_key_bytes);
   }
   return fail;
}

static void client_task (void *pvParameters);
#ifdef  CONFIG_REVK_MQTT_SERVER
static void listen_task (void *pvParameters);
#endif

// Create a connection
lwmqtt_t
lwmqtt_client (lwmqtt_client_config_t * config)
{
   if (!config || !config->hostname)
      return NULL;
   lwmqtt_t handle = malloc (sizeof (*handle));
   if (!handle)
      return handle_free (handle);
   memset (handle, 0, sizeof (*handle));
   handle->sock = -1;
   handle->callback = config->callback;
   handle->arg = config->arg;
   handle->keepalive = config->keepalive ? : 60;
   if ((handle->hostname_ref = config->hostname_ref))
      handle->hostname = (void *) config->hostname;
   else if (!(handle->hostname = strdup (config->hostname)))
      return handle_free (handle);
   handle->port = (config->port ? : (config->ca_cert_bytes || config->crt_bundle_attach) ? 8883 : 1883);
   if ((handle->tlsname_ref = config->tlsname_ref))
      handle->tlsname = (void *) config->tlsname;
   else if (config->tlsname && *config->tlsname && !(handle->tlsname = strdup (config->tlsname)))
      return handle_free (handle);
   // Make connection message
   int mlen = 6 + 1 + 1 + 2 + strlen (config->client ? : "");
   if (config->plen < 0)
      config->plen = strlen ((char *) config->payload ? : "");
   if (config->topic)
      mlen += 2 + strlen (config->topic) + 2 + config->plen;
   if (config->username)
      mlen += 2 + strlen (config->username);
   if (config->password)
      mlen += 2 + strlen (config->password);
   if (handle_certs
       (handle, config->ca_cert_ref, config->ca_cert_bytes, config->ca_cert_buf, config->client_cert_ref, config->client_cert_bytes,
        config->client_cert_buf, config->client_key_ref, config->client_key_bytes, config->client_key_buf))
      return handle_free (handle);      // Nope
   handle->crt_bundle_attach = config->crt_bundle_attach;
   if (mlen >= 128 * 128)
      return handle_free (handle);      // Nope
   mlen += 2;                   // keepalive
   if (mlen >= 128)
      mlen++;                   // two byte len
   mlen += 2;                   // header and one byte len
   if (!(handle->connect = malloc (mlen)))
      return handle_free (handle);
   unsigned char *p = handle->connect;
   void str (int l, const char *s)
   {
      if (l < 0)
         l = strlen (s ? : "");
      *p++ = l >> 8;
      *p++ = l;
      if (l && s)
         memcpy (p, s, l);
      p += l;
   }
   *p++ = 0x10;                 // connect
   if (mlen > 129)
   {                            // Two byte len
      *p++ = (((mlen - 3) & 0x7F) | 0x80);
      *p++ = ((mlen - 3) >> 7);
   } else
      *p++ = mlen - 2;          // 1 byte len
   str (4, "MQTT");
   *p++ = 4;                    // protocol level
   *p = 0x02;                   // connect flags (clean)
   if (config->username)
   {
      *p |= 0x80;               // Username
      if (config->password)
         *p |= 0x40;            // Password
   }
   if (config->topic)
   {
      *p |= 0x04;               // Will
      if (config->retain)
         *p |= 0x20;            // Will retain
   }
   p++;
   *p++ = handle->keepalive >> 8;       // keep alive
   *p++ = handle->keepalive;
   str (-1, config->client);    // Client ID
   if (config->topic)
   {                            // Will
      str (-1, config->topic);  // Topic
      str (config->plen, (void *) config->payload);     // Payload
   }
   if (config->username)
   {
      str (-1, config->username);
      if (config->password)
         str (-1, config->password);
   }
   assert ((p - handle->connect) == mlen);
   handle->connectlen = mlen;
   handle->mutex = xSemaphoreCreateBinary ();
   xSemaphoreGive (handle->mutex);
   handle->running = 1;
   TaskHandle_t task_id = NULL;
   xTaskCreate (client_task, "mqtt-client", 4 * 1024, (void *) handle, 2, &task_id);
   return handle;
}

#ifdef	CONFIG_REVK_MQTT_SERVER
// Start a server
lwmqtt_t
lwmqtt_server (lwmqtt_server_config_t * config)
{
   if (!config)
      return NULL;
   lwmqtt_t handle = malloc (sizeof (*handle));
   if (!handle)
      return handle_free (handle);
   memset (handle, 0, sizeof (*handle));
   handle->callback = config->callback;
   handle->port = (config->port ? : config->ca_cert_bytes ? 8883 : 1883);
   if (handle_certs
       (handle, config->ca_cert_ref, config->ca_cert_bytes, config->ca_cert_buf, config->server_cert_ref, config->server_cert_bytes,
        config->server_cert_buf, config->server_key_ref, config->server_key_bytes, config->server_key_buf))
      return handle_free (handle);
   handle->running = 1;
   TaskHandle_t task_id = NULL;
   xTaskCreate (listen_task, "mqtt-server", 5 * 1024, (void *) handle, 2, &task_id);
   return handle;
}
#endif

// End connection - actually freed later as part of task. Will do a callback when closed if was connected
// NULLs the passed handle - do not use handle after this call
void
lwmqtt_end (lwmqtt_t * handle)
{
   if (!handle || !*handle)
      return;
   if ((*handle)->running)
   {
      ESP_LOGD (TAG, "Ending");
      (*handle)->running = 0;
   }
   *handle = NULL;
}

// Subscribe (return is non null error message if failed)
const char *
lwmqtt_subscribeub (lwmqtt_t handle, const char *topic, char unsubscribe)
{
   const char *ret = NULL;
   if (!handle)
      ret = "No handle";
   else if (handle->server)
      ret = "We are server";
   else
   {
      int tlen = strlen (topic ? : "");
      int mlen = 2 + 2 + tlen;
      if (!unsubscribe)
         mlen++;                // QoS requested
      if (mlen >= 128 * 128)
         ret = "Too big";
      else
      {
         if (mlen >= 128)
            mlen++;             // two byte len
         mlen += 2;             // header and one byte len
         unsigned char *buf = malloc (mlen);
         if (!buf)
            ret = "Malloc";
         else
         {
            if (!xSemaphoreTake (handle->mutex, portMAX_DELAY))
               ret = "Failed to get lock";
            else
            {
               if (handle->sock < 0)
                  ret = "Not connected";
               else
               {
                  unsigned char *p = buf;
                  *p++ = (unsubscribe ? 0xA2 : 0x82);   // subscribe/unsubscribe
                  if (mlen > 129)
                  {             // Two byte len
                     *p++ = (((mlen - 3) & 0x7F) | 0x80);
                     *p++ = ((mlen - 3) >> 7);
                  } else
                     *p++ = mlen - 2;   // 1 byte len
                  if (!++(handle->seq))
                     handle->seq++;     // Non zero
                  *p++ = handle->seq >> 8;
                  *p++ = handle->seq;
                  *p++ = tlen >> 8;
                  *p++ = tlen;
                  if (tlen)
                     memcpy (p, topic, tlen);
                  p += tlen;
                  if (!unsubscribe)
                     *p++ = 0x00;       // QoS requested
                  assert ((p - buf) == mlen);
                  if (hwrite (handle, buf, mlen) < mlen)
                     ret = "Failed to send";
                  else
                     handle->ka = uptime () + handle->keepalive;
               }
               xSemaphoreGive (handle->mutex);
            }
            freez (buf);
         }
      }
   }
   if (ret)
      ESP_LOGD (TAG, "Sub/unsub: %s", ret);
   return ret;
}

// Send (return is non null error message if failed)
const char *
lwmqtt_send_full (lwmqtt_t handle, int tlen, const char *topic, int plen, const unsigned char *payload, char retain)
{
   const char *ret = NULL;
   if (!handle)
      ret = "No handle";
   else
   {
      if (tlen < 0)
         tlen = strlen (topic ? : "");
      if (plen < 0)
         plen = strlen ((char *) payload ? : "");
      int mlen = 2 + tlen + plen;
      if (mlen >= 128 * 128)
         ret = "Too big";
      else
      {
         if (mlen >= 128)
            mlen++;             // two byte len
         mlen += 2;             // header and one byte len
         unsigned char *buf = malloc (mlen);
         if (!buf)
            ret = "Malloc";
         else
         {
            if (!xSemaphoreTake (handle->mutex, portMAX_DELAY))
               ret = "Failed to get lock";
            else
            {
               if (handle->sock < 0)
                  ret = "Not connected";
               else
               {
                  unsigned char *p = buf;
                  *p++ = 0x30 + (retain ? 1 : 0);       // message
                  if (mlen > 129)
                  {             // Two byte len
                     *p++ = (((mlen - 3) & 0x7F) | 0x80);
                     *p++ = ((mlen - 3) >> 7);
                  } else
                     *p++ = mlen - 2;   // 1 byte len
                  *p++ = tlen >> 8;
                  *p++ = tlen;
                  if (tlen)
                     memcpy (p, topic, tlen);
                  p += tlen;
                  if (plen && payload)
                     memcpy (p, payload, plen);
                  p += plen;
                  assert ((p - buf) == mlen);
                  if (hwrite (handle, buf, mlen) < mlen)
                     ret = "Failed to send";
                  else if (!handle->server)
                     handle->ka = uptime () + handle->keepalive;        // client KA refresh
               }
               xSemaphoreGive (handle->mutex);
            }
            freez (buf);
         }
      }
   }
   if (ret)
      ESP_LOGD (TAG, "Send: %s", ret);
   return ret;
}

static void
lwmqtt_loop (lwmqtt_t handle)
{
   // Handle rx messages
   unsigned char *buf = 0;
   int buflen = 0;
   int pos = 0;
   handle->ka = uptime () + (handle->server ? 5 : handle->keepalive);   // Server does not know KA initially
   while (handle->running)
   {                            // Loop handling messages received, and timeouts
      int need = 0;
      if (pos < 2)
         need = 2;
      else if (!(buf[1] & 0x80))
         need = 2 + buf[1];     // One byte len
      else if (pos < 3)
         need = 3;
      else if (!(buf[2] & 0x80))
         need = 3 + (buf[2] << 7) + (buf[1] & 0x7F);    // Two byte len
      else
      {
         ESP_LOGE (TAG, "Silly len %02X %02X %02X", buf[0], buf[1], buf[2]);
         break;
      }
      if (pos < need)
      {
         uint32_t now = uptime ();
         if (now >= handle->ka)
         {
            if (handle->server)
               break;           // timeout
            // client, so send ping
            uint8_t b[] = { 0xC0, 0x00 };       // Ping
            xSemaphoreTake (handle->mutex, portMAX_DELAY);
            if (hwrite (handle, b, sizeof (b)) == sizeof (b))
               handle->ka = uptime () + handle->keepalive;      // Client KA refresh
            xSemaphoreGive (handle->mutex);
         }
         if (!handle->tls || esp_tls_get_bytes_avail (handle->tls) <= 0)
         {                      // Wait for data to arrive
            fd_set r;
            FD_ZERO (&r);
            FD_SET (handle->sock, &r);
            struct timeval to = { 1, 0 };       // Keeps us checking running but is light load at once a second
            int sel = select (handle->sock + 1, &r, NULL, NULL, &to);
            if (sel < 0)
            {
               ESP_LOGE (TAG, "Select failed");
               break;
            }
            if (!FD_ISSET (handle->sock, &r))
               continue;        // Nothing waiting
         }
         if (need > buflen)
         {                      // Make sure we have enough space
            buf = realloc (buf, (buflen = need) + 1);   // One more to allow extra null on end in all cases
            if (!buf)
            {
               ESP_LOGE (TAG, "realloc fail %d", need);
               break;
            }
         }
         int got = hread (handle, buf + pos, need - pos);
         if (got <= 0)
         {
            ESP_LOGD (TAG, "Connection closed");
            break;              // Error or close
         }
         pos += got;
         continue;
      }
      if (handle->server)
         handle->ka = uptime () + handle->keepalive * 3 / 2;    // timeout for client resent on message received
      unsigned char *p = buf + 1,
         *e = buf + pos;
      while (p < e && (*p & 0x80))
         p++;
      p++;
      if (handle->server && !handle->connected && (*buf >> 4) != 1)
         break;                 // Expect login as first message
      switch (*buf >> 4)
      {
      case 1:
#ifdef CONFIG_REVK_MQTT_SERVER
         if (!handle->server)
            break;
         handle->connected = 1;
         ESP_LOGI (TAG, "Connected incoming %d", handle->port);
         // TODO incoming connect
         handle->keepalive = 10;        // TODO get from message
         uint8_t b[4] = { 0x20 };       // conn ack
         xSemaphoreTake (handle->mutex, portMAX_DELAY);
         if (hwrite (handle, b, sizeof (b)) == sizeof (b) && !handle->server)
            handle->ka = uptime () + handle->keepalive; // KA client refresh
         xSemaphoreGive (handle->mutex);
#endif
         break;
      case 2:                  // conack
         if (handle->server)
            break;
         ESP_LOGI (TAG, "Connected %s:%d", handle->hostname, handle->port);
         handle->backoff = 1;
         if (handle->callback)
            handle->callback (handle->arg, NULL, strlen (handle->hostname), (void *) handle->hostname);
         handle->connected = 1;
         break;
      case 3:                  // pub
         {                      // Topic
            int tlen = (p[0] << 8) + p[1];
            p += 2;
            char *topic = (char *) p;
            p += tlen;
            unsigned short id = 0;
            if (*buf & 0x06)
            {
               id = (p[0] << 8) + p[1];
               p += 2;
            }
            if (p > e)
            {
               ESP_LOGE (TAG, "Bad msg");
               break;
            }
            if (*buf & 0x06)
            {                   // reply
               uint8_t b[4] = { (*buf & 0x4) ? 0x50 : 0x40, 2, id >> 8, id };
               xSemaphoreTake (handle->mutex, portMAX_DELAY);
               if (hwrite (handle, b, sizeof (b)) == sizeof (b) && !handle->server)
                  handle->ka = uptime () + handle->keepalive;   // KA client refresh
               xSemaphoreGive (handle->mutex);
            }
            int plen = e - p;
            if (handle->callback)
            {
               if (plen && !(*buf & 0x06))
               {                // Move back a byte for null termination to be added without hitting payload
                  memmove (topic - 1, topic, tlen);
                  topic--;
               }
               topic[tlen] = 0;
               p[plen] = 0;
               handle->callback (handle->arg, topic, plen, p);
            }
         }
         break;
      case 4:                  // puback - no action as we don't use non QoS 0
         break;
      case 5:                  // pubrec - not expected as we don't use non QoS 0
         if (handle->server)
            break;
         {
            uint8_t b[4] = { 0x60, p[0], p[1] };
            xSemaphoreTake (handle->mutex, portMAX_DELAY);
            if (hwrite (handle, b, sizeof (b)) == sizeof (b) && !handle->server)
               handle->ka = uptime () + handle->keepalive;      // KA client refresh
            xSemaphoreGive (handle->mutex);
         }
         break;
      case 6:                  // pubcomp - no action as we don't use non QoS 0
         break;
      case 8:                  // sub
#ifdef CONFIG_REVK_MQTT_SERVER
         if (!handle->server)
            break;
         // TODO
#endif
         break;
      case 9:                  // suback - no action
         break;
      case 10:                 // unsub - no action
         break;
      case 11:                 // unsuback - ok
         if (handle->server)
            break;
         break;
      case 12:                 // ping (no action as resets ka anyway)
         break;
      case 13:                 // pingresp - no action - though we could use lack of reply to indicate broken connection I guess
         break;
      default:
         ESP_LOGE (TAG, "Unknown MQTT %02X (%d)", *buf, pos);
      }
      pos = 0;
   }
   handle->connected = 0;
   freez (buf);
   if (!handle->server && !handle->running)
   {                            // Close connection - as was clean
      ESP_LOGD (TAG, "Close cleanly");
      uint8_t b[] = { 0xE0, 0x00 };     // Disconnect cleanly
      xSemaphoreTake (handle->mutex, portMAX_DELAY);
      hwrite (handle, b, sizeof (b));
      xSemaphoreGive (handle->mutex);
   }
   handle_close (handle);
   if (handle->callback)
      handle->callback (handle->arg, NULL, 0, NULL);
}

static void
client_task (void *pvParameters)
{
   lwmqtt_t handle = pvParameters;
   if (!handle)
   {
      vTaskDelete (NULL);
      return;
   }
   handle->backoff = 1;
   while (handle->running)
   {                            // Loop connecting and trying repeatedly
      handle->sock = -1;
      // Connect
      ESP_LOGD (TAG, "Connecting %s:%d", handle->hostname, handle->port);
      // Can connect using TLS or non TLS with just sock set instead
      if (handle->ca_cert_bytes || handle->crt_bundle_attach)
      {
         esp_tls_t *tls = NULL;
         esp_tls_cfg_t cfg = {
            .cacert_buf = handle->ca_cert_buf,
            .cacert_bytes = handle->ca_cert_bytes,
            .common_name = handle->tlsname,
            .clientcert_buf = handle->our_cert_buf,
            .clientcert_bytes = handle->our_cert_bytes,
            .clientkey_buf = handle->our_key_buf,
            .clientkey_bytes = handle->our_key_bytes,
            .crt_bundle_attach = handle->crt_bundle_attach,
         };
         tls = esp_tls_init ();
         if (esp_tls_conn_new_sync (handle->hostname, strlen (handle->hostname), handle->port, &cfg, tls) != 1)
         {
            ESP_LOGE (TAG, "Could not TLS connect to %s:%d", handle->hostname, handle->port);
            free (tls);
         } else
         {
            handle->tls = tls;
            esp_tls_get_conn_sockfd (handle->tls, &handle->sock);
         }
      } else
      {                         // Non TLS
         // This is annoying as it should just pick IPv6 as preferred, but it sort of works
         // May be better as a generic connect, and we are also rather assuming TLS ^ will connect IPv6 is available
         int tryconnect (int fam)
         {
            if (handle->sock >= 0)
               return 0;        // connected
          struct addrinfo base = { ai_family: fam, ai_socktype:SOCK_STREAM };
            struct addrinfo *a = 0,
               *p = NULL;
            char sport[6];
            snprintf (sport, sizeof (sport), "%d", handle->port);
            if (getaddrinfo (handle->hostname, sport, &base, &a) || !a)
               return 0;
            for (p = a; p; p = p->ai_next)
            {
               handle->sock = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
               if (handle->sock < 0)
                  continue;
               if (connect (handle->sock, p->ai_addr, p->ai_addrlen))
               {
                  close (handle->sock);
                  handle->sock = -1;
                  continue;
               }
               break;
            }
            freeaddrinfo (a);
            return 1;
         }
         if (!tryconnect (AF_INET6) || uptime () > 20)  // Gives IPv6 a chance to actually get started if there is IPv6 DNS for this.
            tryconnect (AF_INET);
         if (handle->sock < 0)
            ESP_LOGD (TAG, "Could not connect to %s:%d", handle->hostname, handle->port);
      }
      if (handle->sock < 0)
      {                         // Failed before we even start
         handle->failed = 1;
         if (handle->callback)
            handle->callback (handle->arg, NULL, 0, NULL);
      } else
      {
         handle->failed = 0;
         hwrite (handle, handle->connect, handle->connectlen);
         lwmqtt_loop (handle);
      }
      if (!handle->running)
         break;                 // client was stopped
      if (handle->backoff < 128)
         handle->backoff *= 2;
      else
         handle->backoff = 255;
      // On ESP32 uint32_t, returned by this func, appears to be long, while on ESP8266 it's a pure unsigned int
      // The easiest and least ugly way to get around is to cast to long explicitly
      ESP_LOGI (TAG, "Waiting %d (mem:%ld)", handle->backoff / 5, (long)esp_get_free_heap_size ());
      sleep (handle->backoff < 5 ? 1 : handle->backoff / 5);
   }
   handle_free (handle);
   vTaskDelete (NULL);
}

#ifdef	CONFIG_REVK_MQTT_SERVER
static void
server_task (void *pvParameters)
{
   lwmqtt_t handle = pvParameters;
   lwmqtt_loop (handle);
   handle_free (handle);
   vTaskDelete (NULL);
}

static void
listen_task (void *pvParameters)
{
   lwmqtt_t handle = pvParameters;
   if (handle)
   {
      struct sockaddr_in dst = {        // Yep IPv4 local
         .sin_addr.s_addr = htonl (INADDR_ANY),
         .sin_family = AF_INET,
         .sin_port = htons (handle->port),
      };
      int sock = socket (AF_INET, SOCK_STREAM, IPPROTO_IP);
      if (sock >= 0)
      {
         if (bind (sock, (void *) &dst, sizeof (dst)) < 0 || listen (sock, 1) < 0)
            close (sock);
         else
         {
            ESP_LOGD (TAG, "Listening for MQTT on %d", handle->port);
            while (handle->running)
            {                   // Loop connecting and trying repeatedly
               struct sockaddr_in addr;
               socklen_t addrlen = sizeof (addr);
               int s = accept (sock, (void *) &addr, &addrlen);
               if (s < 0)
                  break;
               ESP_LOGD (TAG, "Connect on MQTT %d", handle->port);
               lwmqtt_t h = malloc (sizeof (*h));
               if (!h)
                  break;
               memset (h, 0, sizeof (*h));
               h->port = handle->port;  // Only for debugging
               h->callback = handle->callback;
               h->arg = h;
               h->mutex = xSemaphoreCreateBinary ();
               h->server = 1;
               h->sock = s;
               h->running = 1;
               if (handle->ca_cert_bytes)
               {                // TLS
#ifdef CONFIG_ESP_TLS_SERVER
                  esp_tls_cfg_server_t cfg = {
                     .cacert_buf = handle->ca_cert_buf,
                     .cacert_bytes = handle->ca_cert_bytes,
                     .servercert_buf = handle->our_cert_buf,
                     .servercert_bytes = handle->our_cert_bytes,
                     .serverkey_buf = handle->our_key_buf,
                     .serverkey_bytes = handle->our_key_bytes,
                  };
                  h->tls = esp_tls_init ();
                  esp_err_t e = 0;
                  if (!h->tls || (e = esp_tls_server_session_create (&cfg, s, h->tls)))
                  {
                     ESP_LOGE (TAG, "TLS server failed %s", h->tls ? esp_err_to_name (e) : "No TLS");
                     h->running = 0;
                  } else
                  {             // Check client name? Do login callback
                     // TODO server client name check
                  }
#else
                  ESP_LOGE (TAG, "Not built for TLS server");
                  h->running = 0;
#endif
               }
               if (h->running)
               {
                  TaskHandle_t task_id = NULL;
                  xTaskCreate (server_task, "mqtt-listen", 5 * 1024, (void *) h, 2, &task_id);
               } else
               {                // Close
                  ESP_LOGI (TAG, "MQTT aborted");
#ifdef CONFIG_ESP_TLS_SERVER
                  if (h->tls)
                     esp_tls_server_session_delete (h->tls);
#endif
                  close (h->sock);
               }

            }
         }
      }
      handle_free (handle);
   }
   vTaskDelete (NULL);
}
#endif

// Simple send - non retained no wait topic ends on space then payload
const char *
lwmqtt_send_str (lwmqtt_t handle, const char *msg)
{
   if (!handle || !*msg)
      return NULL;
   const char *p;
   for (p = msg; *p && *p != '\t'; p++);
   if (!*p)
      for (p = msg; *p && *p != ' '; p++);
   int tlen = p - msg;
   if (*p)
      p++;
   return lwmqtt_send_full (handle, tlen, msg, strlen (p), (void *) p, 0);
}

int
lwmqtt_connected (lwmqtt_t handle)
{                               // Confirm connected
   return handle && handle->connected;
}

int
lwmqtt_failed (lwmqtt_t handle)
{                               // Confirm failed
   return handle && handle->failed;
}
