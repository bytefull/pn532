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
#define PN532_COMMAND_READGPIO            (0x0C) /* Read GPIO */
#define PN532_COMMAND_WRITEGPIO           (0x0E) /* Write GPIO */

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

/* GPIO validation bit */
#define PN532_GPIO_VALIDATIONBIT (0x80)
/* P32 and P34 are not defined in header file because they are reserved and must always stay high */
#define PN532_GPIO_P32 BIT(2)
#define PN532_GPIO_P34 BIT(4)

#define PN532_MAX_FRAME_SIZE      (64)
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
static const uint8_t PN532_EXPECTED_FIRMWARE_VERSION[] = {0x00, 0x00, 0xFF, 0x06, 0xFA, 0xD5};

struct pn532_data {
	struct ring_buf rx_ring_buffer;
	uint8_t rx_buffer[PN532_RX_RING_BUFFER_SIZE];
	uint8_t tx_buffer[PN532_MAX_FRAME_SIZE];
	int8_t in_listed_tag;
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
		return -EINVAL;
	}

	if (data == NULL) {
		LOG_ERR("Invalid data buffer");
		return -EINVAL;
	}
	if (len == 0) {
		LOG_ERR("Invalid data length");
		return -EINVAL;
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
		return -EINVAL;
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
		return -EINVAL;
	}

	/* Check if the command buffer is valid */
	if (cmd == NULL) {
		LOG_ERR("Invalid command buffer");
		return -EINVAL;
	}

	/* Check if the command length is valid */
	if (cmd_len == 0) {
		LOG_ERR("Invalid command length");
		return -EINVAL;
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

static int pn532_send_command(const struct device *pn532_dev, const uint8_t *cmd, size_t cmd_len,
			      int timeout_ms)
{
	uint8_t ack[sizeof(PN532_ACK)] = {0};
	uint32_t len = 0;

	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}

	if (cmd == NULL) {
		LOG_ERR("Invalid command buffer");
		return -EINVAL;
	}

	if (cmd_len == 0) {
		LOG_ERR("Invalid command length");
		return -EINVAL;
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
		return -ETIMEDOUT;
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

static int pn532_wakeup(const struct device *pn532_dev)
{
	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}
	LOG_DBG("Sending wakeup command");
	int ret = pn532_uart_send(pn532_dev, PN532_WAKEUP_SEQ, sizeof(PN532_WAKEUP_SEQ));
#if !defined(CONFIG_BOARD_NATIVE_SIM)
	k_msleep(2);
#endif
	return ret;
}

int pn532_sam_config(const struct device *pn532_dev)
{
	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}

	struct pn532_data *pn532_data = pn532_dev->data;

	LOG_DBG("Sending SAMConfig command");
	pn532_data->tx_buffer[0] = PN532_COMMAND_SAMCONFIGURATION;
	pn532_data->tx_buffer[1] = 0x01; // normal mode;
	pn532_data->tx_buffer[2] = 0x14; // timeout 50ms * 20 = 1 second
	pn532_data->tx_buffer[3] = 0x01; // use IRQ pin!
	if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 4, 1000) < 0) {
		return -EIO;
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
		return -EIO;
	}
	/* Verify response buffer */
	if ((response_buf[0] != PN532_PREAMBLE) || (response_buf[1] != PN532_STARTCODE1) ||
	    (response_buf[2] != PN532_STARTCODE2)) {
		LOG_ERR("Preamble missing");
		return -EIO;
	}
	/* I don't know what is byte 4 used for */
	if (response_buf[5] != PN532_PN532TOHOST) {
		LOG_ERR("Invalid SAMConfig response: 0x%02X", response_buf[5]);
		return -EIO;
	}
	if (response_buf[6] != 0x15) {
		LOG_ERR("Invalid SAMConfig response: 0x%02X", response_buf[6]);
		return -EIO;
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
		return -EIO;
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
		return -EIO;
	}
	/* Verify response buffer */
	if ((response_buf[5] != PN532_PN532TOHOST) ||
	    (response_buf[6] != PN532_RESPONSE_GETFIRMWAREVERSION)) {
		LOG_ERR("Unexpected response to GetFirmwareVersion");
		return -EIO;
	}
	if (memcmp(response_buf, PN532_EXPECTED_FIRMWARE_VERSION,
		   sizeof(PN532_EXPECTED_FIRMWARE_VERSION)) != 0) {
		LOG_ERR("Unexpected firmware version response");
		return -EIO;
	}

	version->ic = response_buf[7];
	version->ver = response_buf[8];
	version->rev = response_buf[9];
	LOG_DBG("GetFirmwareVersion OK");

	return 0;
}

static int in_list_passive_target(const struct device *pn532_dev)
{
	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}

	struct pn532_data *pn532_data = pn532_dev->data;

	LOG_DBG("Sending InListPassiveTarget command");
	pn532_data->tx_buffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
	pn532_data->tx_buffer[1] = 0x01;
	pn532_data->tx_buffer[2] = 0x00;
	if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 3, 1200) < 0) {
		return -EIO;
	}
	/* Wait for a little bit until we receive the 24 bytes of the InListPassiveTarget response
	 */
	if (pn532_wait_for_rx(pn532_dev, 24, 1200) < 0) {
		LOG_ERR("Timeout waiting for InListPassiveTarget response");
		return -ETIMEDOUT;
	}
	/* Read the inListPassiveTarget response */
	uint8_t response_buf[24] = {0};
	if (ring_buf_get(&pn532_data->rx_ring_buffer, response_buf, 24) != 24) {
		LOG_ERR("Failed to read inListPassiveTarget response");
		return -EIO;
	}
	/* Verify response preamble */
	if ((response_buf[0] != PN532_PREAMBLE) || (response_buf[1] != PN532_STARTCODE1) ||
	    (response_buf[2] != PN532_STARTCODE2)) {
		LOG_ERR("Preamble missing");
		return -EIO;
	}
	/* Verify response length */
	uint8_t length = response_buf[3];
	if (response_buf[4] != (uint8_t)(~length + 1)) {
		LOG_ERR("Length check invalid");
		LOG_DBG("Expected: 0x%02X, Got: 0x%02X", (uint8_t)(~length + 1), response_buf[4]);
		return -EIO;
	}
	/* Verify response code */
	if ((response_buf[5] != PN532_PN532TOHOST) ||
	    (response_buf[6] != PN532_RESPONSE_INLISTPASSIVETARGET)) {
		LOG_ERR("Unexpected response to inlist passive host");
		return -EIO;
	}
	/* Verify number of targets inlisted */
	if (response_buf[7] != 1) {
		LOG_ERR("Unhandled number of targets inlisted");
		LOG_DBG("Number of tags inlisted: 0x%02X", response_buf[7]);
		return -EIO;
	}
	/* Save the listed tag */
	pn532_data->in_listed_tag = response_buf[8];
	LOG_DBG("Tag number: %d", pn532_data->in_listed_tag);
	LOG_DBG("InListPassiveTarget OK");
	return 0;
}

static int in_data_exchange(const struct device *pn532_dev, uint8_t *send, uint8_t send_length,
			    uint8_t *response, uint8_t *response_length)
{
	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}

	if (send == NULL) {
		LOG_ERR("Invalid command buffer");
		return -EINVAL;
	}

	if (send_length == 0) {
		LOG_ERR("Invalid command length");
		return -EINVAL;
	}

	if (response == NULL) {
		LOG_ERR("Invalid response buffer");
		return -EINVAL;
	}

	if ((response_length == NULL) || (*response_length == 0)) {
		LOG_ERR("Invalid response length");
		return -EINVAL;
	}

	LOG_DBG("Sending InDataExchange command");
	struct pn532_data *pn532_data = pn532_dev->data;

	pn532_data->tx_buffer[0] = PN532_COMMAND_INDATAEXCHANGE;
	pn532_data->tx_buffer[1] = pn532_data->in_listed_tag;
	for (uint8_t i = 0; i < send_length; ++i) {
		pn532_data->tx_buffer[i + 2] = send[i];
	}
	if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, send_length + 2, 1000) < 0) {
		return -EIO;
	}
	/* We can ignore the timeout here and only raise a warning
	 * since the user doesn't necessarily know
	 * how many bytes to expect in the response
	 */
	if (pn532_wait_for_rx(pn532_dev, *response_length + 10, 1000) < 0) {
		LOG_WRN("Timeout waiting for inDataExchange response, expected %d bytes but got %d "
			"bytes",
			*response_length + 10, ring_buf_size_get(&pn532_data->rx_ring_buffer));
	}
	/* We can also ignore the length check here and only raise a warning
	 * since the user doesn't necessarily know
	 * how many bytes to expect in the response
	 */
	uint8_t response_buf[128] = {0};
	uint32_t len =
		ring_buf_get(&pn532_data->rx_ring_buffer, response_buf, sizeof(response_buf));
	if (len != *response_length + 10) {
		LOG_WRN("Failed to read inDataExchange response, Expected %d bytes but got %d",
			*response_length + 10, len);
	}
	/* Verify response preamble */
	if ((response_buf[0] != PN532_PREAMBLE) || (response_buf[1] != PN532_STARTCODE1) ||
	    (response_buf[2] != PN532_STARTCODE2)) {
		LOG_ERR("Invalid response preamble");
		return -EIO;
	}
	/* Verify response length */
	uint8_t length = response_buf[3];
	if (response_buf[4] != (uint8_t)(~length + 1)) {
		LOG_ERR("Length check invalid");
		LOG_DBG("Expected: 0x%02X, Got: 0x%02X", (uint8_t)(~length + 1), response_buf[4]);
		return -EIO;
	}
	/* Verify response code */
	if ((response_buf[5] != PN532_PN532TOHOST) ||
	    (response_buf[6] != PN532_RESPONSE_INDATAEXCHANGE)) {
		LOG_ERR("Invalid response code");
		return -EIO;
	}
	/* Verify status code */
	if ((response_buf[7] & 0x3f) != 0) {
		LOG_ERR("Status code indicates an error, expected 0x00 but got 0x%02X",
			response_buf[7] & 0x3f);
		return -EIO;
	}
	/* Save response length */
	length -= 3;
	if (length > *response_length) {
		length = *response_length;
	}
	*response_length = length;
	/* Save response data */
	for (uint8_t i = 0; i < length; ++i) {
		response[i] = response_buf[8 + i];
	}
	LOG_DBG("InDataExchange OK");
	return 0;
}

static int set_serial_baudrate(const struct device *pn532_dev, uint32_t baudrate)
{
	uint8_t baudrate_code;

	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}
	struct pn532_data *pn532_data = pn532_dev->data;

	switch (baudrate) {
	case 9600:
		baudrate_code = PN532_BAUDRATE_9600;
		break;
	case 19200:
		baudrate_code = PN532_BAUDRATE_19200;
		break;
	case 38400:
		baudrate_code = PN532_BAUDRATE_38400;
		break;
	case 57600:
		baudrate_code = PN532_BAUDRATE_57600;
		break;
	case 115200:
		baudrate_code = PN532_BAUDRATE_115200;
		break;
	case 230400:
		baudrate_code = PN532_BAUDRATE_230400;
		break;
	case 460800:
		baudrate_code = PN532_BAUDRATE_460800;
		break;
	case 921600:
		baudrate_code = PN532_BAUDRATE_921600;
		break;
	default:
		LOG_ERR("Unsupported baud rate: %d", baudrate);
		return -EINVAL;
	}

	LOG_DBG("Sending SetSerialBaudRate command");
	pn532_data->tx_buffer[0] = PN532_COMMAND_SETSERIALBAUDRATE;
	pn532_data->tx_buffer[1] = baudrate_code;
	if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 2, 100) < 0) {
		return -EIO;
	}
	/* Wait for a little bit until we receive the 8 bytes: size of the SetSerialBaudRate
	 * response */
	if (pn532_wait_for_rx(pn532_dev, 8, 100) < 0) {
		LOG_ERR("Timeout waiting for SetSerialBaudRate response");
		return -ETIMEDOUT;
	}
	// /* Read the SetSerialBaudRate response */
	uint8_t response_buf[8] = {0};
	if (ring_buf_get(&pn532_data->rx_ring_buffer, response_buf, 8) != 8) {
		LOG_ERR("Failed to read SetSerialBaudRate response");
		return -EIO;
	}
	/* Verify response buffer */
	if ((response_buf[0] != PN532_PREAMBLE) || (response_buf[1] != PN532_STARTCODE1) ||
	    (response_buf[2] != PN532_STARTCODE2)) {
		LOG_ERR("Preamble missing");
		return -EIO;
	}

	if ((response_buf[5] != PN532_PN532TOHOST) ||
	    (response_buf[6] != PN532_RESPONSE_SETSERIALBAUDRATE)) {
		LOG_ERR("Invalid SetSerialBaudRate response: 0x%02X", response_buf[5]);
		return -EIO;
	}

	/* Per datasheet: PN532 switches AFTER receiving our ACK.
	 * Send ACK at 115200, then switch host side. */
	pn532_uart_send(pn532_dev, PN532_ACK, sizeof(PN532_ACK));

	/* Wait >= 200 µs as required by the datasheet before next command */
	k_usleep(500);

	/* Switch host UART to the new baud rate */
	const struct pn532_config *config = pn532_dev->config;
	const struct device *uart_dev = config->uart_dev;
	struct uart_config cfg = {0};
	int ret = uart_config_get(uart_dev, &cfg);
	if (ret) {
		LOG_ERR("uart_config_get failed: %d", ret);
		return -1;
	}
	cfg.baudrate = baudrate;
	ret = uart_configure(uart_dev, &cfg);
	if (ret) {
		LOG_ERR("uart_configure(%d) failed: %d", baudrate, ret);
		return -1;
	}
	LOG_INF("Host UART switched to %d", baudrate);

	/* Flush any garbage from the baud-rate transition */
	k_msleep(2);

	LOG_DBG("SetSerialBaudRate OK");
	return 0;
}

static int gpio_write(const struct device *pn532_dev, uint8_t pins)
{
	if (pn532_dev == NULL) {
		LOG_ERR("Invalid PN532 device pointer");
		return -EINVAL;
	}

	struct pn532_data *pn532_data = pn532_dev->data;

	/* P32 and P34 are reserved and must always stay high */
	pins |= BIT(PN532_GPIO_P32) | BIT(PN532_GPIO_P34);

	LOG_DBG("Sending WriteGPIO command");

	pn532_data->tx_buffer[0] = PN532_COMMAND_WRITEGPIO;
	pn532_data->tx_buffer[1] = PN532_GPIO_VALIDATIONBIT | pins;
	/* We don't want to control P7 port so we put 0x00 here */
	pn532_data->tx_buffer[2] = 0x00;
	if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 3, 1000) < 0) {
		return -EIO;
	}

	/* Response size = 8 bytes */
	if (pn532_wait_for_rx(pn532_dev, 8, 1000) < 0) {
		LOG_ERR("Timeout waiting for WriteGPIO response");
		return -ETIMEDOUT;
	}

	uint8_t response_buf[8] = {0};
	if (ring_buf_get(&pn532_data->rx_ring_buffer, response_buf,
			 sizeof(response_buf)) != sizeof(response_buf)) {
		LOG_ERR("Failed to read WriteGPIO response");
		return -EIO;
	}

	/* Verify response */
	if ((response_buf[5] != PN532_PN532TOHOST) ||
	    (response_buf[6] != (PN532_COMMAND_WRITEGPIO + 1))) {
		LOG_ERR("Invalid WriteGPIO response");
		return -EIO;
	}

	LOG_DBG("WriteGPIO OK");

	return 0;
}

static int gpio_read(const struct device *pn532_dev, uint8_t *pins)
{
	if ((pn532_dev == NULL) || (pins == NULL)) {
		LOG_ERR("Invalid parameter");
		return -EINVAL;
	}

	struct pn532_data *pn532_data = pn532_dev->data;

	LOG_DBG("Sending ReadGPIO command");

	pn532_data->tx_buffer[0] = PN532_COMMAND_READGPIO;
	if (pn532_send_command(pn532_dev, pn532_data->tx_buffer, 1, 1000) < 0) {
		return -EIO;
	}

	/* Response size = 11 bytes */
	if (pn532_wait_for_rx(pn532_dev, 11, 1000) < 0) {
		LOG_ERR("Timeout waiting for ReadGPIO response");
		return -ETIMEDOUT;
	}

	uint8_t response_buf[11] = {0};
	if (ring_buf_get(&pn532_data->rx_ring_buffer, response_buf,
			 sizeof(response_buf)) != sizeof(response_buf)) {
		LOG_ERR("Failed to read ReadGPIO response");
		return -EIO;
	}

	/* Verify response */
	if ((response_buf[5] != PN532_PN532TOHOST) ||
	    (response_buf[6] != (PN532_COMMAND_READGPIO + 1))) {
		LOG_ERR("Invalid ReadGPIO response");
		return -EIO;
	}

	*pins = response_buf[7];

	LOG_DBG("ReadGPIO OK: 0x%02X", *pins);

	return 0;
}

static DEVICE_API(pn532, pn532_api) = {
	.pn532_get_firmware_version = &get_firmware_version,
	.pn532_in_list_passive_target = &in_list_passive_target,
	.pn532_in_data_exchange = &in_data_exchange,
	.pn532_set_serial_baudrate = &set_serial_baudrate,
	.pn532_gpio_write = &gpio_write,
	.pn532_gpio_read = &gpio_read,
};

static int pn532_init(const struct device *dev)
{
	const struct pn532_config *config = dev->config;
	struct pn532_data *pn532_data = dev->data;

	pn532_data->in_listed_tag = -1;
	ring_buf_init(&pn532_data->rx_ring_buffer, sizeof(pn532_data->rx_buffer),
		      pn532_data->rx_buffer);

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

#define PN532_DEFINE(inst)                                                                         \
	static struct pn532_data data_##inst;                                                      \
                                                                                                   \
	static const struct pn532_config config_##inst = {                                         \
		.uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)),                                      \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst, pn532_init, NULL, &data_##inst, &config_##inst, POST_KERNEL,   \
			      CONFIG_PN532_INIT_PRIORITY, &pn532_api);

DT_INST_FOREACH_STATUS_OKAY(PN532_DEFINE)
