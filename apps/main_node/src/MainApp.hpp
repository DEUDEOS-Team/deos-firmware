#pragma once

#include <deos/deos.h>
#include "io/LedController.hpp"
#include "io/SafetyManager.hpp"
#include "network/NetworkMonitor.hpp"

/* Aliases for ROS 2 Bridge */
#define DEOS_NODE_ROS_BRIDGE          DEOS_NODE_MICRO_ROS
#define DEOS_TRANSPORT_ETH_ROS_BRIDGE DEOS_TRANSPORT_ETH_UROS

class MainApp {
public:
    static MainApp& getInstance();
    
    int init();
    void run();

    deos_state_t current_state = DEOS_STATE_INIT;
    void set_state(deos_state_t new_state);
    
    NetworkMonitor network_monitor;

private:
    MainApp() = default;
    ~MainApp() = default;

    // Non-copyable
    MainApp(const MainApp&) = delete;
    MainApp& operator=(const MainApp&) = delete;



    LedController led_controller;
    SafetyManager safety_manager;
};
