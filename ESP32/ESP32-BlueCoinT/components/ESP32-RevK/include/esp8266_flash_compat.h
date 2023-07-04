#ifndef REVK_ESP8266_FLASH_COMPAT_H
#define REVK_ESP8266_FLASH_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

esp_err_t esp_flash_get_size(void *chip, uint32_t *out_size);

#else
#include "esp_flash_spi_init.h"
#endif

#endif
