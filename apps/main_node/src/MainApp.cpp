#include "MainApp.hpp"
#include "transport/eth_textual.hpp"
#include "transport/eth_ros_bridge.hpp"
#include "transport/uart_lora.hpp"
#include "io/ImuSensor.hpp"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main_node_app, LOG_LEVEL_INF);

MainApp& MainApp::getInstance() {
    static MainApp instance;
    return instance;
}

static const char* state_to_str(deos_state_t s) {
    switch(s) {
        case DEOS_STATE_INIT: return "INIT (0x00)";
        case DEOS_STATE_STANDBY: return "STANDBY (0x01)";
        case DEOS_STATE_READY: return "READY (0x02)";
        case DEOS_STATE_ACTIVE: return "ACTIVE (0x03)";
        case DEOS_STATE_CALIBRATING: return "CALIBRATING (0x04)";
        case DEOS_STATE_SAFE: return "SAFE (0x05)";
        case DEOS_STATE_FAULT: return "FAULT (0x06)";
        default: return "UNKNOWN";
    }
}

extern "C" {
    int heartbeat_rx_callback(const deos_message_t *msg, void *user_data) {
        MainApp& app = MainApp::getInstance();
        
        deos_state_t reported_state = DEOS_STATE_UNKNOWN;
        if (msg->payload_len == sizeof(struct deos_heartbeat_payload)) {
            const struct deos_heartbeat_payload *pl = (const struct deos_heartbeat_payload *)msg->payload;
            reported_state = (deos_state_t)pl->current_state;
        }
        
        app.network_monitor.update_heartbeat(msg->source, reported_state, app.current_state);
        return 0;
    }

    int set_state_rx_callback(const deos_message_t *msg, void *user_data) {
        MainApp& app = MainApp::getInstance();
        
        if (msg->payload_len == 1) {
            deos_state_t req_state = (deos_state_t)msg->payload[0];
            LOG_INF("Received SET_STATE command from Node 0x%02X: requesting state %s", msg->source, state_to_str(req_state));
            app.set_state(req_state);
            deos_send_response(msg, DEOS_RESULT_SUCCESS, NULL, 0);
        } else {
            LOG_WRN("Invalid SET_STATE payload length: %d", msg->payload_len);
            deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        }
        return 0;
    }

    int lighting_command_rx_callback(const deos_message_t *msg, void *user_data) {
        MainApp& app = MainApp::getInstance();

        if (msg->payload_len >= 1) {
            uint8_t light_id = msg->payload[0];
            bool turn_on = (msg->command == DEOS_CMD_LIGHTING_TURN_ON);

            int ret = app.led_controller.set_light(light_id, turn_on);
            if (ret == 0) {
                struct deos_lighting_status_payload status = app.led_controller.get_status();
                deos_send_response(msg, DEOS_RESULT_SUCCESS, &status, sizeof(status));
                app.broadcast_lighting_status();
            } else {
                deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
            }
        } else {
            LOG_WRN("Invalid LIGHTING command payload length: %d", msg->payload_len);
            deos_send_response(msg, DEOS_RESULT_INVALID_PARAMETER, NULL, 0);
        }
        return 0;
    }

    int lighting_get_status_rx_callback(const deos_message_t *msg, void *user_data) {
        MainApp& app = MainApp::getInstance();
        struct deos_lighting_status_payload status = app.led_controller.get_status();
        deos_send_response(msg, DEOS_RESULT_SUCCESS, &status, sizeof(status));
        return 0;
    }

    deos_transport_t c_routing_policy(deos_node_id_t destination) {
        if (destination == DEOS_NODE_GROUND_CONTROL) {
            return DEOS_TRANSPORT_UART_LORA;
        } else if (destination == DEOS_NODE_ROS_BRIDGE) {
            return DEOS_TRANSPORT_ETH_ROS_BRIDGE;
        } else if (destination == DEOS_NODE_DIAG_TOOL) {
            return DEOS_TRANSPORT_ETH_TEXTUAL;
        }
        /* UART/LoRa testleri esnasinda CAN FD'ye gonderip sistemi kitlememesi icin kapali */
        /*
        else if (destination >= DEOS_NODE_TRACTION && destination <= DEOS_NODE_BMS_GATEWAY) {
            return DEOS_TRANSPORT_CAN_FD;
        }
        */
        return DEOS_TRANSPORT_LOCAL;
    }
}

int MainApp::init() {
    int ret = led_controller.init();
    if (ret != 0) return ret;

    ret = safety_manager.init();
    if (ret != 0) return ret;

    struct deos_config config = {
        .node_id = DEOS_NODE_MAIN_STM32,
        .can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_can_primary)),
        .router_enabled = true,
        .hosted_nodes_enabled = true
    };

    if (!device_is_ready(config.can_dev)) {
        LOG_ERR("CAN device not ready");
        return -ENODEV;
    }

    ret = deos_init(&config);
    if (ret != 0) {
        LOG_ERR("DEOS init failed: %d", ret);
        return ret;
    }

    deos_router_set_lookup_fn(c_routing_policy);
    
    /* DEOS çekirdeğine Heartbeat paketlerini nereye düşüreceğini söylüyoruz */
    deos_register_handler(DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_HEARTBEAT, heartbeat_rx_callback, NULL);

    /* SET_STATE komutunu (Cmd: 0x02, Class: COMMAND) dinle */
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_SYSTEM, DEOS_CMD_SYSTEM_SET_STATE, set_state_rx_callback, NULL);

    /* LIGHTING (0x08) komut ve durum sorgularını dinle */
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_LIGHTING, DEOS_CMD_LIGHTING_TURN_ON, lighting_command_rx_callback, NULL);
    deos_register_handler(DEOS_CLASS_COMMAND, DEOS_SERVICE_LIGHTING, DEOS_CMD_LIGHTING_TURN_OFF, lighting_command_rx_callback, NULL);
    deos_register_handler(DEOS_CLASS_REQUEST, DEOS_SERVICE_LIGHTING, DEOS_CMD_LIGHTING_GET_STATUS, lighting_get_status_rx_callback, NULL);

    EthTextual::getInstance().init();
    EthRosBridge::getInstance().init();
    UartLora::getInstance().init();
    ImuSensor::getInstance().init();

    return 0;
}

void MainApp::set_state(deos_state_t new_state) {
    if (current_state == new_state) return;

    current_state = new_state;

    /* Durum değiştiğinde ağdaki tüm nodlara bildir (Broadcast) */
    deos_priority_t prio = (new_state == DEOS_STATE_SAFE || new_state == DEOS_STATE_FAULT) 
                            ? DEOS_PRIO_EMERGENCY : DEOS_PRIO_CONTROL;

    deos_send(DEOS_NODE_GLOBAL_BROADCAST, prio, DEOS_CLASS_COMMAND, DEOS_SERVICE_SYSTEM, 
              DEOS_CMD_SYSTEM_SET_STATE, &current_state, sizeof(current_state));

    /* Durum değiştiğinde aydınlatma durumunu da güncelle ve yayınla */
    led_controller.update(current_state, k_uptime_get_32());
    broadcast_lighting_status();

    LOG_INF("System State changed to: %s", state_to_str(current_state));
}

void MainApp::broadcast_lighting_status() {
    struct deos_lighting_status_payload status = led_controller.get_status();
    deos_send(DEOS_NODE_CAN_BROADCAST, DEOS_PRIO_STATUS, DEOS_CLASS_STATUS, DEOS_SERVICE_LIGHTING,
              DEOS_CMD_LIGHTING_STATUS, &status, sizeof(status));
}

void MainApp::run() {
    deos_start();

    LOG_INF("Main System Controller (Brain) loop started.");

    /* Kritik düğümlerin (BMS Main, BMS Aux, Traction, Brake, Steering) Heartbeat akışına abone ol */
    network_monitor.subscribe_heartbeats();

    /* Sistemi hazır (STANDBY) moduna geçir */
    set_state(DEOS_STATE_STANDBY);
    
    uint32_t ms_counter = 0;
    bool last_estop_state = safety_manager.is_estop_triggered();

    while (1) {
        bool estop_pressed = safety_manager.is_estop_triggered();

        /* --- DONANIMSAL KESME / GÜVENLİK KONTROLÜ (EDGE DETECTION) --- */
        if (estop_pressed && !last_estop_state) {
            LOG_ERR("!!! EMERGENCY STOP TRIGGERED !!!");
            set_state(DEOS_STATE_SAFE);
        } else if (!estop_pressed && last_estop_state) {
            LOG_INF("E-STOP Released.");
            set_state(DEOS_STATE_STANDBY);
        }
        
        last_estop_state = estop_pressed;

        /* --- NETWORK (NODE) WATCHDOG --- */
        network_monitor.check_watchdog(*this);

        /* --- STATE MACHINE (DURUM MAKİNESİ) Görselleri --- */
        led_controller.update(current_state, ms_counter);

        ms_counter += 10;
        k_sleep(K_MSEC(10));
    }
}
