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
    uint32_t version = 0;
    const struct device *dev = DEVICE_DT_GET_ANY(nxp_pn532);

    zassert_not_null(dev, "No PN532 device found");

    zassert_true(device_is_ready(dev), "PN532 device is not ready");

    zassert_equal(-EINVAL, pn532_get_firmware_version(NULL, NULL));

    zassert_equal(-EINVAL, pn532_get_firmware_version(dev, NULL));

    zassert_equal(-EINVAL, pn532_get_firmware_version(NULL, &version));
    zassert_equal(0, version);

    zassert_equal(0, pn532_get_firmware_version(dev, &version));
    zassert_equal(0x01020304, version);
}
