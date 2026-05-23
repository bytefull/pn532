/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#include "pn532.h"

int main(void)
{
	const struct device *dev = DEVICE_DT_GET_ONE(nxp_pn532);

	if (!device_is_ready(dev)) {
		LOG_ERR("PN532 device not ready");
		return 0;
	}

	/* ---- GetFirmwareVersion ---- */
	struct pn532_fw_version version = {0};
	if (pn532_get_firmware_version(dev, &version) < 0) {
		LOG_ERR("GetFirmwareVersion failed");
		return 0;
	}
	LOG_INF("Found PN5%02X", version.ic);
	LOG_INF("Firmware version: %d.%d", version.ver, version.rev);

	while (1) {
		k_msleep(100);
	}

	return 0;
}
