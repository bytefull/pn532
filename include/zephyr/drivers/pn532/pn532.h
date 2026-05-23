/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PN532_H
#define PN532_H

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

/* GPIO pin numbers for P3 port */
#define PN532_GPIO_P30 BIT(0)
#define PN532_GPIO_P31 BIT(1)
#define PN532_GPIO_P33 BIT(3)
#define PN532_GPIO_P35 BIT(5)

/* Firmware version structure */
struct pn532_fw_version {
	uint8_t ic;  /* PN5xx IC type (e.g. 0x32 for PN532) */
	uint8_t ver; /* Firmware version */
	uint8_t rev; /* Firmware revision */
};

/** @brief PN532 driver class operations */
__subsystem struct pn532_driver_api {
	/**
	 * @brief Retrieve the firmware version of the PN532 module.
	 *
	 * This function queries the PN532 device for its firmware version.
	 * The version is returned as a 32-bit value encoding IC version,
	 * firmware version, and revision.
	 *
	 * @param dev Pointer to the PN532 device instance.
	 * @param version Pointer to a pn532_fw_version struct to store the parsed firmware version.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if parameters are invalid.
	 * @retval -EIO if communication with the PN532 fails.
	 * @retval -ENOTSUP if the API is unsupported.
	 * @retval -errno Other negative errno codes on failure.
	 */
	int (*pn532_get_firmware_version)(const struct device *dev,
					  struct pn532_fw_version *version);

	/**
	 * @brief InListPassiveTarget command to detect and list nearby NFC tags.
	 *
	 * This function sends the InListPassiveTarget command to the PN532 device,
	 * which attempts to detect nearby NFC tags. If a tag is detected, it is
	 * "inlisted" and its information is stored for subsequent interactions.
	 *
	 * @param dev Pointer to the PN532 device instance.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if parameters are invalid.
	 * @retval -EIO if communication with the PN532 fails.
	 * @retval -ENOTSUP if the API is unsupported.
	 * @retval -errno Other negative errno codes on failure.
	 */
	int (*pn532_in_list_passive_target)(const struct device *dev);

	/**
	 * @brief InDataExchange command to exchange data with the inlisted tag.
	 *
	 * This function sends the InDataExchange command to the PN532 device, allowing
	 * the caller to exchange data with the currently inlisted NFC tag. The command
	 * takes a buffer of data to send to the tag and a buffer to receive the response.
	 *
	 * @param dev Pointer to the PN532 device instance.
	 * @param send Buffer containing data to send to the tag.
	 * @param send_length Length of the data to send.
	 * @param response Buffer to store the response from the tag.
	 * @param response_length Pointer to a variable that initially contains the size of
	 * the response buffer, and is updated with the actual length of the response received.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if parameters are invalid.
	 * @retval -EIO if communication with the PN532 fails.
	 * @retval -ENOTSUP if the API is unsupported.
	 * @retval -errno Other negative errno codes on failure.
	 */
	int (*pn532_in_data_exchange)(const struct device *dev, uint8_t *send, uint8_t send_length,
				      uint8_t *response, uint8_t *response_length);

	/**
	 * @brief Set the serial baud rate of the PN532 device.
	 *
	 * @param dev Pointer to the PN532 device instance.
	 * @param baudrate The desired baud rate.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if parameters are invalid.
	 * @retval -EIO if communication with the PN532 fails.
	 * @retval -ENOTSUP if the API is unsupported.
	 * @retval -errno Other negative errno codes on failure.
	 */
	int (*pn532_set_serial_baudrate)(const struct device *dev, uint32_t baudrate);

	/**
	 * @brief Write the PN532 P3 GPIO state.
	 *
	 * This function writes the state of the PN532 P3 GPIO pins
	 * (P30 to P35) using the WriteGPIO command.
	 *
	 * Pins P32 and P34 are reserved by the PN532 and are always
	 * forced high internally by the driver.
	 *
	 * Bit mapping:
	 * - bit 0 -> P30
	 * - bit 1 -> P31
	 * - bit 2 -> P32 (reserved, always forced high)
	 * - bit 3 -> P33
	 * - bit 4 -> P34 (reserved, always forced high)
	 * - bit 5 -> P35
	 *
	 * @param dev Pointer to the PN532 device instance.
	 * @param pins GPIO bitmap to write.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if @p dev is NULL.
	 * @retval -EIO if communication with the PN532 fails.
	 * @retval -ENOTSUP if the API is unsupported.
	 */
	int (*pn532_gpio_write)(const struct device *dev, uint8_t pins);

	/**
	 * @brief Read the PN532 P3 GPIO state.
	 *
	 * This function reads the current state of the PN532 P3 GPIO pins
	 * (P30 to P35) using the ReadGPIO command.
	 *
	 * Bit mapping:
	 * - bit 0 -> P30
	 * - bit 1 -> P31
	 * - bit 2 -> P32
	 * - bit 3 -> P33
	 * - bit 4 -> P34
	 * - bit 5 -> P35
	 *
	 * @param dev Pointer to the PN532 device instance.
	 * @param pins Pointer where the GPIO bitmap will be stored.
	 *
	 * @retval 0 if successful.
	 * @retval -EINVAL if parameters are invalid.
	 * @retval -EIO if communication with the PN532 fails.
	 * @retval -ENOTSUP if the API is unsupported.
	 */
	int (*pn532_gpio_read)(const struct device *dev, uint8_t *pins);
};

__syscall int pn532_get_firmware_version(const struct device *dev,
					 struct pn532_fw_version *version);

static inline int z_impl_pn532_get_firmware_version(const struct device *dev,
						    struct pn532_fw_version *version)
{
	if ((dev == NULL) || (version == NULL)) {
		return -EINVAL;
	}

	if (!DEVICE_API_IS(pn532, dev)) {
		return -ENOTSUP;
	}

	return DEVICE_API_GET(pn532, dev)->pn532_get_firmware_version(dev, version);
}

__syscall int pn532_in_list_passive_target(const struct device *dev);

static inline int z_impl_pn532_in_list_passive_target(const struct device *dev)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	if (!DEVICE_API_IS(pn532, dev)) {
		return -ENOTSUP;
	}

	return DEVICE_API_GET(pn532, dev)->pn532_in_list_passive_target(dev);
}

__syscall int pn532_in_data_exchange(const struct device *dev, uint8_t *send, uint8_t send_length,
				     uint8_t *response, uint8_t *response_length);

static inline int z_impl_pn532_in_data_exchange(const struct device *dev, uint8_t *send,
						uint8_t send_length, uint8_t *response,
						uint8_t *response_length)
{
	if ((dev == NULL) || (send == NULL) || (response == NULL) || (response_length == NULL) ||
	    (send_length == 0)) {
		return -EINVAL;
	}

	if (!DEVICE_API_IS(pn532, dev)) {
		return -ENOTSUP;
	}

	return DEVICE_API_GET(pn532, dev)
		->pn532_in_data_exchange(dev, send, send_length, response, response_length);
}

__syscall int pn532_set_serial_baudrate(const struct device *dev, uint32_t baudrate);

static inline int z_impl_pn532_set_serial_baudrate(const struct device *dev, uint32_t baudrate)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	if (!DEVICE_API_IS(pn532, dev)) {
		return -ENOTSUP;
	}

	return DEVICE_API_GET(pn532, dev)->pn532_set_serial_baudrate(dev, baudrate);
}

__syscall int pn532_gpio_write(const struct device *dev, uint8_t pins);

static inline int z_impl_pn532_gpio_write(const struct device *dev, uint8_t pins)
{
	if (dev == NULL) {
		return -EINVAL;
	}

	if (!DEVICE_API_IS(pn532, dev)) {
		return -ENOTSUP;
	}

	return DEVICE_API_GET(pn532, dev)->pn532_gpio_write(dev, pins);
}

__syscall int pn532_gpio_read(const struct device *dev, uint8_t *pins);

static inline int z_impl_pn532_gpio_read(const struct device *dev, uint8_t *pins)
{
	if ((dev == NULL) || (pins == NULL)) {
		return -EINVAL;
	}

	if (!DEVICE_API_IS(pn532, dev)) {
		return -ENOTSUP;
	}

	return DEVICE_API_GET(pn532, dev)->pn532_gpio_read(dev, pins);
}

#include <syscalls/pn532.h>

/** @} */

/** @} */

#endif /* PN532_H */
