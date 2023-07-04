#ifndef REVK_ESP8266_GPIO_COMPAT_H
#define REVK_ESP8266_GPIO_COMPAT_H

#ifdef CONFIG_IDF_TARGET_ESP8266

static inline esp_err_t gpio_reset_pin(gpio_num_t gpio_num)
{
	// According to the documentation. We don't have alternate functions.
	return gpio_pullup_en(gpio_num);
}

#endif

#endif
