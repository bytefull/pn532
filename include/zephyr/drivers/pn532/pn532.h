/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PN532_H
#define PN532_H

#include <zephyr/device.h>
#include <zephyr/toolchain.h>

/**
 * @defgroup drivers_pn532 PN532 drivers
 * @ingroup drivers
 * @{
 *
 * @brief A custom driver class to interface with the PN532 NFC module
 *
 * This driver provides an interface to the PN532
 * NFC controller over UART.
 *
 * It exposes a custom device driver API built using Zephyr's __subsystem and
 * z_impl_ mechanism, enabling structured and extendable access to PN532 features.
 */

/**
 * @defgroup drivers_pn532_ops PN532 driver operations
 * @{
 *
 * @brief Operations of the PN532 driver class.
 *
 * Each driver class tipically provides a set of operations that need to be
 * implemented by each driver. These are used to implement the public API. If
 * support for system calls is needed, the operations structure must be tagged
 * with `__subsystem` and follow the `${class}_driver_api` naming scheme.
 */

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
	 * @retval -EIO if communication with the device fails.
	 * @retval -EINVAL if @p version is NULL.
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
	 * @retval 0 if a tag was successfully detected and inlisted.
	 * @retval -EIO if communication with the device fails.
	 * @retval -EINVAL if @p dev is NULL.
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
	 * @retval 0 if data exchange was successful.
	 * @retval -EIO if communication with the device fails.
	 * @retval -EINVAL if any pointer parameter is NULL or if send_length is 0.
	 * @retval -errno Other negative errno codes on failure.
	 */
	int (*pn532_in_data_exchange)(const struct device *dev, uint8_t *send, uint8_t send_length,
				      uint8_t *response, uint8_t *response_length);

	/**
	 * @brief Set the serial baud rate of the PN532 device.
	 *
	 * This function configures the serial baud rate of the PN532 device.
	 *
	 * @param dev Pointer to the PN532 device instance.
	 * @param baudrate The desired baud rate.
	 *
	 * @retval 0 if successful.
	 * @retval -EIO if communication with the device fails.
	 * @retval -EINVAL if @p dev is NULL or @p baudrate is unsupported.
	 * @retval -errno Other negative errno codes on failure.
	 */
	int (*pn532_set_serial_baudrate)(const struct device *dev, uint32_t baudrate);

	int (*pn532_gpio_pin_set)(const struct device *pn532_dev, int value);
	int (*pn532_gpio_pin_get)(const struct device *pn532_dev);
};

/** @} */

/**
 * @defgroup drivers_pn532_api PN532 driver API
 * @{
 *
 * @brief Public API provided by the PN532 driver class.
 *
 * The public API defines the interface used by applications to interact with
 * PN532 NFC devices. If support for system calls is required, API functions
 * must be annotated with `__syscall` and provide a corresponding implementation
 * named `z_impl_<function_name>`, following Zephyr's syscall conventions.
 */

/**
 * @brief Get the firmware version of the PN532 NFC module.
 *
 *
 * @param dev Pointer to the PN532 device instance.
 * @param version Pointer to a uint32_t where the firmware version will be stored.
 *
 * @retval 0 if successful.
 * @retval -EIO if communication with the PN532 fails.
 * @retval -EINVAL if @p dev or @p version is NULL.
 * @retval -errno Other negative errno codes on failure.
 */
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

/**
 * @brief Send the InListPassiveTarget command to the PN532 to detect nearby NFC tags.
 *
 * @param dev Pointer to the PN532 device instance.
 *
 * @retval 0 if a tag was successfully detected and inlisted.
 * @retval -EIO if communication with the device fails.
 * @retval -EINVAL if @p dev is NULL.
 */
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

/**
 * @brief Send the InDataExchange command to the PN532 to exchange data with the inlisted tag.
 *
 * @param dev Pointer to the PN532 device instance.
 * @param send Buffer containing data to send to the tag.
 * @param send_length Length of the data to send.
 * @param response Buffer to store the response from the tag.
 * @param response_length Pointer to a variable that initially contains the size of
 * the response buffer, and is updated with the actual length of the response received.
 *
 * @retval 0 if data exchange was successful.
 * @retval -EIO if communication with the device fails.
 * @retval -EINVAL if any pointer parameter is NULL or if send_length is 0.
 */
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

/**
 * @brief Set the serial baud rate of the PN532 device.
 *
 * @param dev Pointer to the PN532 device instance.
 * @param baudrate The desired baud rate.
 *
 * @retval 0 if successful.
 * @retval -EIO if communication with the device fails.
 * @retval -EINVAL if @p dev is NULL or @p baudrate is unsupported.
 */
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

#include <syscalls/pn532.h>

/** @} */

/** @} */

#endif /* PN532_H */
