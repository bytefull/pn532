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
