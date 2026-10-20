#pragma once

#include <deos/deos.h>
#include <zephyr/sys/atomic.h>
#include "io/LedController.hpp"
#include "io/SafetyManager.hpp"
#include "network/NetworkMonitor.hpp"

class MainApp {
public:
    static MainApp& getInstance();
    
    int init();
    void run();

    deos_state_t get_state() const;
    void set_state(deos_state_t new_state);
    
    NetworkMonitor network_monitor;
    LedController led_controller;
    SafetyManager safety_manager;

    void broadcast_lighting_status();

private:
    MainApp() = default;
    ~MainApp() = default;

    atomic_t current_state = ATOMIC_INIT(DEOS_STATE_INIT);

    // Non-copyable
    MainApp(const MainApp&) = delete;
    MainApp& operator=(const MainApp&) = delete;
};
