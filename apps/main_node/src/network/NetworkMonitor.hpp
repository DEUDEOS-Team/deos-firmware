#pragma once
#include <deos/deos.h>
#include <stdint.h>

class MainApp;

class NetworkMonitor {
public:
    NetworkMonitor();
    
    void update_heartbeat(deos_node_id_t node_id, deos_state_t node_reported_state, deos_state_t expected_state);
    void check_watchdog(MainApp& app);
    void subscribe_heartbeats();

private:
    int64_t last_heartbeat[256];
};
