#include "ImuSensor.hpp"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <deos/deos.h>

LOG_MODULE_REGISTER(imu_sensor, LOG_LEVEL_INF);

#define IMU_THREAD_STACK_SIZE 1024
#define IMU_THREAD_PRIORITY 7

static struct k_thread imu_thread_data;
static K_KERNEL_STACK_DEFINE(imu_thread_stack, IMU_THREAD_STACK_SIZE);

static void imu_thread_func(void *p1, void *p2, void *p3) {
    uint32_t counter = 0;
    LOG_INF("IMU Sensor thread started");

    while (1) {
        struct deos_sensor_imu_payload imu_data = {0};
        
        /* Generate some fake data for testing */
        imu_data.accel_x = 100; /* 1.0 m/s^2 */
        imu_data.accel_y = 50;  /* 0.5 m/s^2 */
        imu_data.accel_z = 981; /* 9.81 m/s^2 */
        
        imu_data.gyro_x = 0;
        imu_data.gyro_y = 0;
        imu_data.gyro_z = (int16_t)((counter % 200) - 100); /* Swaying gyro -1.0 to 1.0 deg/s */

        deos_send_from_node(
            DEOS_NODE_IMU,
            DEOS_NODE_ROS_BRIDGE,
            DEOS_PRIO_STATUS,
            DEOS_CLASS_STATUS,
            DEOS_SERVICE_SENSORS,
            DEOS_CMD_SENSOR_IMU_DATA,
            &imu_data,
            sizeof(imu_data)
        );

        counter++;
        k_sleep(K_MSEC(100)); /* 10 Hz */
    }
}

ImuSensor& ImuSensor::getInstance() {
    static ImuSensor instance;
    return instance;
}

void ImuSensor::init() {
    deos_register_local_node(DEOS_NODE_IMU);

    k_thread_create(
        &imu_thread_data,
        imu_thread_stack,
        K_KERNEL_STACK_SIZEOF(imu_thread_stack),
        imu_thread_func,
        NULL, NULL, NULL,
        IMU_THREAD_PRIORITY, 0, K_NO_WAIT
    );
}
