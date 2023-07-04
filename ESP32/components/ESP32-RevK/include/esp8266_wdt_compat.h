#ifndef REVK_ESP8266_WDT_COMPAT_H
#define REVK_ESP8266_WDT_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

static inline esp_err_t compat_task_wdt_add(void)
{
	return esp_task_wdt_init();
}

static inline esp_err_t compat_task_wdt_reconfigure(bool init, uint32_t timeout, bool panic)
{
	return ESP_OK;
}

#else // ESP32

static inline esp_err_t compat_task_wdt_add(void)
{
	return esp_task_wdt_add(NULL);
}

static inline esp_err_t compat_task_wdt_reconfigure(bool init, uint32_t timeout, bool panic)
{
    esp_task_wdt_config_t config = {
       .timeout_ms = timeout,
       .trigger_panic = panic,
    };
#ifndef	CONFIG_ESP_TASK_WDT_INIT
    if (init && esp_task_wdt_init(&config) == ESP_OK)
		return ESP_OK;
#endif
    return esp_task_wdt_reconfigure(&config);
}

#endif

#endif
