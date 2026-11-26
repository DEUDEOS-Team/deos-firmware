#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <stdint.h>
#include <string.h>

#include "bms_gateway.h"
#include "config.h"
#include "daly_parser.h"
#include <deos/deos.h>
#include <deos/deos_fault.h>

LOG_MODULE_REGISTER(bms_gateway, LOG_LEVEL_INF);

struct DeosBmsStatus {
  struct deos_bms_status_payload pack_info;
  uint16_t cell_voltages_mV[32]{0};
  uint8_t balancing_mode{0};
  uint32_t balancing_mask{0};
};

class BmsGateway;

class BmsDevice {
private:
  deos_node_id_t deos_node_id;
  uint8_t daly_board_number;
  bool is_online;
  int64_t last_rx_timestamp;
  bool comm_fault_raised{false};
  DeosBmsStatus status;

  deos_node_id_t pending_get_status_requester;
  deos_node_id_t pending_get_cells_requester;
  deos_node_id_t pending_get_balancing_requester;

public:
  BmsDevice(deos_node_id_t node_id, uint8_t board_id)
      : deos_node_id(node_id), daly_board_number(board_id), is_online(false),
        last_rx_timestamp(0), pending_get_status_requester(DEOS_NODE_INVALID),
        pending_get_cells_requester(DEOS_NODE_INVALID),
        pending_get_balancing_requester(DEOS_NODE_INVALID) {
    memset(&status.pack_info, 0, sizeof(status.pack_info));
    status.pack_info.cell_count = 16;
    status.pack_info.temperature_sensor_count = 1;
  }

  deos_node_id_t getNodeId() const { return deos_node_id; }
  uint8_t getBoardId() const { return daly_board_number; }

  bool isOnline() const {
    return is_online && (k_uptime_get() - last_rx_timestamp < BMS_TIMEOUT_MS);
  }

  int64_t getLastRxTimestamp() const { return last_rx_timestamp; }
  const DeosBmsStatus &getStatus() const { return status; }

  void setPendingStatusRequester(deos_node_id_t requester_id) {
    pending_get_status_requester = requester_id;
  }
  deos_node_id_t getPendingStatusRequester() const {
    return pending_get_status_requester;
  }
  void clearPendingStatusRequester() {
    pending_get_status_requester = DEOS_NODE_INVALID;
  }

  void setPendingCellsRequester(deos_node_id_t requester_id) {
    pending_get_cells_requester = requester_id;
  }
  deos_node_id_t getPendingCellsRequester() const {
    return pending_get_cells_requester;
  }
  void clearPendingCellsRequester() {
    pending_get_cells_requester = DEOS_NODE_INVALID;
  }

  void setPendingBalancingRequester(deos_node_id_t requester_id) {
    pending_get_balancing_requester = requester_id;
  }
  deos_node_id_t getPendingBalancingRequester() const {
    return pending_get_balancing_requester;
  }
  void clearPendingBalancingRequester() {
    pending_get_balancing_requester = DEOS_NODE_INVALID;
  }

  void checkCommStatus() {
    bool current_online = isOnline();
    if (!current_online && !comm_fault_raised) {
      uint16_t fault_id = (daly_board_number == DALY_BOARD_MAIN) ? DEOS_FAULT_GW_MAIN_COMM_LOST : DEOS_FAULT_GW_AUX_COMM_LOST;
      deos_fault_raise_for_node(DEOS_NODE_BMS_GATEWAY, fault_id, DEOS_FAULT_SEVERITY_ERROR);
      comm_fault_raised = true;
      LOG_ERR("BMS Board %d Comm Lost! Raising Fault 0x%04X", daly_board_number, fault_id);
    } else if (current_online && comm_fault_raised) {
      uint16_t fault_id = (daly_board_number == DALY_BOARD_MAIN) ? DEOS_FAULT_GW_MAIN_COMM_LOST : DEOS_FAULT_GW_AUX_COMM_LOST;
      deos_fault_set_inactive_for_node(DEOS_NODE_BMS_GATEWAY, fault_id);
      comm_fault_raised = false;
      LOG_INF("BMS Board %d Comm Restored. Clearing Fault 0x%04X", daly_board_number, fault_id);
    }
  }

  void updatePackStatus(const DalyPackInfo &info);
  void updateCellStatus(const DalyCellInfo &info);
  void updateBalancingState(const DalyBalancingState &info);
  void updateBalancingCells(const DalyBalancingCells &info);
  void updateAlarms(const DalyAlarmInfo &info);
};

class BmsGateway {
private:
  BmsDevice devices[2] = {BmsDevice(DEOS_NODE_BMS_MAIN, DALY_BOARD_MAIN),
                          BmsDevice(DEOS_NODE_BMS_AUX, DALY_BOARD_AUX)};

  const device *can_daly;

  static BmsGateway *instance;

  uint32_t build_daly_req_id(uint8_t data_id, uint8_t board_num) {
    return 0x18000000U | (static_cast<uint32_t>(data_id) << 16) |
           (static_cast<uint32_t>(board_num) << 8) | DALY_HOST_ADDRESS;
  }

  void handle_daly_response(const struct can_frame &frame) {
    if (frame.dlc != 8)
      return;

    uint8_t data_id = static_cast<uint8_t>((frame.id >> 16) & 0xFF);
    uint8_t target_host = static_cast<uint8_t>((frame.id >> 8) & 0xFF);
    uint8_t source_board = static_cast<uint8_t>(frame.id & 0xFF);

    if (target_host != DALY_HOST_ADDRESS)
      return;

    BmsDevice *device = findBmsByBoardId(source_board);
    if (!device)
      return;

    if (data_id == 0x90) {
      DalyPackInfo info;
      if (DalyParser::parsePackInfo(frame.data, info)) {
        device->updatePackStatus(info);
      }
    } else if (data_id == 0x95) {
      DalyCellInfo info;
      if (DalyParser::parseCellInfo(frame.data, info)) {
        device->updateCellStatus(info);
      }
    } else if (data_id == 0x4D) {
      DalyBalancingState info;
      LOG_INF("Raw 0x4D [Board %d]: %02X %02X %02X %02X %02X %02X %02X %02X",
              source_board, frame.data[0], frame.data[1], frame.data[2],
              frame.data[3], frame.data[4], frame.data[5], frame.data[6],
              frame.data[7]);
      if (DalyParser::parseBalancingState(frame.data, info)) {
        device->updateBalancingState(info);
      }
    } else if (data_id == 0x4F) {
      DalyBalancingCells info;
      LOG_INF("Raw 0x4F [Board %d]: %02X %02X %02X %02X %02X %02X %02X %02X",
              source_board, frame.data[0], frame.data[1], frame.data[2],
              frame.data[3], frame.data[4], frame.data[5], frame.data[6],
              frame.data[7]);
      if (DalyParser::parseBalancingCells(frame.data, info)) {
        device->updateBalancingCells(info);
      }
    } else if (data_id == 0x98) {
      DalyAlarmInfo info;
      if (DalyParser::parseAlarms(frame.data, info)) {
        device->updateAlarms(info);
      }
    }
  }

public:
  BmsGateway() : can_daly(DEVICE_DT_GET(DALY_CAN_NODE)) {}

  void sendDalyRequest(uint8_t data_id, uint8_t board_num) {
    struct can_frame frame = {0};
    frame.flags = CAN_FRAME_IDE;
    frame.id = build_daly_req_id(data_id, board_num);
    frame.dlc = 8;
    can_send(can_daly, &frame, K_MSEC(100), nullptr, nullptr);
  }

  void sendBmsStatus(BmsDevice *device, deos_node_id_t target_node) {
    const auto &status = device->getStatus();

    deos_send_from_node(device->getNodeId(), target_node, DEOS_PRIO_STATUS,
                        DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                        DEOS_CMD_BMS_STATUS, &status.pack_info,
                        sizeof(status.pack_info));
  }

  void sendCellVoltages(BmsDevice *device, deos_node_id_t target_node) {
    const auto &status = device->getStatus();

    for (uint8_t i = 0; i < status.pack_info.cell_count; i++) {
      struct deos_bms_cell_voltage_payload cell_pl = {0};
      cell_pl.cell_index = i + 1; // 1-based index
      cell_pl.cell_voltage = status.cell_voltages_mV[i];

      deos_send_from_node(device->getNodeId(), target_node, DEOS_PRIO_STATUS,
                          DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                          DEOS_CMD_BMS_CELL_VOLTAGE, &cell_pl, sizeof(cell_pl));
      k_msleep(10);
    }

    /* End of list */
    struct deos_bms_cell_voltage_payload end_pl = {0};
    end_pl.cell_index = 0x00;
    end_pl.cell_voltage = 0x0000;

    deos_send_from_node(device->getNodeId(), target_node, DEOS_PRIO_STATUS,
                        DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                        DEOS_CMD_BMS_CELL_VOLTAGE, &end_pl, sizeof(end_pl));
  }

  void sendBalancing(BmsDevice *device, deos_node_id_t target_node) {
    const auto &status = device->getStatus();

    for (uint8_t i = 0; i < status.pack_info.cell_count; i++) {
      struct deos_bms_balancing_payload bal_pl = {0};
      bal_pl.cell_index = i + 1; // 1-based index
      bal_pl.balancing_state = (status.balancing_mask & (1UL << i)) ? 1 : 0;

      deos_send_from_node(device->getNodeId(), target_node, DEOS_PRIO_STATUS,
                          DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                          DEOS_CMD_BMS_BALANCING, &bal_pl, sizeof(bal_pl));
      k_msleep(5);
    }

    /* End of list */
    struct deos_bms_balancing_payload end_pl = {0};
    end_pl.cell_index = 0x00;
    end_pl.balancing_state = status.balancing_mode;

    deos_send_from_node(device->getNodeId(), target_node, DEOS_PRIO_STATUS,
                        DEOS_CLASS_RESPONSE, DEOS_SERVICE_BMS,
                        DEOS_CMD_BMS_BALANCING, &end_pl, sizeof(end_pl));
  }

  BmsDevice *findBmsByNodeId(deos_node_id_t node_id) {
    for (auto &dev : devices) {
      if (dev.getNodeId() == node_id)
        return &dev;
    }
    return nullptr;
  }

  BmsDevice *findBmsByBoardId(uint8_t board_id) {
    for (auto &dev : devices) {
      if (dev.getBoardId() == board_id)
        return &dev;
    }
    return nullptr;
  }

  static int deos_msg_handler(const deos_message_t *msg, void *user_data) {
    BmsGateway *gw = static_cast<BmsGateway *>(user_data);
    BmsDevice *device = gw->findBmsByNodeId(msg->destination);

    if (!device)
      return 0;

    if (msg->message_class == DEOS_CLASS_REQUEST &&
        msg->service == DEOS_SERVICE_BMS) {
      if (msg->command == DEOS_CMD_BMS_GET_STATUS) {
        if (k_uptime_get() - device->getLastRxTimestamp() <
            BMS_CACHE_FRESHNESS_MS) {
          gw->sendBmsStatus(device, msg->source);
        } else {
          device->setPendingStatusRequester(msg->source);
          gw->sendDalyRequest(0x90, device->getBoardId());
        }
      } else if (msg->command == DEOS_CMD_BMS_GET_CELL_VOLTAGES) {
        if (k_uptime_get() - device->getLastRxTimestamp() <
            BMS_CACHE_FRESHNESS_MS) {
          gw->sendCellVoltages(device, msg->source);
        } else {
          device->setPendingCellsRequester(msg->source);
          gw->sendDalyRequest(0x95, device->getBoardId());
        }
      } else if (msg->command == DEOS_CMD_BMS_GET_BALANCING) {
        if (k_uptime_get() - device->getLastRxTimestamp() <
            BMS_CACHE_FRESHNESS_MS) {
          gw->sendBalancing(device, msg->source);
        } else {
          device->setPendingBalancingRequester(msg->source);
          gw->sendDalyRequest(0x4D, device->getBoardId());
          gw->sendDalyRequest(0x4F, device->getBoardId());
        }
      }
    }
    return 0;
  }

  int init() {
    instance = this;
    if (!device_is_ready(can_daly))
      return -1;
    can_start(can_daly);

    deos_register_handler_for_node(DEOS_NODE_BMS_MAIN, DEOS_CLASS_REQUEST,
                                   DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS,
                                   deos_msg_handler, this);
    deos_register_handler_for_node(
        DEOS_NODE_BMS_MAIN, DEOS_CLASS_REQUEST, DEOS_SERVICE_BMS,
        DEOS_CMD_BMS_GET_CELL_VOLTAGES, deos_msg_handler, this);
    deos_register_handler_for_node(DEOS_NODE_BMS_MAIN, DEOS_CLASS_REQUEST,
                                   DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_BALANCING,
                                   deos_msg_handler, this);

    deos_register_handler_for_node(DEOS_NODE_BMS_AUX, DEOS_CLASS_REQUEST,
                                   DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_STATUS,
                                   deos_msg_handler, this);
    deos_register_handler_for_node(
        DEOS_NODE_BMS_AUX, DEOS_CLASS_REQUEST, DEOS_SERVICE_BMS,
        DEOS_CMD_BMS_GET_CELL_VOLTAGES, deos_msg_handler, this);
    deos_register_handler_for_node(DEOS_NODE_BMS_AUX, DEOS_CLASS_REQUEST,
                                   DEOS_SERVICE_BMS, DEOS_CMD_BMS_GET_BALANCING,
                                   deos_msg_handler, this);

    return 0;
  }

  static BmsGateway *getInstance() { return instance; }

  void processDalyFrame(const struct can_frame &frame) {
    if (!(frame.flags & CAN_FRAME_IDE))
      return;

    uint8_t data_id = static_cast<uint8_t>((frame.id >> 16) & 0xFF);
    if (data_id == 0x90 || data_id == 0x95 || data_id == 0x4D ||
        data_id == 0x4F || data_id == 0x98) {
      handle_daly_response(frame);
    }
  }
};

BmsGateway *BmsGateway::instance = nullptr;

void BmsDevice::updatePackStatus(const DalyPackInfo &info) {
  status.pack_info.pack_voltage = info.voltage_dV * 10;
  int32_t cur = info.current_dA;
  if (cur < -32768)
    cur = -32768;
  else if (cur > 32767)
    cur = 32767;
  status.pack_info.pack_current = static_cast<int16_t>(cur);
  status.pack_info.state_of_charge = info.soc_dPct * 10;

  is_online = true;
  last_rx_timestamp = k_uptime_get();

  if (pending_get_status_requester != DEOS_NODE_INVALID &&
      BmsGateway::getInstance()) {
    BmsGateway::getInstance()->sendBmsStatus(this,
                                             pending_get_status_requester);
    clearPendingStatusRequester();
  }
}

void BmsDevice::updateCellStatus(const DalyCellInfo &info) {
  uint8_t base_idx = (info.frame_index - 1) * 3;
  uint16_t min_v = 0xFFFF;
  uint16_t max_v = 0;

  for (int i = 0; i < 3; i++) {
    if (base_idx + i < 32) {
      status.cell_voltages_mV[base_idx + i] = info.cell_v_mV[i];

      if (info.cell_v_mV[i] > 0 && info.cell_v_mV[i] < min_v)
        min_v = info.cell_v_mV[i];
      if (info.cell_v_mV[i] > max_v)
        max_v = info.cell_v_mV[i];
    }
  }

  if (min_v < status.pack_info.minimum_cell_voltage ||
      status.pack_info.minimum_cell_voltage == 0) {
    status.pack_info.minimum_cell_voltage = min_v;
  }
  if (max_v > status.pack_info.maximum_cell_voltage) {
    status.pack_info.maximum_cell_voltage = max_v;
  }

  is_online = true;
  last_rx_timestamp = k_uptime_get();

  if (info.frame_index == (status.pack_info.cell_count + 2) / 3 &&
      pending_get_cells_requester != DEOS_NODE_INVALID &&
      BmsGateway::getInstance()) {
    BmsGateway::getInstance()->sendCellVoltages(this,
                                                pending_get_cells_requester);
    clearPendingCellsRequester();
  }
}

void BmsDevice::updateBalancingState(const DalyBalancingState &info) {
  status.balancing_mode = info.state;
  is_online = true;
  last_rx_timestamp = k_uptime_get();

  // Balancing cells mask usually comes shortly after. We might send response on
  // cells update.
}

void BmsDevice::updateBalancingCells(const DalyBalancingCells &info) {
  status.balancing_mask = info.cell_mask;
  is_online = true;
  last_rx_timestamp = k_uptime_get();

  if (pending_get_balancing_requester != DEOS_NODE_INVALID &&
      BmsGateway::getInstance()) {
    BmsGateway::getInstance()->sendBalancing(this,
                                             pending_get_balancing_requester);
    clearPendingBalancingRequester();
  }
}

void BmsDevice::updateAlarms(const DalyAlarmInfo &info) {
  is_online = true;
  last_rx_timestamp = k_uptime_get();
  
  auto handle_fault = [&](bool active, uint16_t fault_id, deos_fault_severity_t severity) {
      if (active) {
          deos_fault_raise_for_node(deos_node_id, fault_id, severity);
      } else {
          deos_fault_set_inactive_for_node(deos_node_id, fault_id);
      }
  };

  handle_fault(info.pack_overvoltage, DEOS_FAULT_BMS_PACK_OVERVOLTAGE, DEOS_FAULT_SEVERITY_CRITICAL);
  handle_fault(info.pack_undervoltage, DEOS_FAULT_BMS_PACK_UNDERVOLTAGE, DEOS_FAULT_SEVERITY_CRITICAL);
  handle_fault(info.charge_overcurrent, DEOS_FAULT_BMS_CHARGE_OVERCURRENT, DEOS_FAULT_SEVERITY_CRITICAL);
  handle_fault(info.discharge_overcurrent, DEOS_FAULT_BMS_DISCHARGE_OVERCURRENT, DEOS_FAULT_SEVERITY_CRITICAL);
  handle_fault(info.overtemperature, DEOS_FAULT_BMS_OVERTEMPERATURE, DEOS_FAULT_SEVERITY_CRITICAL);
  handle_fault(info.undertemperature, DEOS_FAULT_BMS_UNDERTEMPERATURE, DEOS_FAULT_SEVERITY_CRITICAL);
  handle_fault(info.bms_internal_fault, DEOS_FAULT_BMS_INTERNAL_ERROR, DEOS_FAULT_SEVERITY_ERROR);
}

K_MSGQ_DEFINE(daly_can_rx_queue, sizeof(struct can_frame), 32, 4);

static void rx_callback(const struct device *dev, struct can_frame *frame,
                        void *user_data) {
  k_msgq_put(&daly_can_rx_queue, frame, K_NO_WAIT);
}

static void daly_rx_thread(void) {
  struct can_frame frame;
  while (1) {
    k_msgq_get(&daly_can_rx_queue, &frame, K_FOREVER);
    if (BmsGateway::getInstance()) {
      BmsGateway::getInstance()->processDalyFrame(frame);
    }
  }
}

K_THREAD_DEFINE(gateway_thread_id, 2048, daly_rx_thread, nullptr, nullptr,
                nullptr, 5, 0, 0);

static void daly_polling_thread(void) {
  while (1) {
    if (BmsGateway::getInstance()) {
      BmsGateway::getInstance()->findBmsByBoardId(DALY_BOARD_MAIN)->checkCommStatus();
      BmsGateway::getInstance()->findBmsByBoardId(DALY_BOARD_AUX)->checkCommStatus();

      BmsGateway::getInstance()->sendDalyRequest(0x90, DALY_BOARD_MAIN);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x95, DALY_BOARD_MAIN);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x4D, DALY_BOARD_MAIN);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x4F, DALY_BOARD_MAIN);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x98, DALY_BOARD_MAIN);
      k_msleep(200);

      BmsGateway::getInstance()->sendDalyRequest(0x90, DALY_BOARD_AUX);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x95, DALY_BOARD_AUX);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x4D, DALY_BOARD_AUX);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x4F, DALY_BOARD_AUX);
      k_msleep(100);
      BmsGateway::getInstance()->sendDalyRequest(0x98, DALY_BOARD_AUX);
      k_msleep(200);
    } else {
      k_msleep(1000);
    }
  }
}

K_THREAD_DEFINE(polling_thread_id, 1024, daly_polling_thread, nullptr, nullptr,
                nullptr, 6, 0, 0);

static BmsGateway gateway;

int run_bms_gateway(void) {
  int err = gateway.init();
  if (err)
    return err;

  struct can_filter filter = {0};
  filter.flags = CAN_FILTER_IDE;
  filter.id = 0;
  filter.mask = 0;

  can_add_rx_filter(DEVICE_DT_GET(DALY_CAN_NODE), rx_callback, nullptr,
                    &filter);

  return 0;
}
