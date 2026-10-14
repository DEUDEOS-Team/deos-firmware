#include "NetworkMonitor.hpp"
#include "../MainApp.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main_node_app);

NetworkMonitor::NetworkMonitor() {
    for (int i = 0; i < 256; i++) {
        last_heartbeat[i] = 0;
    }
}

void NetworkMonitor::update_heartbeat(deos_node_id_t node_id, deos_state_t node_reported_state, deos_state_t expected_state) {
    last_heartbeat[node_id] = k_uptime_get();

    /* Eğer noddan state bilgisi gelmediyse veya unknown ise kontrol etme */
    if (node_reported_state == DEOS_STATE_UNKNOWN) {
        return;
    }

    /* Gateway'in global state'i ile nodun kendi raporladığı state uyuşmuyor mu? */
    if (node_reported_state != expected_state) {
        LOG_WRN("Node 0x%02X state mismatch! Expected: 0x%02X, Node: 0x%02X. Enforcing...", 
                node_id, expected_state, node_reported_state);
                
        /* Uyuşmayan noda özel (Unicast) yüksek öncelikli State emri göndererek ikna et */
        deos_priority_t prio = (expected_state == DEOS_STATE_SAFE || expected_state == DEOS_STATE_FAULT) 
                                ? DEOS_PRIO_EMERGENCY : DEOS_PRIO_CRITICAL_CONTROL;

        deos_send(node_id, prio, DEOS_CLASS_COMMAND, DEOS_SERVICE_SYSTEM, 
                  DEOS_CMD_SYSTEM_SET_STATE, &expected_state, sizeof(expected_state));
    }
}

void NetworkMonitor::check_watchdog(MainApp& app) {
    int64_t now = k_uptime_get();

    /* Sistemi kitleyecek kritik nodlar: Motor ve Ana Batarya */
    deos_node_id_t critical_nodes[] = {DEOS_NODE_TRACTION, DEOS_NODE_BMS_MAIN};

    for (size_t i = 0; i < ARRAY_SIZE(critical_nodes); i++) {
        deos_node_id_t n = critical_nodes[i];

        /* Eğer nod önceden bağlandıysa (0 değilse) ve üzerinden 2500 ms geçtiyse */
        if (last_heartbeat[n] != 0 && (now - last_heartbeat[n] > 2500)) {
            LOG_ERR("!!! NETWORK WATCHDOG TIMEOUT !!! Node 0x%02X lost!", n);

            /* Nod koptuğu an sistemi acil durum (SAFE) moduna alıyoruz */
            app.set_state(DEOS_STATE_SAFE);

            /* Sürekli aynı hatayı basmamak için sıfırlıyoruz */
            last_heartbeat[n] = 0;
        }
    }
}
