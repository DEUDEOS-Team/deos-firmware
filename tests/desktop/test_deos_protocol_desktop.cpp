/**
 * @file test_deos_protocol_desktop.cpp
 * @brief Standalone Desktop / ROS 2 Bridge Test for DEOS Protocol
 *
 * This test verifies that lib/deos_protocol can be compiled and executed on
 * a standard Linux PC (x86_64 / aarch64) with native g++ / gcc without ANY
 * Zephyr RTOS dependencies, RTOS headers, or RTOS build system.
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

#include <deos/deos_frame.h>
#include <deos/deos_types.h>
#include <deos/deos_icd.h>
#include <deos/deos_codec.h>
#include <deos/deos_dispatcher.h>

// Mock ROS 2 Node Context to demonstrate C++ user_data integration
class MockRosBridgeNode {
public:
    MockRosBridgeNode() : m_rx_speed(0), m_rx_gear(0), m_lighting_rx_count(0) {}

    void on_traction_speed(uint16_t speed) {
        m_rx_speed = speed;
    }

    void on_lighting_event() {
        m_lighting_rx_count++;
    }

    uint16_t m_rx_speed;
    uint8_t  m_rx_gear;
    int      m_lighting_rx_count;
};

// Static C callback matching deos_message_handler_t signature
static int ros_bridge_traction_handler(const deos_message_t *msg, void *user_data) {
    auto *node = static_cast<MockRosBridgeNode*>(user_data);
    if (!node || !msg) return -1;

    if (msg->payload_len >= sizeof(struct deos_traction_target_payload)) {
        auto *payload = reinterpret_cast<const struct deos_traction_target_payload*>(msg->payload);
        node->on_traction_speed(payload->target_speed);
    }
    return 0;
}

static int ros_bridge_lighting_handler(const deos_message_t *msg, void *user_data) {
    (void)msg;
    auto *node = static_cast<MockRosBridgeNode*>(user_data);
    if (!node) return -1;
    node->on_lighting_event();
    return 0;
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "DEOS Protocol Standalone Desktop & ROS 2 Bridge Test\n";
    std::cout << "========================================================\n";

    // -------------------------------------------------------------
    // Test 1: Little-Endian Codec
    // -------------------------------------------------------------
    std::cout << "[TEST 1] Testing Little-Endian Codec Helpers... ";
    uint8_t raw_buf[8] = {0};
    deos_put_u16_le(raw_buf, 0x1234);
    assert(raw_buf[0] == 0x34 && raw_buf[1] == 0x12);
    assert(deos_get_u16_le(raw_buf) == 0x1234);

    deos_put_u32_le(raw_buf, 0xAABBCCDD);
    assert(raw_buf[0] == 0xDD && raw_buf[1] == 0xCC && raw_buf[2] == 0xBB && raw_buf[3] == 0xAA);
    assert(deos_get_u32_le(raw_buf) == 0xAABBCCDD);
    std::cout << "PASSED\n";

    // -------------------------------------------------------------
    // Test 2: Universal Frame Encoding and Decoding with CRC
    // -------------------------------------------------------------
    std::cout << "[TEST 2] Testing Universal Frame Encode / Decode with CRC16... ";
    deos_message_t tx_msg = {};
    tx_msg.priority = DEOS_PRIO_CONTROL;
    tx_msg.message_class = DEOS_CLASS_COMMAND;
    tx_msg.service = DEOS_SERVICE_TRACTION;
    tx_msg.command = DEOS_CMD_TRACTION_SET_TARGET_SPEED;
    tx_msg.destination = DEOS_NODE_TRACTION;
    tx_msg.source = DEOS_NODE_ROS_BRIDGE;
    tx_msg.version = DEOS_PROTOCOL_VERSION;
    tx_msg.sequence = 42;

    struct deos_traction_target_payload target_speed = {};
    target_speed.target_speed = 1500; // 15.00 m/s
    target_speed.target_accel = 200;  // 2.00 m/s^2
    memcpy(tx_msg.payload, &target_speed, sizeof(target_speed));
    tx_msg.payload_len = sizeof(target_speed);

    uint8_t wire_frame[128];
    int encoded_len = deos_frame_encode(&tx_msg, wire_frame, sizeof(wire_frame));
    assert(encoded_len == (DEOS_UNIVERSAL_HEADER_SIZE + sizeof(target_speed)));

    deos_message_t rx_msg = {};
    int decode_status = deos_frame_decode(wire_frame, encoded_len, &rx_msg);
    assert(decode_status == 0);
    assert(rx_msg.priority == DEOS_PRIO_CONTROL);
    assert(rx_msg.service == DEOS_SERVICE_TRACTION);
    assert(rx_msg.command == DEOS_CMD_TRACTION_SET_TARGET_SPEED);
    assert(rx_msg.destination == DEOS_NODE_TRACTION);
    assert(rx_msg.source == DEOS_NODE_ROS_BRIDGE);
    assert(rx_msg.sequence == 42);
    assert(rx_msg.payload_len == sizeof(target_speed));

    auto *rx_payload = reinterpret_cast<const struct deos_traction_target_payload*>(rx_msg.payload);
    assert(rx_payload->target_speed == 1500);
    assert(rx_payload->target_accel == 200);

    // Corrupt payload length byte to verify rejection
    uint8_t original_len_byte = wire_frame[DEOS_UF_LEN_OFFSET];
    wire_frame[DEOS_UF_LEN_OFFSET] = 120; // Invalid length > frame_len
    int corrupt_status = deos_frame_decode(wire_frame, encoded_len, &rx_msg);
    assert(corrupt_status != 0); // Must be rejected!
    wire_frame[DEOS_UF_LEN_OFFSET] = original_len_byte; // Restore valid frame
    std::cout << "PASSED\n";

    // -------------------------------------------------------------
    // Test 3: Standalone Dispatcher and C++ user_data Integration
    // -------------------------------------------------------------
    std::cout << "[TEST 3] Testing Standalone Dispatcher Registry & Dispatch... ";
    MockRosBridgeNode ros_node;
    deos_dispatcher_t dispatcher;
    deos_dispatcher_init(&dispatcher);

    int reg_ret1 = deos_dispatcher_register(
        &dispatcher,
        DEOS_CLASS_COMMAND,
        DEOS_SERVICE_TRACTION,
        DEOS_CMD_TRACTION_SET_TARGET_SPEED,
        ros_bridge_traction_handler,
        &ros_node
    );
    assert(reg_ret1 == 0);

    int reg_ret2 = deos_dispatcher_register(
        &dispatcher,
        DEOS_CLASS_STATUS,
        DEOS_SERVICE_LIGHTING,
        DEOS_CMD_LIGHTING_STATUS,
        ros_bridge_lighting_handler,
        &ros_node
    );
    assert(reg_ret2 == 0);

    // Re-decode original valid frame and dispatch it
    assert(deos_frame_decode(wire_frame, encoded_len, &rx_msg) == 0);

    int dispatched_count = deos_dispatcher_dispatch(&dispatcher, &rx_msg);
    assert(dispatched_count == 1);
    assert(ros_node.m_rx_speed == 1500);

    // Dispatch a lighting message
    deos_message_t light_msg = {};
    light_msg.message_class = DEOS_CLASS_STATUS;
    light_msg.service = DEOS_SERVICE_LIGHTING;
    light_msg.command = DEOS_CMD_LIGHTING_STATUS;
    light_msg.destination = DEOS_NODE_GLOBAL_BROADCAST;
    light_msg.payload_len = 6;

    int light_count = deos_dispatcher_dispatch(&dispatcher, &light_msg);
    assert(light_count == 1);
    assert(ros_node.m_lighting_rx_count == 1);

    std::cout << "PASSED\n";

    std::cout << "========================================================\n";
    std::cout << "ALL DESKTOP / ROS 2 BRIDGE TESTS COMPLETED SUCCESSFULLY!\n";
    std::cout << "========================================================\n";
    return 0;
}
