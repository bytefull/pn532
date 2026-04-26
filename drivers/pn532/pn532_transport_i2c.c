/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pn532_i2c, CONFIG_PN532_LOG_LEVEL);

#include "pn532.h"
#include "pn532_transport.h"

#define PN532_I2C_READY (0x01)

struct pn532_i2c_config {
	const struct device *i2c_dev;
	uint16_t i2c_addr;
};



int pn532_transport_init(const struct device *dev)
{
	const struct pn532_i2c_config *cfg = dev->config;

    if (!device_is_ready(cfg->i2c_dev)) {
        LOG_ERR("I2C device not ready");
        return -ENODEV;
    }

    LOG_INF("PN532 I2C transport initialized (addr=0x%02X)", cfg->i2c_addr);

    return 0;
}

int pn532_transport_write(const struct device *dev, const uint8_t *buf, size_t len)
{
    const struct pn532_i2c_config *cfg = dev->config;

    return i2c_write(cfg->i2c_dev, buf, len, cfg->i2c_addr);
}

int pn532_transport_read(const struct device *dev, uint8_t *buf, size_t len)
{
    const struct pn532_i2c_config *cfg = dev->config;

    return i2c_read(cfg->i2c_dev, buf, len, cfg->i2c_addr);
}

bool pn532_transport_is_ready(const struct device *dev, int timeout_ms)
{
	const struct pn532_i2c_config *cfg = dev->config;
    uint8_t status = 0;
    int64_t start_time = k_uptime_get();
    int ret;

    while ((k_uptime_get() - start_time) < timeout_ms) {
        ret = i2c_read(cfg->i2c_dev, &status, 1, cfg->i2c_addr);
        if ((ret == 0) && (status == PN532_I2C_READY)) {
            return true;
        }
        k_sleep(K_MSEC(10));
    }

    return false;
}
