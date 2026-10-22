/*
 * DEOS Firmware - Traction ECU Application
 * 
 * Target Node: DEOS_NODE_TRACTION (0x10)
 * Primary Service: DEOS_SERVICE_TRACTION (0x01)
 * Hardware Platform: STM32H5 (NUCLEO-H563ZI)
 *
 * Implements full CAN-FD bidirectional test capabilities with the Main Router (0x03)
 * compliant with DOC-ICD-NODE-TRAC-008 and DOC-ICD-TRP-CANFD-005.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <deos/deos.h>
#include <deos/deos_can.h>
#include <deos/deos_fault.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(traction_app, LOG_LEVEL_INF);

/* Onboard User LEDs (NUCLEO-H563ZI) */
static const struct gpio_dt_spec led_green  = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0}); // LD1 (Green)
static const struct gpio_dt_spec led_yellow = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios, {0}); // LD2 (Yellow)
static const struct gpio_dt_spec led_red    = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios, {0}); // LD3 (Red)

/* Internal Traction State */
static deos_state_t g_current_state = DEOS_STATE_STANDBY;
static uint16_t     g_target_speed = 0;       /* 0.01 m/s */
static int16_t      g_target_accel = 0;       /* 0.01 m/s^2 */
static uint8_t      g_current_gear = DEOS_GEAR_NEUTRAL;
static uint16_t     g_torque_limit = 10000;   /* 100.00% */
static uint16_t     g_simulated_speed = 0;    /* 0.01 m/s */
static uint16_t     g_motor_current = 0;      /* 0.1 A */
static int16_t      g_inverter_temp = 320;    /* 32.0 degC */
static int16_t      g_motor_temp = 350;       /* 35.0 degC */
static uint8_t      g_wheel_slip = 0;         /* 0: Normal */

static uint32_t     g_rx_count = 0;
static uint32_t     g_tx_count = 0;

static const char *state_to_str(deos_state_t s)
{
    switch (s) {
        case DEOS_STATE_INIT:        return "INIT (0x00)";
        case DEOS_STATE_STANDBY:     return "STANDBY (0x01)";
        case DEOS_STATE_READY:       return "READY (0x02)";
        case DEOS_STATE_ACTIVE:      return "ACTIVE (0x03)";
        case DEOS_STATE_CALIBRATING: return "CALIBRATING (0x04)";
        case DEOS_STATE_SAFE:        return "SAFE (0x05)";
        case DEOS_STATE_FAULT:       return "FAULT (0x06)";
        default:                     return "UNKNOWN";
    }
}

static const char *gear_to_str(uint8_t g)
{
    switch (g) {
        case DEOS_GEAR_NEUTRAL: return "NEUTRAL (0:N)";
        case DEOS_GEAR_DRIVE:   return "DRIVE (1:D)";
        case DEOS_GEAR_REVERSE: return "REVERSE (2:R)";
        case DEOS_GEAR_PARK:    return "PARK (3:P)";
        default:                return "INVALID";
    }
}

static int leds_init(void)
{
    if (led_green.port && gpio_is_ready_dt(&led_green)) {
        gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    }
    if (led_yellow.port && gpio_is_ready_dt(&led_yellow)) {
        gpio_pin_configure_dt(&led_yellow, GPIO_OUTPUT_INACTIVE);
    }
    if (led_red.port && gpio_is_ready_dt(&led_red)) {
        gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    }
    return 0;
}

/**
 * @brief Updates board LEDs matching main_node LedController state machine.
 */
static void leds_update(deos_state_t current_state, uint32_t ms_counter)
{
    bool g = false; // Green (LD1)
    bool y = false; // Yellow (LD2)
    bool r = false; // Red (LD3)

    switch (current_state) {
    case DEOS_STATE_INIT:
        // Chaser effect during boot
        g = (ms_counter % 600 < 200);
        y = (ms_counter % 600 >= 200 && ms_counter % 600 < 400);
        r = (ms_counter % 600 >= 400);
        break;

    case DEOS_STATE_STANDBY:
        // Parked / Standby: Both signals blink slowly (dörtlüler gibi 1 Hz), Brake OFF
        g = y = (ms_counter % 1000 < 500);
        r = false;
        break;

    case DEOS_STATE_READY:
    case DEOS_STATE_ACTIVE:
        // Normal operating mode: Solid green when active, Yellow if target speed set, Red if stopped
        g = true;
        y = (g_target_speed > 0) ? (ms_counter % 1000 < 500) : false;
        r = (g_current_gear == DEOS_GEAR_PARK || g_target_speed == 0);
        break;

    case DEOS_STATE_SAFE:
        // E-STOP pressed: Hard Brake ON (Red ON), Hazards flashing rapidly (4 Hz)
        r = true;
        g = y = (ms_counter % 250 < 125);
        break;

    case DEOS_STATE_FAULT:
        // Critical Error: Alternating left/right signals (police lights 5 Hz), fast blinking brake
        g = (ms_counter % 200 < 100);
        y = !g;
        r = (ms_counter % 200 < 100);
        break;

    default:
        g = y = r = false;
        break;
    }

    if (led_green.port && gpio_is_ready_dt(&led_green)) {
        gpio_pin_set_dt(&led_green, g);
    }
    if (led_yellow.port && gpio_is_ready_dt(&led_yellow)) {
        gpio_pin_set_dt(&led_yellow, y);
    }
    if (led_red.port && gpio_is_ready_dt(&led_red)) {
        gpio_pin_set_dt(&led_red, r);
    }
}

/* ========================================================================= */
/*                          DEOS MESSAGE HANDLERS                            */
/* ========================================================================= */

/**
 * @brief Handles PONG responses received from other nodes (e.g. Main Router).
 */
static int handle_pong(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    uint32_t ping_id = 0;
    if (msg->payload_len >= sizeof(uint32_t)) {
        ping_id = deos_get_u32_le(msg->payload);
    }

    LOG_INF("================================================================");
    LOG_INF(">>> [RX PONG] Verified CAN-FD round-trip with Node 0x%02X! <<<", msg->source);
    LOG_INF("    Ping ID: %u | Wire Seq: %u", ping_id, msg->sequence);
    LOG_INF("================================================================");
    return 0;
}

/**
 * @brief Handles Heartbeat messages received from the network.
 */
static int handle_heartbeat(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    deos_state_t remote_state = DEOS_STATE_UNKNOWN;
    if (msg->payload_len == sizeof(struct deos_heartbeat_payload)) {
        const struct deos_heartbeat_payload *hb = (const struct deos_heartbeat_payload *)msg->payload;
        remote_state = (deos_state_t)hb->current_state;
    }

    LOG_INF(">>> [RX HEARTBEAT] from Node 0x%02X (%s) | Remote State: %s",
            msg->source, deos_node_name(msg->source), state_to_str(remote_state));
    return 0;
}

/**
 * @brief Handles SET_STATE command from Main Controller / Router.
 */
static int handle_set_state(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    if (msg->payload_len < 1) {
        LOG_WRN("SET_STATE rejected: empty payload from 0x%02X", msg->source);
        deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        return DEOS_RESULT_INVALID_PARAMETER;
    }

    deos_state_t new_state = (deos_state_t)msg->payload[0];
    LOG_INF("================================================================");
    LOG_INF(">>> [RX SET_STATE] Command from Node 0x%02X: %s -> %s",
            msg->source, state_to_str(g_current_state), state_to_str(new_state));
    LOG_INF("================================================================");

    g_current_state = new_state;

    if (g_current_state == DEOS_STATE_SAFE || g_current_state == DEOS_STATE_FAULT) {
        g_target_speed = 0;
        g_simulated_speed = 0;
        g_motor_current = 0;
        LOG_WRN("Traction in SAFE/FAULT mode: Output power cut off!");
    }

    /* Acknowledge state change to sender */
    deos_send_response(msg, DEOS_RESULT_SUCCESS, &g_current_state, sizeof(g_current_state));
    g_tx_count++;
    return 0;
}

/**
 * @brief Handles GET_STATE request.
 */
static int handle_get_state(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    LOG_INF(">>> [RX GET_STATE] from Node 0x%02X -> Responding: %s",
            msg->source, state_to_str(g_current_state));

    deos_send_response(msg, DEOS_RESULT_SUCCESS, &g_current_state, sizeof(g_current_state));
    g_tx_count++;
    return 0;
}

/**
 * @brief Handles SET_TARGET_SPEED command (DOC-ICD-NODE-TRAC-008 Section 1.1).
 */
static int handle_set_target_speed(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    if (msg->payload_len < sizeof(struct deos_traction_target_payload)) {
        LOG_WRN("Invalid SET_TARGET_SPEED payload length: %d", msg->payload_len);
        deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        return DEOS_RESULT_INVALID_PARAMETER;
    }

    const struct deos_traction_target_payload *cmd =
        (const struct deos_traction_target_payload *)msg->payload;

    g_target_speed = cmd->target_speed;
    g_target_accel = cmd->target_accel;

    LOG_INF(">>> [RX SET_TARGET_SPEED] from Node 0x%02X: Target Spd = %u.%02u m/s, Accel = %d.%02u m/s^2",
            msg->source,
            g_target_speed / 100, g_target_speed % 100,
            g_target_accel / 100, abs(g_target_accel) % 100);

    /* Commands don't strictly require a response unless requested, but we ack for test verification */
    deos_send_response(msg, DEOS_RESULT_SUCCESS, NULL, 0);
    g_tx_count++;
    return 0;
}

/**
 * @brief Handles SET_GEAR command (DOC-ICD-NODE-TRAC-008 Section 1.1).
 */
static int handle_set_gear(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    if (msg->payload_len < sizeof(struct deos_traction_gear_payload)) {
        LOG_WRN("Invalid SET_GEAR payload length: %d", msg->payload_len);
        deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        return DEOS_RESULT_INVALID_PARAMETER;
    }

    const struct deos_traction_gear_payload *cmd =
        (const struct deos_traction_gear_payload *)msg->payload;

    if (cmd->gear > DEOS_GEAR_PARK) {
        LOG_WRN("Invalid gear value: %u", cmd->gear);
        deos_send_response(msg, DEOS_RESULT_OUT_OF_RANGE, NULL, 0);
        return DEOS_RESULT_OUT_OF_RANGE;
    }

    LOG_INF(">>> [RX SET_GEAR] from Node 0x%02X: %s -> %s",
            msg->source, gear_to_str(g_current_gear), gear_to_str(cmd->gear));

    g_current_gear = cmd->gear;
    deos_send_response(msg, DEOS_RESULT_SUCCESS, NULL, 0);
    g_tx_count++;
    return 0;
}

/**
 * @brief Handles SET_TORQUE_LIMIT command (DOC-ICD-NODE-TRAC-008 Section 1.1).
 */
static int handle_set_torque_limit(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    if (msg->payload_len < sizeof(struct deos_traction_torque_limit_payload)) {
        LOG_WRN("Invalid SET_TORQUE_LIMIT payload length: %d", msg->payload_len);
        deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        return DEOS_RESULT_INVALID_PARAMETER;
    }

    const struct deos_traction_torque_limit_payload *cmd =
        (const struct deos_traction_torque_limit_payload *)msg->payload;

    g_torque_limit = cmd->torque_limit;
    LOG_INF(">>> [RX SET_TORQUE_LIMIT] from Node 0x%02X: Limit = %u.%02u %%",
            msg->source, g_torque_limit / 100, g_torque_limit % 100);

    deos_send_response(msg, DEOS_RESULT_SUCCESS, NULL, 0);
    g_tx_count++;
    return 0;
}

/**
 * @brief Handles GET_STATUS telemetry request (DOC-ICD-NODE-TRAC-008 Section 1.2).
 */
static int handle_get_status(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    struct deos_traction_status_payload status;
    status.current_speed = g_simulated_speed;
    status.motor_current = g_motor_current;
    status.inverter_temp = g_inverter_temp;
    status.motor_temp = g_motor_temp;
    status.current_gear = g_current_gear;
    status.wheel_slip_state = g_wheel_slip;

    LOG_INF(">>> [RX GET_STATUS] from Node 0x%02X -> Responding with telemetry (Speed: %u.%02u m/s, Curr: %u.%01u A)",
            msg->source, status.current_speed / 100, status.current_speed % 100,
            status.motor_current / 10, status.motor_current % 10);

    deos_send_response(msg, DEOS_RESULT_SUCCESS, &status, sizeof(status));
    g_tx_count++;
    return 0;
}

/**
 * @brief Handles broadcast Lighting Status from Main Controller.
 */
static int handle_lighting_status(const deos_message_t *msg, void *user_data)
{
    ARG_UNUSED(user_data);
    g_rx_count++;

    LOG_INF(">>> [RX LIGHTING BROADCAST] from Node 0x%02X (Len: %d) - Verified Broadcast CAN-FD reception!",
            msg->source, msg->payload_len);
    return 0;
}

/* ========================================================================= */
/*                               MAIN ENTRY POINT                            */
/* ========================================================================= */

int main(void)
{
    LOG_INF("=================================================================");
    LOG_INF("   DEOS Traction Node (STM32H5 / NUCLEO-H563ZI) Starting...     ");
    LOG_INF("   Node ID: 0x%02X (TRACTION) | Proto Version: 0x%02X           ",
            DEOS_NODE_TRACTION, DEOS_PROTOCOL_VERSION);
    LOG_INF("=================================================================");

    /* 1. Setup User Indicator LEDs */
    leds_init();
    LOG_INF("Indicator LEDs configured (Green: LD1, Yellow: LD2, Red: LD3).");

    /* 2. Initialize DEOS Core */
    struct deos_config d_cfg = {
        .node_id = DEOS_NODE_TRACTION,
        .router_enabled = false,
        .hosted_nodes_enabled = false
    };

    int ret = deos_init(&d_cfg);
    if (ret != 0) {
        LOG_ERR("DEOS Core initialization failed: %d", ret);
        return ret;
    }

    /* Register Node Metadata & Capabilities */
    uint32_t supported_services = (1U << DEOS_SERVICE_SYSTEM) |
                                  (1U << DEOS_SERVICE_TRACTION) |
                                  (1U << DEOS_SERVICE_DIAGNOSTIC);
    deos_set_node_info(DEOS_HW_TYPE_STM32H5, 1, 1, 0, 0x54524143 /* 'TRAC' */, supported_services);

    /* 3. Initialize CAN-FD Transport */
    const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_can_primary));
    if (!device_is_ready(can_dev)) {
        LOG_ERR("CAN-FD device (%s) is not ready!", can_dev ? can_dev->name : "NULL");
        return -ENODEV;
    }

    ret = deos_can_transport_init(can_dev);
    if (ret != 0) {
        LOG_ERR("DEOS CAN-FD transport init failed: %d", ret);
        return ret;
    }
    LOG_INF("CAN-FD Transport initialized on %s (500k nominal, 2M data).", can_dev->name);

    /* 4. Register Message Handlers */
    /* System Handlers */
    deos_register_handler(DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM,
                          DEOS_CMD_SYSTEM_PONG, handle_pong, NULL);
    deos_register_handler(DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM,
                          DEOS_CMD_SYSTEM_HEARTBEAT, handle_heartbeat, NULL);
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_SYSTEM,
                          DEOS_CMD_SYSTEM_SET_STATE, handle_set_state, NULL);
    deos_register_handler(DEOS_CLASS_REQUEST, DEOS_SERVICE_SYSTEM,
                          DEOS_CMD_SYSTEM_GET_STATE, handle_get_state, NULL);

    /* Traction Functional Handlers */
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_TRACTION,
                          DEOS_CMD_TRACTION_SET_TARGET_SPEED, handle_set_target_speed, NULL);
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_TRACTION,
                          DEOS_CMD_TRACTION_SET_GEAR, handle_set_gear, NULL);
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_TRACTION,
                          DEOS_CMD_TRACTION_SET_TORQUE_LIMIT, handle_set_torque_limit, NULL);
    deos_register_handler(DEOS_CLASS_REQUEST, DEOS_SERVICE_TRACTION,
                          DEOS_CMD_TRACTION_GET_STATUS, handle_get_status, NULL);

    /* Cross-Node Broadcast Listeners (e.g. from Main Router) */
    deos_register_handler(DEOS_CLASS_STATUS, DEOS_SERVICE_LIGHTING,
                          DEOS_CMD_LIGHTING_STATUS, handle_lighting_status, NULL);

    LOG_INF("All DEOS protocol handlers registered successfully.");

    /* 5. Start DEOS Stack & CAN Threads */
    ret = deos_start();
    if (ret != 0) {
        LOG_ERR("Failed to start DEOS core: %d", ret);
        return ret;
    }

    ret = deos_can_transport_start();
    if (ret != 0) {
        LOG_ERR("Failed to start CAN transport: %d", ret);
        return ret;
    }

    LOG_INF("DEOS Traction node is fully RUNNING on CAN-FD bus.");

    /* 6. Main Test Loop (Runs at 20 ms for smooth LED state transitions) */
    uint32_t ms_counter = 0;
    uint32_t ping_counter = 0;

    while (1) {
        k_sleep(K_MSEC(20));
        ms_counter += 20;

        /* Update LEDs based on current system state (identical to main_node LedController) */
        leds_update(g_current_state, ms_counter);

        /* 100 ms tasks: simulation dynamics */
        if (ms_counter % 100 == 0) {
            if ((g_current_state == DEOS_STATE_ACTIVE || g_current_state == DEOS_STATE_READY) &&
                (g_current_gear == DEOS_GEAR_DRIVE || g_current_gear == DEOS_GEAR_REVERSE)) {
                if (g_simulated_speed < g_target_speed) {
                    g_simulated_speed += 20; /* +0.2 m/s ramp */
                    if (g_simulated_speed > g_target_speed) g_simulated_speed = g_target_speed;
                } else if (g_simulated_speed > g_target_speed) {
                    if (g_simulated_speed >= 20) g_simulated_speed -= 20;
                    else g_simulated_speed = 0;
                }
                g_motor_current = (g_simulated_speed > 0) ? 150 : 10; /* 15.0 A or 1.0 A idle */
            } else {
                g_simulated_speed = 0;
                g_motor_current = 0;
            }
        }

        /* 5000 ms tasks: Send PING to Main Router */
        if (ms_counter % 5000 == 0) {
            ping_counter++;
            int send_ret = deos_send_ping(DEOS_NODE_MAIN_STM32, ping_counter);
            if (send_ret == 0) {
                g_tx_count++;
                LOG_INF("TX [PING #%u] -> Main Router (0x%02X)... Waiting for PONG reply",
                        ping_counter, DEOS_NODE_MAIN_STM32);
            } else {
                LOG_WRN("Failed to send PING to Main Router: %d", send_ret);
            }
        }

        /* 10000 ms tasks: Traffic Statistics */
        if (ms_counter % 10000 == 0) {
            LOG_INF("--- CAN-FD Traffic Stats: TX Total = %u msgs | RX Total = %u msgs | State: %s ---",
                    g_tx_count, g_rx_count, state_to_str(g_current_state));
        }
    }

    return 0;
}

