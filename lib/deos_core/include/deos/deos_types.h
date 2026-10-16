#ifndef DEOS_TYPES_H
#define DEOS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <deos/deos_icd.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Maximum theoretical command payload for DEOS
 * (64 bytes CAN-FD payload - 4 bytes common header)
 */
#define DEOS_MAX_PAYLOAD_LEN 60

/*
 * Core DEOS Message Structure
 * Reusable container for DEOS protocol messages independent of CAN transport.
 */
typedef struct {
    deos_priority_t priority;
    deos_message_class_t message_class;
    deos_service_id_t service;

    deos_node_id_t destination;
    deos_node_id_t source;

    uint8_t version;
    uint8_t sequence;
    uint8_t command;

    uint8_t payload[DEOS_MAX_PAYLOAD_LEN];
    uint8_t payload_len;
} deos_message_t;

/*
 * Configuration Struct
 */
struct deos_config {
    deos_node_id_t node_id;
    bool router_enabled;
    bool hosted_nodes_enabled;
};

/*
 * Message Handler Type
 */
typedef int (*deos_message_handler_t)(const deos_message_t *message, void *user_data);

/*
 * Transport Type
 */
typedef enum {
    DEOS_TRANSPORT_LOCAL = 0,
    DEOS_TRANSPORT_CAN_FD,
    DEOS_TRANSPORT_ETH_TEXTUAL,
    DEOS_TRANSPORT_ETH_ROS_BRIDGE,
    DEOS_TRANSPORT_UART_LORA,
    DEOS_TRANSPORT_COUNT,
    DEOS_TRANSPORT_BROADCAST_ALL = 0xFE,
    DEOS_TRANSPORT_UNKNOWN = 0xFF
} deos_transport_t;

/*
 * SYSTEM Command Payloads
 */
struct deos_heartbeat_payload {
    uint8_t current_state; /* deos_state_t */
} __attribute__((packed));

#define DEOS_INTERVAL_ON_CHANGE 0xFFFF

struct deos_subscribe_req_payload {
    uint8_t  target_service;
    uint8_t  target_command;
    uint16_t interval_ms;
    uint16_t lease_ttl_s;
} __attribute__((packed));

struct deos_subscribe_resp_payload {
    uint8_t  result_code;
    uint8_t  target_service;
    uint8_t  target_command;
    uint16_t actual_interval_ms;
} __attribute__((packed));

struct deos_unsubscribe_req_payload {
    uint8_t  target_service;
    uint8_t  target_command;
} __attribute__((packed));

/*
 * TRACTION Command Payloads
 */
struct deos_traction_target_payload {
    uint16_t target_speed; /* 0.01 m/s */
    int16_t  target_accel; /* 0.01 m/s^2 */
} __attribute__((packed));

struct deos_traction_gear_payload {
    uint8_t gear; /* deos_traction_gear_t */
} __attribute__((packed));

struct deos_traction_torque_limit_payload {
    uint16_t torque_limit; /* 0.01 % (0..10000) */
} __attribute__((packed));

/*
 * STEERING Command Payloads
 */
struct deos_steering_target_payload {
    int16_t  target_angle;            /* 0.01 deg */
    uint16_t target_angular_velocity; /* 0.01 deg/s */
} __attribute__((packed));

struct deos_steering_mode_payload {
    uint8_t mode;
} __attribute__((packed));

/*
 * BRAKE Command Payloads
 */
struct deos_brake_target_payload {
    uint16_t target_brake_demand; /* 0.01 % */
    int16_t  target_deceleration; /* 0.01 m/s^2 */
} __attribute__((packed));

struct deos_brake_mode_payload {
    uint8_t mode;
} __attribute__((packed));

struct deos_brake_park_payload {
    uint8_t target_park_brake;
} __attribute__((packed));

struct deos_brake_prefill_payload {
    uint8_t prefill_cmd;
} __attribute__((packed));

/*
 * BMS Command Payloads
 */
struct deos_bms_status_payload {
    uint16_t pack_voltage;             /* 0.01 V */
    int16_t  pack_current;             /* 0.1 A */
    uint16_t state_of_charge;          /* 0.01 % */
    int16_t  bms_temperature;          /* 0.1 degC */
    uint16_t minimum_cell_voltage;     /* 0.001 V */
    uint16_t maximum_cell_voltage;     /* 0.001 V */
    uint16_t nominal_pack_voltage;     /* 0.01 V */
    uint8_t  cell_count;
    uint8_t  temperature_sensor_count;
} __attribute__((packed));

struct deos_bms_cell_voltage_payload {
    uint8_t  cell_index;
    uint16_t cell_voltage; /* 0.001 V */
} __attribute__((packed));

struct deos_bms_temperature_payload {
    uint8_t sensor_index;
    int16_t temperature; /* 0.1 degC */
} __attribute__((packed));

struct deos_bms_balancing_payload {
    uint8_t cell_index;
    uint8_t balancing_state;
} __attribute__((packed));

/*
 * DIAGNOSTIC Command Payloads
 */
struct deos_diagnostic_fault_payload {
    uint16_t fault_id;
    uint8_t  severity;
    uint8_t  state;
    uint16_t occurrence_count;
    uint32_t last_occurrence_time; /* ms */
} __attribute__((packed));

/*
 * PARAMETER Command Payloads
 */
struct deos_parameter_payload {
    uint16_t parameter_id;
    uint8_t  value[]; /* Variable length, implicitly defined by DEOS message length - 2 */
} __attribute__((packed));

/*
 * CALIBRATION Command Payloads
 * TODO: Define deos_calibration_start_payload and deos_calibration_status_payload 
 * once the calibration sequences and required sensor arguments are finalized in the ICD.
 */

/*
 * TRACTION Status Payload
 */
struct deos_traction_status_payload {
    uint16_t current_speed;     /* 0.01 m/s */
    uint16_t motor_current;     /* 0.1 A */
    int16_t  inverter_temp;     /* 0.1 degC */
    int16_t  motor_temp;        /* 0.1 degC */
    uint8_t  current_gear;      /* deos_traction_gear_t */
    uint8_t  wheel_slip_state;
} __attribute__((packed));

/*
 * STEERING Status Payload
 */
struct deos_steering_status_payload {
    int16_t  current_angle;     /* 0.01 deg */
    uint16_t motor_current;     /* 0.1 A */
    int16_t  motor_temp;        /* 0.1 degC */
    int16_t  chassis_yaw_rate;  /* 0.01 deg/s */
    uint8_t  is_tracking_target;
} __attribute__((packed));

/*
 * BRAKE Status Payload
 */
struct deos_brake_status_payload {
    uint16_t current_position;           /* 0.01 mm */
    uint16_t actuator_force;             /* 1 N */
    uint16_t motor_current;              /* 0.1 A */
    int16_t  chassis_longitudinal_accel; /* 0.01 m/s^2 */
    uint8_t  brake_system_active;
    uint8_t  park_brake_state;
} __attribute__((packed));

/*
 * SENSORS Status Payload
 */
struct deos_sensor_imu_payload {
    int16_t accel_x;    /* 0.01 m/s^2 */
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;     /* 0.01 deg/s */
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t mag_x;      /* 1 uT or mG (Depends on sensor scaling, typical uT) */
    int16_t mag_y;
    int16_t mag_z;
} __attribute__((packed));

/*
 * LIGHTING Status Payload
 */
struct deos_lighting_status_payload {
    uint8_t headlight_state;   /* deos_light_state_t (0: OFF, 1: ON, 2: BLINKING, 3: FAULT) */
    uint8_t brake_light_state; /* deos_light_state_t */
    uint8_t turn_left_state;   /* deos_light_state_t */
    uint8_t turn_right_state;  /* deos_light_state_t */
    uint8_t hazard_state;      /* deos_light_state_t */
    uint8_t fault_flags;       /* Bitmask: b0=Headlight, b1=Brake, b2=TurnL, b3=TurnR */
} __attribute__((packed));

#ifdef __cplusplus
}
#endif

#endif /* DEOS_TYPES_H */
