#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deos/deos.h>
#include <deos/deos_can.h>

#include "bms_gateway.h"
#include "config.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void) {
    LOG_INF("Starting BMS Gateway Application...");
    
    struct deos_config deos_cfg = {
        .node_id = DEOS_NODE_BMS_GATEWAY,
        .router_enabled = false, /* We are handling routing manually for Daly or via hosted nodes */
        .hosted_nodes_enabled = true /* Act on behalf of BMS_MAIN and BMS_AUX */
    };
    
    int err = deos_init(&deos_cfg);
    if (err) {
        LOG_ERR("DEOS Core Init failed: %d", err);
        return err;
    }

    const struct device *can_dev = DEVICE_DT_GET(DEOS_CAN_NODE);
    if (device_is_ready(can_dev)) {
        err = deos_can_transport_init(can_dev);
        if (err) {
            LOG_ERR("CAN transport init failed: %d", err);
            return err;
        }
    } else {
        LOG_WRN("CAN device not ready!");
    }

    /* Gateway arkasindaki BMS'leri lokal node olarak DEOS'a kaydet */
    deos_register_local_node(DEOS_NODE_BMS_MAIN);
    deos_register_local_node(DEOS_NODE_BMS_AUX);
    
    err = deos_start();
    if (err) {
        LOG_ERR("DEOS Core Start failed: %d", err);
        return err;
    }
    deos_can_transport_start();

    err = run_bms_gateway();
    if (err) {
        LOG_ERR("BMS Gateway Logic failed to start: %d", err);
        return err;
    }
    
    return 0;
}
