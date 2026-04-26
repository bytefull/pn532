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

/* PN532 frame fields */
#define PN532_PREAMBLE    (0x00) /* Command sequence start, byte 1/3 */
#define PN532_STARTCODE1  (0x00) /* Command sequence start, byte 2/3 */
#define PN532_STARTCODE2  (0xFF) /* Command sequence start, byte 3/3 */
#define PN532_POSTAMBLE   (0x00) /* EOD */
#define PN532_HOSTTOPN532 (0xD4) /* Host-to-PN532 */
#define PN532_PN532TOHOST (0xD5) /* PN532-to-host */

/* PN532 Commands */
#define PN532_COMMAND_GETFIRMWAREVERSION  (0x02) /* Get firmware version */
#define PN532_COMMAND_SAMCONFIGURATION    (0x14) /* SAM configuration */
#define PN532_COMMAND_INLISTPASSIVETARGET (0x4A) /* List passive target */
#define PN532_COMMAND_INDATAEXCHANGE      (0x40) /* Data exchange */
#define PN532_COMMAND_SETSERIALBAUDRATE   (0x10) /* Set serial baud rate */

/* PN532 Responses */
#define PN532_RESPONSE_INLISTPASSIVETARGET (0x4B) /* List passive target */
#define PN532_RESPONSE_INDATAEXCHANGE      (0x41) /* Data exchange */
#define PN532_RESPONSE_SETSERIALBAUDRATE   (0x11) /* Set serial baud rate */
#define PN532_RESPONSE_GETFIRMWAREVERSION  (0x03) /* Get firmware version */

/**
 * @brief Baud rate values for the PN532.
 * BR is a byte indicating the baud rate requested by the host controller:
 *     − 0x00: 9.6 kbaud,
 *     − 0x01: 19.2 kbaud,
 *     − 0x02: 38.4 kbaud,
 *     − 0x03: 57.6 kbaud,
 *     − 0x04: 115.2 kbaud,
 *     − 0x05: 230.4 kbaud,
 *     − 0x06: 460.8 kbaud,
 *     − 0X07: 921.6 kbaud,
 *     − 0x08: 1.288 Mbaud.
 */
#define PN532_BAUDRATE_9600    (0x00) /* 9.6 kbaud */
#define PN532_BAUDRATE_19200   (0x01) /* 19.2 kbaud */
#define PN532_BAUDRATE_38400   (0x02) /* 38.4 kbaud */
#define PN532_BAUDRATE_57600   (0x03) /* 57.6 kbaud */
#define PN532_BAUDRATE_115200  (0x04) /* 115.2 kbaud */
#define PN532_BAUDRATE_230400  (0x05) /* 230.4 kbaud */
#define PN532_BAUDRATE_460800  (0x06) /* 460.8 kbaud */
#define PN532_BAUDRATE_921600  (0x07) /* 921600 baud */
#define PN532_BAUDRATE_1288000 (0x08) /* 1.288 Mbaud */

#define PN532_DEFAULT_BAUDRATE (PN532_BAUDRATE_115200)

#define PN532_MAX_FRAME_SIZE (64)
#define PN532_RX_RING_BUFFER_SIZE (256)

/* ACK frame */
static const uint8_t PN532_ACK[] = {
    0x00, /* Preamble */
    0x00, /* Start code 1 */
    0xFF, /* Start code 2 */
    0x00, /* LEN  = 0 (no payload) */
    0xFF, /* LCS  = 0x100 - LEN = 0x100 - 0x00 = 0xFF */
    0x00  /* Postamble */
};

/* Wake-up sequence */
static const uint8_t PN532_WAKEUP_SEQ[] = {
    0x55, /* Dummy byte (wakeup pattern, generates clock edges) */
    0x55, /* Dummy byte (ensures PN532 exits power-down) */
    0x00, /* Preamble (start of a "fake" frame) */
    0x00, /* Start code 1 */
    0x00  /* (Not a valid frame, just padding / noise) */
};

/* Expected firmware version message from PN532 */
static const uint8_t PN532_EXPECTED_FIRMWARE_VERSION[] = {
    0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD5
};

struct pn532_data {
    struct ring_buf rx_ring_buffer;
    uint8_t rx_buffer[PN532_RX_RING_BUFFER_SIZE];
    uint8_t tx_buffer[PN532_MAX_FRAME_SIZE];
};

struct pn532_config {
    const struct device *uart_dev;
};

static void pn532_uart_rx_cb(const struct device *uart_dev, void *user_data)
{
    if (uart_dev == NULL) {
        LOG_ERR("Invalid UART device in callback");
        return;
    }

    if (!uart_irq_update(uart_dev)) {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }

    const struct device *pn532_dev = user_data;
    struct pn532_data *pn532_data = pn532_dev->data;

    uint8_t chunk[32];
    int len = uart_fifo_read(uart_dev, chunk, sizeof(chunk));
    if (len <= 0) {
        LOG_WRN("No bytes found in UART FIFO");
        return;
    }
    LOG_HEXDUMP_DBG(chunk, len, "RX chunk");

    if (ring_buf_put(&pn532_data->rx_ring_buffer, chunk, len) != len) {
        return;
    }
}

static int pn532_uart_send(const struct device *pn532_dev, const uint8_t *data, uint32_t len)
{
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -1;
    }

    if (data == NULL) {
        LOG_ERR("Invalid data buffer");
        return -1;
    }
    if (len == 0) {
        LOG_ERR("Invalid data length");
        return -1;
    }

    const struct pn532_config *config = pn532_dev->config;
    const struct device *uart_dev = config->uart_dev;

    for (uint32_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, data[i]);
    }

    LOG_HEXDUMP_DBG(data, len, "TX");

    return 0;
}

static int pn532_wait_for_rx(const struct device *pn532_dev, size_t expected_len, int timeout_ms)
{
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -1;
    }

    uint32_t rx_len = 0;
    struct pn532_data *pn532_data = pn532_dev->data;
    int64_t end = k_uptime_get() + timeout_ms;

    while (k_uptime_get() < end) {
        rx_len = ring_buf_size_get(&pn532_data->rx_ring_buffer);
        if (rx_len >= expected_len) {
            return 0;
        }
        k_sleep(K_USEC(100));
    }

    return -ETIMEDOUT;
}

static int pn532_write_command(const struct device *pn532_dev, uint8_t *cmd, uint8_t cmd_len)
{
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -1;
    }

    /* Check if the command buffer is valid */
    if (cmd == NULL) {
        LOG_ERR("Invalid command buffer");
        return -1;
    }

    /* Check if the command length is valid */
    if (cmd_len == 0) {
        LOG_ERR("Invalid command length");
        return -1;
    }

    /* Allocate full frame: 8 bytes overhead + command length */
    uint8_t frame[8 + cmd_len];

    /* The length should be the command length plus 1 for TFI */
    uint8_t len = cmd_len + 1;

    /* Frame header */
    frame[0] = PN532_PREAMBLE;
    frame[1] = PN532_STARTCODE1;
    frame[2] = PN532_STARTCODE2;

    /* Frame length and length checksum */
    frame[3] = len;
    frame[4] = ~len + 1;

    /* Frame identifier */
    frame[5] = PN532_HOSTTOPN532;

    /*
     * Copy command payload into frame and compute checksum
     * Checksum is computed over TFI + DATA
     */
    uint8_t sum = 0;
    for (uint8_t i = 0; i < cmd_len; i++) {
      frame[6 + i] = cmd[i];
      sum += cmd[i];
    }
    frame[6 + cmd_len] = ~(PN532_HOSTTOPN532 + sum) + 1;

    /* Postamble */
    frame[7 + cmd_len] = PN532_POSTAMBLE;

    /* Send the complete frame */
    pn532_uart_send(pn532_dev, frame, 8 + cmd_len);

    return 0;
}

static int pn532_send_command(const struct device *pn532_dev, const uint8_t *cmd, size_t cmd_len, int timeout_ms)
{
    uint8_t ack[sizeof(PN532_ACK)] = {0};
    uint32_t len = 0;
    
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -1;
    }

    if (cmd == NULL) {
        LOG_ERR("Invalid command buffer");
        return -1;
    }

    if (cmd_len == 0) {
        LOG_ERR("Invalid command length");
        return -1;
    }

    struct pn532_data *pn532_data = pn532_dev->data;

    /* Flush the RX ring buffer each time we want to send a command */
    ring_buf_reset(&pn532_data->rx_ring_buffer);

    /* Send command */
    if (pn532_write_command(pn532_dev, (uint8_t *)cmd, cmd_len) < 0) {
        return -1;
    }

    /* Wait for 6 bytes of ACK */
    if (pn532_wait_for_rx(pn532_dev, sizeof(ack), timeout_ms) < 0) {
        LOG_ERR("Timeout waiting for ACK");
        return -1;
    }

    /* Read the ACK bytes */
    len = ring_buf_get(&pn532_data->rx_ring_buffer, ack, sizeof(ack));
    if (len != sizeof(ack)) {
        LOG_ERR("Failed to read ACK, expected %d bytes but got %d", sizeof(ack), len);
        return -1;
    }

    /* Check if the received bytes match the expected ACK */
    if (memcmp(ack, PN532_ACK, sizeof(PN532_ACK)) != 0) {
        LOG_ERR("Invalid ACK");
        return -1;
    }
    LOG_DBG("ACK received");

    /* Did the response already start arriving right after reading the ACK? */
    if (ring_buf_size_get(&pn532_data->rx_ring_buffer)) {
        LOG_DBG("Response already started arriving right after ACK");
        return 0;
    }

    /* Otherwise wait for a little bit more for the response to arrive */
    if (pn532_wait_for_rx(pn532_dev, 1, timeout_ms) < 0) {
        LOG_ERR("Timeout waiting for response");
        return -ETIMEDOUT;
    }

    LOG_DBG("Response received");
    return 0;
}

static int pn532_wakeup(const struct device *pn532_dev) {
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -1;
    }
    LOG_DBG("Sending wakeup command");
    int ret = pn532_uart_send(pn532_dev, PN532_WAKEUP_SEQ, sizeof(PN532_WAKEUP_SEQ));
#if !defined(CONFIG_BOARD_NATIVE_SIM)
    k_msleep(2);
#endif
    return ret;
}

int pn532_sam_config(const struct device *pn532_dev) {
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -1;
    }

    struct pn532_data *pn532_data = pn532_dev->data;

    LOG_DBG("Sending SAMConfig command");
    pn532_data->tx_buffer[0] = PN532_COMMAND_SAMCONFIGURATION;
    pn532_data->tx_buffer[1] = 0x01; // normal mode;
    pn532_data->tx_buffer[2] = 0x14; // timeout 50ms * 20 = 1 second
    pn532_data->tx_buffer[3] = 0x01; // use IRQ pin!
    if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 4, 1000) < 0) {
        return -1;
    }
    /* Wait for a little bit until we receive the 9 bytes: size of the SAMConfig response */
    if (pn532_wait_for_rx(pn532_dev, 9, 1000) < 0) {
        LOG_ERR("Timeout waiting for SAMConfig response");
        return -ETIMEDOUT;
    }
    /* Read the SAMConfig response */
    uint8_t response_buf[9] = {0};
    if (ring_buf_get(&pn532_data->rx_ring_buffer, response_buf, 9) != 9) {
        LOG_ERR("Failed to read SAMConfig response");
        return -1;
    }
    /* Verify response buffer */
    if ((response_buf[0] != PN532_PREAMBLE) ||
        (response_buf[1] != PN532_STARTCODE1) ||
        (response_buf[2] != PN532_STARTCODE2)) {
        LOG_ERR("Preamble missing");
        return -1;
    }
    /* I don't know what is byte 4 used for */
    if (response_buf[5] != PN532_PN532TOHOST) {
        LOG_ERR("Invalid SAMConfig response: 0x%02X", response_buf[5]);
        return -1;
    }
    if (response_buf[6] != 0x15) {
        LOG_ERR("Invalid SAMConfig response: 0x%02X", response_buf[6]);
        return -1;
    }

    LOG_DBG("SAMConfig OK");
    return 0;
}

static int get_firmware_version(const struct device *pn532_dev, struct pn532_fw_version *version)
{
    if (pn532_dev == NULL) {
        LOG_ERR("Invalid PN532 device pointer");
        return -EINVAL;
    }

    if (version == NULL) {
        LOG_ERR("Invalid firmware version struct pointer");
        return -EINVAL;
    }

    struct pn532_data *pn532_data = pn532_dev->data;

    LOG_DBG("Sending GetFirmwareVersion command");
    pn532_data->tx_buffer[0] = PN532_COMMAND_GETFIRMWAREVERSION;
    if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 1, 1000) < 0) {
        return -1;
    }
    /* Wait for a little bit until we receive the 13 bytes of the GetFirmwareVersion response */
    if (pn532_wait_for_rx(pn532_dev, 13, 100) < 0) {
        LOG_ERR("Timeout waiting for GetFirmwareVersion response");
        return -ETIMEDOUT;
    }
    /* Read the GetFirmwareVersion response */
    uint8_t response_buf[13] = {0};
    if (ring_buf_get(&pn532_data->rx_ring_buffer, response_buf, 13) != 13) {
        LOG_ERR("Failed to read GetFirmwareVersion response");
        return -1;
    }
    /* Verify response buffer */
    if ((response_buf[5] != PN532_PN532TOHOST) ||
        (response_buf[6] != PN532_RESPONSE_GETFIRMWAREVERSION)) {
        LOG_ERR("Unexpected response to GetFirmwareVersion");
        return -1;
    }
    if (memcmp(response_buf,
               PN532_EXPECTED_FIRMWARE_VERSION,
               sizeof(PN532_EXPECTED_FIRMWARE_VERSION)) != 0) {
        LOG_ERR("Unexpected firmware version response");
        return -1;
    }

    version->ic = response_buf[7];
    version->ver = response_buf[8];
    version->rev = response_buf[9];
    LOG_DBG("GetFirmwareVersion OK");

    return 0;
}

static DEVICE_API(pn532, pn532_api) = {
    .pn532_get_firmware_version = &get_firmware_version,
};

static int pn532_init(const struct device *dev)
{
    const struct pn532_config *config = dev->config;
    struct pn532_data *pn532_data = dev->data;

    ring_buf_init(&pn532_data->rx_ring_buffer, sizeof(pn532_data->rx_buffer), pn532_data->rx_buffer);

    if (!device_is_ready(config->uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }

    uart_irq_callback_user_data_set(config->uart_dev, pn532_uart_rx_cb, (void *)dev);
    uart_irq_rx_enable(config->uart_dev);

    pn532_wakeup(dev);

    if (pn532_sam_config(dev) < 0) {
        LOG_ERR("SAMConfig failed after wakeup");
        return -1;
    }

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
