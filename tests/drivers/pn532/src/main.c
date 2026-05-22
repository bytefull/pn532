/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/ztest.h>

#include "pn532.h"

ZTEST_SUITE(pn532, NULL, NULL, NULL, NULL, NULL);

/**
 * @brief Test firmware version retrieval for the PN532 driver
 */
ZTEST(pn532, test_get_firmware_version)
{
	const struct device *dev = DEVICE_DT_GET_ONE(nxp_pn532);
    struct pn532_fw_version version = {0};

    zassert_equal(-EINVAL, pn532_get_firmware_version(NULL, NULL));

    zassert_equal(-EINVAL, pn532_get_firmware_version(dev, NULL));

    zassert_equal(-EINVAL, pn532_get_firmware_version(NULL, &version));
    zassert_equal(0, version.ic);
    zassert_equal(0, version.ver);
    zassert_equal(0, version.rev);
}

/**
 * @brief Test inlist passive target for the PN532 driver
 */
ZTEST(pn532, test_in_list_passive_target)
{
	const struct device *dev = DEVICE_DT_GET_ONE(nxp_pn532);

    zassert_equal(-EINVAL, pn532_in_list_passive_target(NULL));
}

/**
 * @brief Test in data exchange for the PN532 driver
 */
ZTEST(pn532, test_in_data_exchange)
{
	const struct device *dev = DEVICE_DT_GET_ONE(nxp_pn532);
    uint8_t send[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t send_length = sizeof(send);
    uint8_t response[32] = {0};

    zassert_equal(-EINVAL, pn532_in_data_exchange(NULL, NULL, 0, NULL, NULL));

    zassert_equal(-EINVAL, pn532_in_data_exchange(dev, NULL, 0, NULL, NULL));

    zassert_equal(-EINVAL, pn532_in_data_exchange(dev, send, NULL, NULL, NULL));

    zassert_equal(-EINVAL, pn532_in_data_exchange(dev, send, send_length, NULL, NULL));

    zassert_equal(-EINVAL, pn532_in_data_exchange(dev, send, send_length, response, NULL));
}

/**
 * @brief Test set serial baudrate for the PN532 driver
 */
ZTEST(pn532, test_set_serial_baudrate)
{
	const struct device *dev = DEVICE_DT_GET_ONE(nxp_pn532);

    zassert_equal(-EINVAL, pn532_set_serial_baudrate(NULL, 0));
    zassert_equal(-EINVAL, pn532_set_serial_baudrate(dev, 0));
}

/**
 * @brief Test GPIO write to the PN532 driver
 */
ZTEST(pn532, test_gpio_pin_set)
{
    zassert_equal(-EINVAL, pn532_gpio_pin_set(NULL, 0));
}

/**
 * @brief Test GPIO read from the PN532 driver
 */
ZTEST(pn532, test_gpio_pin_get)
{
    zassert_equal(-EINVAL, pn532_gpio_pin_get(NULL));
}
