#include <deos/deos.h>
#include <deos/deos_can.h>
#include <deos/deos_fault.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(dummy_can, LOG_LEVEL_INF);

static int handle_pong(const deos_message_t *msg, void *user_data) {
  LOG_INF("Received PONG from Node 0x%02X", msg->source);
  return 0;
}

static int handle_heartbeat(const deos_message_t *msg, void *user_data) {
  if (msg->payload_len == sizeof(struct deos_heartbeat_payload)) {
    const struct deos_heartbeat_payload *pl =
        (const struct deos_heartbeat_payload *)msg->payload;
    LOG_INF("Received HEARTBEAT from Node 0x%02X (State: 0x%02X)", msg->source,
            pl->current_state);
  } else {
    LOG_INF("Received HEARTBEAT from Node 0x%02X (Legacy/Empty Payload)",
            msg->source);
  }
  return 0;
}

static int handle_bms_status(const deos_message_t *msg, void *user_data) {
  if (msg->payload_len == sizeof(struct deos_bms_status_payload)) {
    const struct deos_bms_status_payload *status =
        (const struct deos_bms_status_payload *)msg->payload;
    LOG_INF("BMS STATUS (Node 0x%02X): Pack V: %d mV, I: %d cA, SOC: %d %%, "
            "Cells: %d",
            msg->source, status->pack_voltage, status->pack_current,
            status->state_of_charge / 10, status->cell_count);
  } else {
    LOG_WRN("Invalid BMS STATUS payload length: %d", msg->payload_len);
  }
  return 0;
}

static int handle_bms_cell_voltage(const deos_message_t *msg, void *user_data) {
  if (msg->payload_len == sizeof(struct deos_bms_cell_voltage_payload)) {
    const struct deos_bms_cell_voltage_payload *pl =
        (const struct deos_bms_cell_voltage_payload *)msg->payload;
    if (pl->cell_index == 0) {
      LOG_INF("BMS CELL VOLTAGE (Node 0x%02X): End of list.", msg->source);
    } else {
      LOG_INF("BMS CELL VOLTAGE (Node 0x%02X): Cell %d = %d mV", msg->source,
              pl->cell_index, pl->cell_voltage);
    }
  }
  return 0;
}

static int handle_bms_balancing(const deos_message_t *msg, void *user_data) {
  if (msg->payload_len == sizeof(struct deos_bms_balancing_payload)) {
    const struct deos_bms_balancing_payload *pl =
        (const struct deos_bms_balancing_payload *)msg->payload;
    if (pl->cell_index == 0) {
      LOG_INF("BMS BALANCING (Node 0x%02X): End of list. Overall state = %d",
              msg->source, pl->balancing_state);
    } else {
      LOG_INF("BMS BALANCING (Node 0x%02X): Cell %d State = %d", msg->source,
              pl->cell_index, pl->balancing_state);
    }
  } else {
    LOG_WRN("Invalid BMS BALANCING payload length: %d", msg->payload_len);
  }
  return 0;
}

int main(void) {
  LOG_INF("Dummy CAN Node (Diagnostic Tool) Starting...");

  struct deos_config config = {.node_id = DEOS_NODE_DIAG_TOOL,
                               .router_enabled = false,
                               .hosted_nodes_enabled = false};

  int ret = deos_init(&config);
  if (ret != 0) {
    LOG_ERR("DEOS init failed: %d", ret);
    return ret;
  }

  const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_can_primary));
  if (!device_is_ready(can_dev)) {
    LOG_ERR("CAN device not ready");
    return -1;
  }

  ret = deos_can_transport_init(can_dev);
  if (ret != 0) {
    LOG_ERR("CAN transport init failed: %d", ret);
    return ret;
  }

  /* Register handlers */
  deos_register_handler(DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM,
                        DEOS_CMD_SYSTEM_PONG, handle_pong, NULL);
  deos_register_handler(DEOS_CLASS_NETWORK, DEOS_SERVICE_SYSTEM,
                        DEOS_CMD_SYSTEM_HEARTBEAT, handle_heartbeat, NULL);
  deos_register_handler(DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                        DEOS_CMD_BMS_STATUS, handle_bms_status, NULL);
  deos_register_handler(DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                        DEOS_CMD_BMS_CELL_VOLTAGE, handle_bms_cell_voltage,
                        NULL);
  deos_register_handler(DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                        DEOS_CMD_BMS_BALANCING, handle_bms_balancing, NULL);

  deos_start();
  deos_can_transport_start();

  int loop_counter = 0;
  while (1) {
    k_sleep(K_SECONDS(2));

    // Target BMS Main Node
    deos_node_id_t target_bms = DEOS_NODE_BMS_MAIN;

    if (loop_counter % 5 == 0) {
      LOG_INF("Sending PING to BMS Main, Aux, and Gateway...");
      deos_send_ping(target_bms, loop_counter);
      deos_send_ping(DEOS_NODE_BMS_AUX, loop_counter + 1);
      deos_send_ping(DEOS_NODE_BMS_GATEWAY, loop_counter + 2);
    } else if (loop_counter % 5 == 1) {
      LOG_INF("Sending GET_STATUS to BMS...");
      deos_send(target_bms, DEOS_PRIO_STATUS, DEOS_CLASS_REQUEST,
                DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS, NULL, 0);
    } else if (loop_counter % 5 == 2) {
      LOG_INF("Sending GET_CELL_VOLTAGES to BMS...");
      deos_send(target_bms, DEOS_PRIO_STATUS, DEOS_CLASS_REQUEST,
                DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_CELL_VOLTAGES, NULL, 0);
    } else if (loop_counter % 5 == 3) {
      LOG_INF("Sending GET_BALANCING to BMS...");
      deos_send(target_bms, DEOS_PRIO_STATUS, DEOS_CLASS_REQUEST,
                DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_BALANCING, NULL, 0);
    } else {
      LOG_INF("Waiting...");
    }

    loop_counter++;
  }

  return 0;
}
