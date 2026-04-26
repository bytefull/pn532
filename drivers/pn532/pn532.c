/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_pn532

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pn532, CONFIG_PN532_LOG_LEVEL);

#include "pn532.h"

#define PN532_MAX_FRAME_SIZE (64)
#define PN532_RX_RING_BUFFER_SIZE (256)

struct pn532_data {
    struct ring_buf rx_ring_buffer;
    uint8_t rx_buffer[PN532_RX_RING_BUFFER_SIZE];
    uint8_t tx_buffer[PN532_MAX_FRAME_SIZE];
};

struct pn532_config {
    const struct device *uart_dev;
};

/* Buffer for building commands to send to PN532 */

/* RX ring buffer for storing incoming data from PN532 */

static void pn532_uart_rx_cb(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    if (dev == NULL) {
        LOG_ERR("Invalid UART device in callback");
        return;
    }

    if (!uart_irq_update(dev)) {
        return;
    }

    if (!uart_irq_rx_ready(dev)) {
        return;
    }

    uint8_t chunk[32];
    int len = uart_fifo_read(dev, chunk, sizeof(chunk));
    if (len <= 0) {
        LOG_WRN("No bytes found in UART FIFO");
        return;
    }
    LOG_HEXDUMP_DBG(chunk, len, "RX chunk");

    struct pn532_data *data = dev->data;
    if (ring_buf_put(&data->rx_ring_buffer, chunk, len) != len) {
        LOG_ERR("Failed to put %d bytes into RX ring buffer", len);
        return;
    }
}

static int get_firmware_version(const struct device *dev, struct pn532_fw_version *version)
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
    version->ic = 0x32;
    version->ver = 0x01;
    version->rev = 0x06;

    return 0;
}

static DEVICE_API(pn532, pn532_api) = {
    .pn532_get_firmware_version = &get_firmware_version,
};

static int pn532_init(const struct device *dev)
{
    const struct pn532_config *config = dev->config;
    struct pn532_data *data = dev->data;
    ring_buf_init(&data->rx_ring_buffer, sizeof(data->rx_buffer), data->rx_buffer);

    if (!device_is_ready(config->uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    uart_irq_callback_set(config->uart_dev, pn532_uart_rx_cb);
    uart_irq_rx_enable(config->uart_dev);

    LOG_DBG("PN532 UART initialized");

    return 0;
}

#define PN532_DEFINE(inst)                             \
    static struct pn532_data data_##inst;              \
                                                       \
    static const struct pn532_config config_##inst = { \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),  \
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
