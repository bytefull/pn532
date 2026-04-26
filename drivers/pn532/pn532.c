/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_pn532

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pn532, CONFIG_PN532_LOG_LEVEL);

#include "pn532.h"
#include "pn532_transport.h"


struct pn532_data {
    uint32_t dummy;
};

struct pn532_config {
    const struct device *i2c_dev;
    uint16_t i2c_addr;
};

static int get_firmware_version(const struct device *dev, uint32_t *version)
{
    if ((dev == NULL) || (version == NULL)) {
        return -EINVAL;
    }

    /* 1. Build the frame using the command and command length as inputs */
    /* 2. Send the frame over the configured transport e.g: I2C */
    /* 3. Wait (timeout+retry) for the PN532 to be ready to give us an answer (in this case: ACK),
          e.g: in I2C this is done by polling/reading the RDY status byte */
    /* 4. Read ACK frame (6 bytes) over the configured transport and verify it */
    /* 5. Poll the RDY status byte again to check if the PN532 to be ready to give us an answer
          (in this case: firmware version) */
    /* 6. Optionally we can send an ACK to the PN532 */

    /* TODO: Replace dummy hardcoded value with real implementation */
    *version = 0x01020304;

    return 0;
}

static DEVICE_API(pn532, pn532_api) = {
    .pn532_get_firmware_version = &get_firmware_version,
};

static int pn532_init(const struct device *dev)
{
    int ret = 0;
    if (ret < 0) {
        LOG_ERR("Transport init failed: %d", ret);
        return ret;
    }

    LOG_INF("PN532 initialized");

    return 0;
}

#define PN532_DEFINE(inst)                             \
    static struct pn532_data data_##inst;              \
                                                       \
    static const struct pn532_config config_##inst = { \
        .i2c_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),   \
        .i2c_addr = DT_INST_REG_ADDR(inst),            \
    };                                                 \
                                                       \
    DEVICE_DT_INST_DEFINE(inst,                        \
                          pn532_init,                  \
                          NULL,                        \
                          &data_##inst,                \
                          &config_##inst,              \
                          POST_KERNEL,                 \
                          CONFIG_PN532_INIT_PRIORITY,  \
                          &pn532_api);

DT_INST_FOREACH_STATUS_OKAY(PN532_DEFINE)
