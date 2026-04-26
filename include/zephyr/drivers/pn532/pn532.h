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
    int (*pn532_get_firmware_version)(const struct device *dev, struct pn532_fw_version *version);
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
__syscall int pn532_get_firmware_version(const struct device *dev, struct pn532_fw_version *version);

static inline int z_impl_pn532_get_firmware_version(const struct device *dev, struct pn532_fw_version *version)
{
    if ((dev == NULL) || (version == NULL)) {
        return -EINVAL;
    }

    if (!DEVICE_API_IS(pn532, dev)) {
        return -ENOTSUP;
    }

    return DEVICE_API_GET(pn532, dev)->pn532_get_firmware_version(dev, version);
}

#include <syscalls/pn532.h>

/** @} */

/** @} */

#endif /* PN532_H */
