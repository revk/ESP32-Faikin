#ifndef REVK_ESP8266_TLS_COMPAT_H
#define REVK_ESP8266_TLS_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

static inline int esp_tls_conn_destroy(esp_tls_t *tls)
{
	esp_tls_conn_delete(tls);
	return 0;
}

#endif

#endif
