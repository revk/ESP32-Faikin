#ifndef REVK_ESP8266_HTTPD_COMPAT_H
#define REVK_ESP8266_HTTPD_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

static inline esp_err_t httpd_resp_sendstr_chunk(httpd_req_t *r, const char *buf)
{
	return httpd_resp_send_chunk(r, buf, buf ? strlen(buf) : 0);
}

static inline esp_err_t httpd_resp_sendstr(httpd_req_t *r, const char *buf)
{
	return httpd_resp_send(r, buf, strlen(buf));
}

#endif

#endif
