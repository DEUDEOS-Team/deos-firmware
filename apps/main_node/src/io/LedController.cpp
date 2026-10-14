#include "LedController.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(main_node_app);

#define LED_GREEN_NODE DT_ALIAS(led0) // Right signal
#define LED_BLUE_NODE  DT_ALIAS(led1) // Left signal
#define LED_RED_NODE   DT_ALIAS(led2) // Brake light

LedController::LedController() : 
    led_green(GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios)),
    led_blue(GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios)),
    led_red(GPIO_DT_SPEC_GET(LED_RED_NODE, gpios)) 
{}

int LedController::init() {
    if (!gpio_is_ready_dt(&led_green)) {
        LOG_ERR("Green LED device is not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&led_blue)) {
        LOG_ERR("Blue LED device is not ready");
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&led_red)) {
        LOG_ERR("Red LED device is not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);

    return 0;
}

void LedController::update(deos_state_t current_state, uint32_t ms_counter) {
    last_system_state = current_state;
    bool g = false; // Right signal (Green)
    bool b = false; // Left signal (Blue)
    bool r = false; // Brake light (Red)

    switch (current_state) {
    case DEOS_STATE_INIT:
        // Chaser effect during boot
        g = (ms_counter % 600 < 200);
        b = (ms_counter % 600 >= 200 && ms_counter % 600 < 400);
        r = (ms_counter % 600 >= 400);
        break;

    case DEOS_STATE_STANDBY:
        // Parked / Hazard lights: Both signals blink slowly (dörtlüler yanıyor), Brake OFF
        g = b = (ms_counter % 1000 < 500);
        r = false;
        break;

    case DEOS_STATE_READY:
    case DEOS_STATE_ACTIVE:
        // Normal operating mode: follow vehicle command requests
        if (req_hazard) {
            g = b = (ms_counter % 1000 < 500);
        } else {
            g = req_turn_right ? (ms_counter % 1000 < 500) : false;
            b = req_turn_left ? (ms_counter % 1000 < 500) : false;
        }
        r = req_brake;
        break;

    case DEOS_STATE_SAFE:
        // E-STOP pressed: Hard Brake ON, Hazards flashing rapidly.
        r = true; 
        g = b = (ms_counter % 250 < 125);
        break;

    case DEOS_STATE_FAULT:
        // Critical Error: Alternating left/right signals (police lights), fast blinking brake
        g = (ms_counter % 200 < 100);
        b = !g;
        r = (ms_counter % 200 < 100);
        break;

    default:
        g = b = r = false;
        break;
    }

    gpio_pin_set_dt(&led_green, g);
    gpio_pin_set_dt(&led_blue, b);
    gpio_pin_set_dt(&led_red, r);
}

int LedController::set_light(uint8_t light_id, bool turn_on) {
    switch (light_id) {
    case DEOS_LIGHT_HEADLIGHT:
        req_headlight = turn_on;
        LOG_INF("Headlight command: %s", turn_on ? "ON" : "OFF");
        break;
    case DEOS_LIGHT_BRAKE:
        req_brake = turn_on;
        LOG_INF("Brake light command: %s", turn_on ? "ON" : "OFF");
        break;
    case DEOS_LIGHT_TURN_LEFT:
        req_turn_left = turn_on;
        LOG_INF("Left turn signal command: %s", turn_on ? "ON" : "OFF");
        break;
    case DEOS_LIGHT_TURN_RIGHT:
        req_turn_right = turn_on;
        LOG_INF("Right turn signal command: %s", turn_on ? "ON" : "OFF");
        break;
    case DEOS_LIGHT_ALL:
        req_headlight = turn_on;
        req_brake = turn_on;
        req_turn_left = turn_on;
        req_turn_right = turn_on;
        LOG_INF("All lights command: %s", turn_on ? "ON" : "OFF");
        break;
    default:
        LOG_WRN("Unknown Light ID: 0x%02X", light_id);
        return -EINVAL;
    }
    return 0;
}

struct deos_lighting_status_payload LedController::get_status() const {
    struct deos_lighting_status_payload status = {0};

    // Headlight status
    status.headlight_state = req_headlight ? DEOS_LIGHT_STATE_ON : DEOS_LIGHT_STATE_OFF;

    // Brake light status (Active if safe/e-stop triggered or explicitly requested)
    if (last_system_state == DEOS_STATE_SAFE || req_brake) {
        status.brake_light_state = DEOS_LIGHT_STATE_ON;
    } else {
        status.brake_light_state = DEOS_LIGHT_STATE_OFF;
    }

    // Left turn signal status
    if (last_system_state == DEOS_STATE_STANDBY || last_system_state == DEOS_STATE_SAFE || req_hazard) {
        status.turn_left_state = DEOS_LIGHT_STATE_BLINKING;
    } else if (req_turn_left) {
        status.turn_left_state = DEOS_LIGHT_STATE_BLINKING;
    } else {
        status.turn_left_state = DEOS_LIGHT_STATE_OFF;
    }

    // Right turn signal status
    if (last_system_state == DEOS_STATE_STANDBY || last_system_state == DEOS_STATE_SAFE || req_hazard) {
        status.turn_right_state = DEOS_LIGHT_STATE_BLINKING;
    } else if (req_turn_right) {
        status.turn_right_state = DEOS_LIGHT_STATE_BLINKING;
    } else {
        status.turn_right_state = DEOS_LIGHT_STATE_OFF;
    }

    // Hazard status
    if (last_system_state == DEOS_STATE_STANDBY || last_system_state == DEOS_STATE_SAFE || req_hazard) {
        status.hazard_state = DEOS_LIGHT_STATE_BLINKING;
    } else {
        status.hazard_state = DEOS_LIGHT_STATE_OFF;
    }

    status.fault_flags = 0x00; // All ok
    return status;
}
