#ifndef	LWMQTT_H
#define	LWMQTT_H
// Light weight MQTT client
// QoS 0 only, no queuing or resending (using TCP to do that for us)
// Live sending to TCP for outgoing messages
// Simple callback for incoming messages
// Automatic reconnect

// Callback function for a connection (client or server)
// For client, the arg passed is as specified in the client config
// For server, the arg passed is the lwmqtt_t for the new connection
// Called for incoming message
// - Topic is NULL terminated string, even if zero length topic has been used, and can be overwritten in situ
// - Payload is also NULL terminated at len, for convenience, and can be overwritten in situ
// Called for connect
// - Topic is NULL
// - Payload is server/client name
// Called for disconnect
// - Topic is NULL
// - Payload is NULL
// Called as server for subscribe
// - Topic is subscribe pattern
// - Payload is NULL
typedef void lwmqtt_callback_t (void *arg, char *topic, unsigned short len, unsigned char *payload);

typedef struct lwmqtt_client_config_s lwmqtt_client_config_t;

// Config for connection
struct lwmqtt_client_config_s
{
   lwmqtt_callback_t *callback;
   void *arg;
   const char *client;
   const char *hostname;        // Name or IP
   const char *username;
   const char *password;
   const char *tlsname;         // Name of cert if not host name
   unsigned short port;         // Port 0=auto
   unsigned short keepalive;    // 0=default
   // Will
   const char *topic;           // Will topic
   int plen;                    // Will payload len (-1 does strlen)
   const unsigned char *payload;        // Will payload
   uint8_t retain:1;            // Will retain
   // TLS
   void *ca_cert_buf;           // For checking server - assumed we need to make a copy
   int ca_cert_bytes;
   void *client_cert_buf;       // For client auth
   int client_cert_bytes;
   void *client_key_buf;        // For client auth
   int client_key_bytes;
     esp_err_t (*crt_bundle_attach) (void *conf);
   uint8_t hostname_ref:1;      // The _buf above is fixed and so we do not need to make a copy
   uint8_t tlsname_ref:1;       // The _buf above is fixed and so we do not need to make a copy
   uint8_t ca_cert_ref:1;       // The _buf above is fixed and so we do not need to make a copy
   uint8_t client_cert_ref:1;   // The _buf above is fixed and so we do not need to make a copy
   uint8_t client_key_ref:1;    // The _buf above is fixed and so we do not need to make a copy
};

#ifdef	CONFIG_REVK_MQTT_SERVER
typedef struct lwmqtt_server_config_s lwmqtt_server_config_t;

// Config for connection
struct lwmqtt_server_config_s
{
   lwmqtt_callback_t *callback;
   unsigned short port;         // Port 0=auto
   // TLS
   void *ca_cert_buf;           // For checking server
   int ca_cert_bytes;
   void *server_cert_buf;       // For server auth
   int server_cert_bytes;
   void *server_key_buf;        // For server auth
   int server_key_bytes;
   uint8_t ca_cert_ref:1;       // The _buf above is fixed and so we do not need to make a copy
   uint8_t server_cert_ref:1;   // The _buf above is fixed and so we do not need to make a copy
   uint8_t server_key_ref:1;    // The _buf above is fixed and so we do not need to make a copy
};
#endif

// Handle for connection
typedef struct lwmqtt_s *lwmqtt_t;

uint32_t uptime (void);         // Seconds uptime

int lwmqtt_connected (lwmqtt_t);        // If connected
int lwmqtt_failed (lwmqtt_t);   // If failed connect

// Create a client connection (NULL if failed)
lwmqtt_t lwmqtt_client (lwmqtt_client_config_t *);

#ifdef	CONFIG_REVK_MQTT_SERVER
// Start a server (the return value is only usable in lwmqtt_end)
lwmqtt_t lwmqtt_server (lwmqtt_server_config_t *);
#endif

// End connection - actually freed later as part of task. Will do a callback when closed if was connected
// NULLs the passed handle - do not use handle after this call
void lwmqtt_end (lwmqtt_t *);

// Subscribe (return is non null error message if failed)
const char *lwmqtt_subscribeub (lwmqtt_t, const char *topic, char unsubscribe);
#define lwmqtt_subscribe(h,t) lwmqtt_subscribeub(h,t,0);
#define lwmqtt_unsubscribe(h,t) lwmqtt_subscribeub(h,t,1);

// Send (return is non null error message if failed) (-1 tlen or plen do strlen)
const char *lwmqtt_send_full (lwmqtt_t, int tlen, const char *topic, int plen, const unsigned char *payload, char retain);
// Simpler
#define lwmqtt_send(h,t,l,p) lwmqtt_send_full(h,-1,t,l,p,0,0);

// Simple send - non retained no wait topic ends on space then payload
const char *lwmqtt_send_str (lwmqtt_t, const char *msg);
#endif
