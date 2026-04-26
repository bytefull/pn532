#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include "pn532.h"

int main(void)
{
    const struct device *dev = DEVICE_DT_GET_ONE(nxp_pn532);

    if (!device_is_ready(dev)) {
        LOG_INF("PN532 device not ready");
        return EXIT_FAILURE;
    }

    /* ---- Wakeup ---- */
    pn532_wakeup();

    /* ---- SAMConfig ---- */
    if (!pn532_sam_config()) {
        LOG_ERR("SAMConfig failed");
        return 0;
    }

    /* ---- GetFirmwareVersion ---- */
    struct pn532_fw_version fw_version = {0};
    if (!pn532_get_firmware_version(&fw_version)) {
        LOG_ERR("GetFirmwareVersion failed");
        return 0;
    }
    LOG_INF("Found PN5%02X", fw_version.ic);
    LOG_INF("Firmware version: %d.%d", fw_version.ver, fw_version.rev);

    while (1)
    {
        k_msleep(100);
    }

    return 0;
}
