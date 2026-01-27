/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PN532_TRANSPORT_H
#define PN532_TRANSPORT_H

#include <stdbool.h>
#include <zephyr/device.h>

/**
 * @brief Initialize the PN532 transport layer.
 *
 * This function performs any required hardware initialization for the
 * transport interface (e.g., I2C, SPI, UART) used by the PN532 device.
 *
 * @param dev Pointer to the PN532 device structure.
 *
 * @return 0 on success, negative error code on failure.
 */
int pn532_transport_init(const struct device *dev);

/**
 * @brief Write data to the PN532 over the configured transport.
 *
 * This function transmits a buffer to the PN532 device using the active
 * transport interface.
 *
 * @param dev Pointer to the PN532 device structure.
 * @param buf Pointer to the data buffer to send.
 * @param len Length of the data to be written.
 *
 * @return Number of bytes written on success, negative error code on failure.
 */
int pn532_transport_write(const struct device *dev, const uint8_t *buf, size_t len);

/**
 * @brief Read data from the PN532 over the configured transport.
 *
 * This function reads a buffer from the PN532 device using the active
 * transport interface.
 *
 * @param dev Pointer to the PN532 device structure.
 * @param buf Pointer to the buffer to store received data.
 * @param len Maximum number of bytes to read.
 *
 * @return Number of bytes read on success, negative error code on failure.
 */
int pn532_transport_read(const struct device *dev, uint8_t *buf, size_t len);

/**
 * @brief Check if the PN532 transport is ready.
 *
 * This function indicates whether the transport layer (e.g., I2C, SPI, UART)
 * is ready for communication with the PN532 device.
 *
 * @param dev Pointer to the PN532 device structure.
 * @param timeout_ms Timeout to wait in milliseconds
 *
 * @return true if the transport is ready, false otherwise.
 */
bool pn532_transport_is_ready(const struct device *dev, int timeout_ms);

#endif /* PN532_TRANSPORT_H */
