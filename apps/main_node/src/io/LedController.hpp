#pragma once
#include <deos/deos.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

class LedController {
public:
    LedController();
    int init();
    void update(deos_state_t current_state, uint32_t ms_counter);

    int set_light(uint8_t light_id, bool turn_on);
    struct deos_lighting_status_payload get_status() const;

private:
    struct gpio_dt_spec led_green; // led0 (Right Signal)
    struct gpio_dt_spec led_blue;  // led1 (Left Signal)
    struct gpio_dt_spec led_red;   // led2 (Brake Light)

    deos_state_t last_system_state = DEOS_STATE_INIT;
    mutable struct k_mutex state_lock;
    bool req_headlight = false;
    bool req_brake = false;
    bool req_turn_left = false;
    bool req_turn_right = false;
    bool req_hazard = false;
};
