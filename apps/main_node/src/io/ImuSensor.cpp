#include "ImuSensor.hpp"
#include <errno.h>
#include <deos/deos.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(imu_sensor, LOG_LEVEL_INF);

static int imu_data_request(const deos_message_t *request, void *user_data) {
  ARG_UNUSED(user_data);

  if (request->destination != DEOS_NODE_IMU || request->payload_len != 0) {
    return -EINVAL;
  }

  struct deos_sensor_imu_payload sample = {};
  sample.accel_x = 100; /* 1.0 m/s^2 */
  sample.accel_y = 50;  /* 0.5 m/s^2 */
  sample.accel_z = 981; /* 9.81 m/s^2 */
  const uint32_t phase = (k_uptime_get_32() / 100U) % 200U;
  sample.gyro_z = static_cast<int16_t>(static_cast<int32_t>(phase) - 100);

  return deos_send_from_node(DEOS_NODE_IMU, request->source, DEOS_PRIO_STATUS,
                             DEOS_CLASS_STATUS, DEOS_SERVICE_SENSORS,
                             DEOS_CMD_SENSOR_IMU_DATA, &sample, sizeof(sample));
}

ImuSensor &ImuSensor::getInstance() {
  static ImuSensor instance;
  return instance;
}

void ImuSensor::init() {
  const int node_result = deos_register_local_node(DEOS_NODE_IMU);
  if (node_result != 0) {
    LOG_ERR("Failed to register IMU node: %d", node_result);
    return;
  }

  const int handler_result = deos_register_handler_for_node(
      DEOS_NODE_IMU, DEOS_CLASS_REQUEST, DEOS_SERVICE_SENSORS,
      DEOS_CMD_SENSOR_IMU_DATA, imu_data_request, nullptr);
  if (handler_result != 0) {
    LOG_ERR("Failed to register IMU data handler: %d", handler_result);
  }
}
