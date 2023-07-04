#ifndef REVK_ESP8266_COMPAT_H
#define REVK_ESP8266_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

#include "tcpip_adapter.h"

typedef struct
{
	tcpip_adapter_if_t adapter;
} esp_netif_t;

typedef enum
{
	ESP_NETIF_DNS_MAIN     = TCPIP_ADAPTER_DNS_MAIN,
	ESP_NETIF_DNS_BACKUP   = TCPIP_ADAPTER_DNS_BACKUP,
	ESP_NETIF_DNS_FALLBACK = TCPIP_ADAPTER_DNS_FALLBACK
} esp_netif_dns_type_t;

typedef tcpip_adapter_ip_info_t esp_netif_ip_info_t;
typedef tcpip_adapter_dns_info_t esp_netif_dns_info_t;
typedef ip6_addr_t esp_ip6_addr_t;

static inline void esp_netif_set_ip4_addr(ip4_addr_t *addr, uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
	IP4_ADDR(addr, a, b, c, d);
}

static inline esp_err_t esp_netif_str_to_ip4(const char *src, ip4_addr_t *dst)
{
	return ip4addr_aton(src, dst) ? ESP_OK : ESP_FAIL;
}

static inline esp_err_t esp_netif_str_to_ip6(const char *src, ip6_addr_t *dst)
{
	return ip6addr_aton(src, dst) ? ESP_OK : ESP_FAIL;
}

static inline esp_err_t esp_netif_get_old_ip_info(esp_netif_t *tcpip_if, tcpip_adapter_ip_info_t *ip_info)
{
	return tcpip_adapter_get_old_ip_info(tcpip_if->adapter, ip_info);
}

static inline esp_err_t esp_netif_get_ip_info(esp_netif_t *tcpip_if, tcpip_adapter_ip_info_t *ip_info)
{
	return tcpip_adapter_get_ip_info(tcpip_if->adapter, ip_info);
}

static inline esp_err_t esp_netif_set_ip_info(esp_netif_t *tcpip_if, tcpip_adapter_ip_info_t *ip_info)
{
	return tcpip_adapter_set_ip_info(tcpip_if->adapter, ip_info);
}

static inline esp_err_t esp_netif_dhcpc_stop(esp_netif_t *tcpip_if)
{
	return tcpip_adapter_dhcpc_stop(tcpip_if->adapter);
}

static inline esp_err_t esp_netif_dhcpc_start(esp_netif_t *tcpip_if)
{
	return tcpip_adapter_dhcpc_start(tcpip_if->adapter);
}

static inline esp_err_t esp_netif_dhcps_stop(esp_netif_t *tcpip_if)
{
	return tcpip_adapter_dhcps_stop(tcpip_if->adapter);
}

static inline esp_err_t esp_netif_dhcps_start(esp_netif_t *tcpip_if)
{
	return tcpip_adapter_dhcps_start(tcpip_if->adapter);
}

static inline esp_err_t esp_netif_set_dns_info(esp_netif_t *tcpip_if, esp_netif_dns_type_t type, tcpip_adapter_dns_info_t *dns)
{
	return tcpip_adapter_set_dns_info(tcpip_if->adapter, type, dns);
}

static inline esp_err_t esp_netif_create_ip6_linklocal(esp_netif_t *tcpip_if)
{
	return tcpip_adapter_create_ip6_linklocal(tcpip_if->adapter);
}

static inline esp_err_t esp_netif_get_ip6_global(esp_netif_t *tcpip_if, ip6_addr_t *if_ip6)
{
    return tcpip_adapter_get_ip6_global(tcpip_if->adapter, if_ip6);
}

static inline void esp_netif_init(void)
{
	tcpip_adapter_init();
}

static inline esp_err_t esp_netif_set_hostname(esp_netif_t *tcpip_if, const char *hostname)
{
	return tcpip_adapter_set_hostname(tcpip_if->adapter, hostname);
}

esp_netif_t *esp_netif_create_default_wifi_sta(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);

#endif

#endif
