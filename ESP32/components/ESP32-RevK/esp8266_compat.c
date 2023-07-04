#include "sdkconfig.h"

#include "esp8266_netif_compat.h"
#include "esp8266_flash_compat.h"

esp_netif_t* esp_netif_create_default_wifi_sta(void)
{
	static esp_netif_t netif = {TCPIP_ADAPTER_IF_STA};
	return &netif;
}
	
esp_netif_t* esp_netif_create_default_wifi_ap(void)
{
	static esp_netif_t netif = {TCPIP_ADAPTER_IF_AP};
	return &netif;
}

esp_err_t esp_flash_get_size(void *chip, uint32_t *out_size)
{
	switch (system_get_flash_size_map())
	{
	case FLASH_SIZE_4M_MAP_256_256:
		*out_size = 4;
		break;
    case FLASH_SIZE_2M:
		*out_size = 2;
		break;
    case FLASH_SIZE_8M_MAP_512_512:
    	*out_size = 8;
		break;
    case FLASH_SIZE_16M_MAP_512_512:
    case FLASH_SIZE_16M_MAP_1024_1024:
    	*out_size = 16;
		break;
    case FLASH_SIZE_32M_MAP_512_512:
    case FLASH_SIZE_32M_MAP_1024_1024:
    case FLASH_SIZE_32M_MAP_2048_2048:
    	*out_size = 32;
		break;
    case FLASH_SIZE_64M_MAP_1024_1024:
    	*out_size = 64;
		break;
    case FLASH_SIZE_128M_MAP_1024_1024:
    	*out_size = 128;
		break;
	default:
	    return ESP_FAIL;
	}
	
	return ESP_OK;
}
