#ifndef REVK_ESP8266_OTA_COMPAT_H
#define REVK_ESP8266_OTA_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

static const esp_app_desc_t *esp_app_get_description(void)
{
    return esp_ota_get_app_description();
}

#endif

#endif
