#ifndef _CN_WIRED_DRIVER_H
#define _CN_WIRED_DRIVER_H

#include <driver/gpio.h>

esp_err_t cn_wired_driver_install (gpio_num_t rx_num, gpio_num_t tx_num, int rx_invert, int tx_invert);
void cn_wired_driver_delete (void);
esp_err_t cn_wired_read_bytes (uint8_t *rx, int timeout);
esp_err_t cn_wired_write_bytes (const uint8_t *buf);
void cn_wired_stats (jo_t j);

// The driver borrows this function from the main code for own logging
jo_t jo_comms_alloc (void);

#endif
