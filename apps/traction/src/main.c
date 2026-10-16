#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deos/deos.h>

#include "traction_app.h"
#include "traction_config.h"
#include "traction_hw.h"
#include "traction_control.h"

LOG_MODULE_REGISTER(traction_main, LOG_LEVEL_INF);

extern void traction_handlers_init(void);

int main(void)
{
    LOG_INF("Starting Traction Node Initialization...");
    
    // 1. App layer init
    traction_app_init();
    traction_config_init();
    traction_hw_init();
    
    // 2. DEOS core config and init
    struct deos_config d_cfg = {
        .node_id = DEOS_NODE_TRACTION,
        .router_enabled = false,
        .hosted_nodes_enabled = false
    };
    
    if (deos_init(&d_cfg) < 0) {
        LOG_ERR("Failed to initialize DEOS core");
        return -1;
    }
    
    // 3. Register Application handlers
    traction_handlers_init();
    
    // 4. Start Control Loop
    traction_control_init();
    
    // 5. Start DEOS
    if (deos_start() < 0) {
        LOG_ERR("Failed to start DEOS core");
        return -1;
    }
    
    LOG_INF("Traction Node Running.");
    
    // System automatically handles heartbeat and background tasks
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}
