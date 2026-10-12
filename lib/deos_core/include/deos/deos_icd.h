#ifndef DEOS_ICD_H
#define DEOS_ICD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * DEOS Protocol Version
 */
#define DEOS_PROTOCOL_VERSION 0x11

/*
 * DEOS Timing & Limits Configuration
 */
/* CAN-FD Physical Layer */
#define DEOS_CANFD_NOMINAL_BITRATE    500000  /* 500 kbps for Arbitration */
#define DEOS_CANFD_DATA_BITRATE       2000000 /* 2 Mbps for Data Phase */

/* Network Management */
#define DEOS_HEARTBEAT_PERIOD_MS      1000    /* Nodes broadcast heartbeat every 1000ms */
#define DEOS_PING_TIMEOUT_MS          100     /* Max wait time for PONG */

/* Common Header Layout */
#define DEOS_COMMON_HEADER_SIZE 4
#define DEOS_VERSION_OFFSET     0
#define DEOS_SEQUENCE_OFFSET    1
#define DEOS_COMMAND_OFFSET     2
#define DEOS_LENGTH_OFFSET      3
#define DEOS_PAYLOAD_OFFSET     4

/*
 * 29-bit CAN Extended Identifier
 * Bits 28..26 : Priority       (3 bit)
 * Bits 25..22 : Message Class  (4 bit)
 * Bits 21..16 : Service ID     (6 bit)
 * Bits 15..8  : Destination    (8 bit)
 * Bits 7..0   : Source         (8 bit)
 */
#define DEOS_CAN_PRIORITY_SHIFT      26
#define DEOS_CAN_MESSAGE_CLASS_SHIFT 22
#define DEOS_CAN_SERVICE_SHIFT       16
#define DEOS_CAN_DESTINATION_SHIFT   8
#define DEOS_CAN_SOURCE_SHIFT        0

#define DEOS_CAN_PRIORITY_MASK       0x07
#define DEOS_CAN_MESSAGE_CLASS_MASK  0x0F
#define DEOS_CAN_SERVICE_MASK        0x3F
#define DEOS_CAN_DESTINATION_MASK    0xFF
#define DEOS_CAN_SOURCE_MASK         0xFF

/*
 * Priority Registry
 */
typedef enum {
    DEOS_PRIO_EMERGENCY        = 0,
    DEOS_PRIO_CRITICAL_CONTROL = 1,
    DEOS_PRIO_CONTROL          = 2,
    DEOS_PRIO_STATUS           = 3,
    DEOS_PRIO_CONFIG           = 4,
    DEOS_PRIO_NETWORK          = 5,
    DEOS_PRIO_LOW              = 6,
    DEOS_PRIO_BACKGROUND       = 7
} deos_priority_t;

/*
 * Node Registry
 */
typedef enum {
    DEOS_NODE_INVALID        = 0x00,
    DEOS_NODE_TEXTUAL        = 0x01,
    DEOS_NODE_GROUND_CONTROL = 0x02,
    DEOS_NODE_MAIN_STM32     = 0x03,
    DEOS_NODE_ROS_BRIDGE     = 0x04,

    DEOS_NODE_TRACTION       = 0x10,
    DEOS_NODE_STEERING       = 0x11,
    DEOS_NODE_BRAKE          = 0x12,
    DEOS_NODE_IMU            = 0x13,

    DEOS_NODE_BMS_MAIN       = 0x20,
    DEOS_NODE_BMS_AUX        = 0x21,

    DEOS_NODE_BMS_GATEWAY    = 0x30,

    DEOS_NODE_DIAG_TOOL      = 0xF0,

    DEOS_NODE_CAN_BROADCAST      = 0xFE,
    DEOS_NODE_GLOBAL_BROADCAST   = 0xFF,
    DEOS_NODE_BROADCAST          = 0xFF /* Alias */
} deos_node_id_t;

/*
 * Message Class Registry
 * 
 * REQUEST:   bilgi / durum / işlem sonucu talep eder
 * RESPONSE:  REQUEST cevabını veya cevap gerektiren transactional operation sonucunu taşır
 * COMMAND:   çalışma davranışını / gerçek zamanlı kontrolü değiştirir
 * CONFIG:    çalışma parametrelerini değiştirir
 * STATUS:    unsolicited durum / telemetry bilgisidir
 * EVENT:     asynchronous olay / fault bildirimidir
 */
typedef enum {
    DEOS_CLASS_SAFETY   = 0x0,
    DEOS_CLASS_COMMAND  = 0x1,
    DEOS_CLASS_STATUS   = 0x2,
    DEOS_CLASS_EVENT    = 0x3,
    DEOS_CLASS_REQUEST  = 0x4,
    DEOS_CLASS_RESPONSE = 0x5,
    DEOS_CLASS_CONFIG   = 0x6,
    DEOS_CLASS_NETWORK  = 0x7
} deos_message_class_t;

/*
 * Service Registry
 */
typedef enum {
    DEOS_SERVICE_SYSTEM       = 0x00,
    DEOS_SERVICE_TRACTION     = 0x01,
    DEOS_SERVICE_STEERING     = 0x02,
    DEOS_SERVICE_BRAKE        = 0x03,
    DEOS_SERVICE_BMS          = 0x04,
    DEOS_SERVICE_CALIBRATION  = 0x05,
    DEOS_SERVICE_DIAGNOSTIC   = 0x06,
    DEOS_SERVICE_SENSORS      = 0x07
} deos_service_id_t;

/*
 * SYSTEM Commands
 */
typedef enum {
    DEOS_CMD_SYSTEM_GET_STATE     = 0x01,
    DEOS_CMD_SYSTEM_SET_STATE     = 0x02,
    DEOS_CMD_SYSTEM_GET_MODE      = 0x03,
    DEOS_CMD_SYSTEM_SET_MODE      = 0x04,
    DEOS_CMD_SYSTEM_RESET_NODE    = 0x05,
    DEOS_CMD_SYSTEM_GET_NODE_INFO = 0x06,
    DEOS_CMD_SYSTEM_HEARTBEAT     = 0x07,
    DEOS_CMD_SYSTEM_PING          = 0x08,
    DEOS_CMD_SYSTEM_PONG          = 0x09
} deos_system_command_t;

/*
 * TRACTION Commands
 */
typedef enum {
    DEOS_CMD_TRACTION_SET_TARGET_SPEED = 0x01,
    DEOS_CMD_TRACTION_GET_STATUS       = 0x02,
    /* 0x03 reserved */
    DEOS_CMD_TRACTION_SET_GEAR         = 0x04,
    DEOS_CMD_TRACTION_SET_TORQUE_LIMIT = 0x05
} deos_traction_command_t;

/*
 * TRACTION Gears
 */
typedef enum {
    DEOS_GEAR_NEUTRAL = 0x00,
    DEOS_GEAR_DRIVE   = 0x01,
    DEOS_GEAR_REVERSE = 0x02,
    DEOS_GEAR_PARK    = 0x03
} deos_traction_gear_t;

/*
 * TRACTION Parameters
 */
typedef enum {
    DEOS_PARAM_TRACTION_PID_KP               = 0x0001,
    DEOS_PARAM_TRACTION_PID_KI               = 0x0002,
    DEOS_PARAM_TRACTION_PID_KD               = 0x0003,

    DEOS_PARAM_TRACTION_MAX_CURRENT          = 0x0010,
    DEOS_PARAM_TRACTION_MAX_SPEED            = 0x0011,
    DEOS_PARAM_TRACTION_ACCEL_LIMIT          = 0x0012,
    DEOS_PARAM_TRACTION_CMD_TIMEOUT_MS       = 0x0013,
    DEOS_PARAM_TRACTION_POLE_PAIRS           = 0x0014,
    DEOS_PARAM_TRACTION_GEAR_RATIO           = 0x0015,

    DEOS_PARAM_TRACTION_WHEEL_CIRCUM_MM      = 0x0020,
    DEOS_PARAM_TRACTION_REGEN_CUTOFF_SPD     = 0x0022
} deos_traction_parameter_t;

/*
 * STEERING Commands
 */
typedef enum {
    DEOS_CMD_STEERING_SET_TARGET_ANGLE = 0x01,
    DEOS_CMD_STEERING_GET_STATUS       = 0x02,
    /* 0x03 reserved */
    DEOS_CMD_STEERING_SET_MODE         = 0x04
} deos_steering_command_t;

/*
 * STEERING Parameters
 */
typedef enum {
    DEOS_PARAM_STEERING_PID_KP               = 0x0001,
    DEOS_PARAM_STEERING_PID_KI               = 0x0002,
    DEOS_PARAM_STEERING_PID_KD               = 0x0003,
    DEOS_PARAM_STEERING_PID_I_MAX            = 0x0004,
    DEOS_PARAM_STEERING_FEEDFORWARD_K        = 0x0005,
    DEOS_PARAM_STEERING_DEADBAND             = 0x0006,

    DEOS_PARAM_STEERING_ZERO_OFFSET          = 0x0010,
    DEOS_PARAM_STEERING_MIN_ANGLE            = 0x0011,
    DEOS_PARAM_STEERING_MAX_ANGLE            = 0x0012,
    DEOS_PARAM_STEERING_CURRENT_LIMIT        = 0x0013,
    DEOS_PARAM_STEERING_MAX_ANGULAR_VEL      = 0x0014,
    DEOS_PARAM_STEERING_MAX_ANGULAR_ACC      = 0x0015,
    DEOS_PARAM_STEERING_SOFT_STOP_MARGIN     = 0x0016,
    DEOS_PARAM_STEERING_MAX_TRACK_ERR        = 0x0017,
    DEOS_PARAM_STEERING_TRACK_TIMEOUT_MS     = 0x0018
} deos_steering_parameter_t;

/*
 * BRAKE Commands
 */
typedef enum {
    DEOS_CMD_BRAKE_SET_TARGET   = 0x01,
    DEOS_CMD_BRAKE_GET_STATUS   = 0x02,
    /* 0x03 reserved */
    DEOS_CMD_BRAKE_SET_MODE     = 0x04,
    DEOS_CMD_BRAKE_SET_PARK     = 0x05,
    DEOS_CMD_BRAKE_SET_PREFILL  = 0x06
} deos_brake_command_t;

/*
 * BRAKE Parameters
 */
typedef enum {
    DEOS_PARAM_BRAKE_PID_KP                  = 0x0001,
    DEOS_PARAM_BRAKE_PID_KI                  = 0x0002,
    DEOS_PARAM_BRAKE_PID_KD                  = 0x0003,

    DEOS_PARAM_BRAKE_MIN_POSITION            = 0x0010,
    DEOS_PARAM_BRAKE_MAX_POSITION            = 0x0011,
    DEOS_PARAM_BRAKE_CURRENT_LIMIT           = 0x0012,
    DEOS_PARAM_BRAKE_SLEW_RATE_LIMIT         = 0x0013,
    DEOS_PARAM_BRAKE_CMD_TIMEOUT_MS          = 0x0014,
    DEOS_PARAM_BRAKE_KISS_POINT_OFFSET       = 0x0015,
    DEOS_PARAM_BRAKE_MAX_FORCE_LIMIT         = 0x0016
} deos_brake_parameter_t;

/*
 * BMS Commands
 */
typedef enum {
    DEOS_CMD_BMS_GET_STATUS          = 0x01,
    DEOS_CMD_BMS_STATUS              = 0x02,
    DEOS_CMD_BMS_GET_CELL_VOLTAGES   = 0x03,
    DEOS_CMD_BMS_CELL_VOLTAGE        = 0x04,
    DEOS_CMD_BMS_GET_TEMPERATURES    = 0x05,
    DEOS_CMD_BMS_TEMPERATURE         = 0x06,
    DEOS_CMD_BMS_GET_BALANCING       = 0x07,
    DEOS_CMD_BMS_BALANCING           = 0x08
} deos_bms_command_t;

/*
 * CALIBRATION Commands
 */
typedef enum {
    DEOS_CMD_CAL_START        = 0x01,
    DEOS_CMD_CAL_ABORT        = 0x02,
    DEOS_CMD_CAL_GET_STATUS   = 0x03,
    DEOS_CMD_CAL_SAVE_RESULT  = 0x04,
    DEOS_CMD_CAL_CLEAR_RESULT = 0x05
} deos_calibration_command_t;

/*
 * DIAGNOSTIC Commands
 */
typedef enum {
    DEOS_CMD_DIAG_GET_HEALTH   = 0x01,
    DEOS_CMD_DIAG_GET_FAULTS   = 0x02,
    DEOS_CMD_DIAG_CLEAR_FAULTS = 0x03
} deos_diagnostic_command_t;

/*
 * SENSORS Commands
 */
typedef enum {
    DEOS_CMD_SENSOR_IMU_DATA   = 0x01
} deos_sensors_command_t;

/*
 * Parameter Commands
 */
typedef enum {
    DEOS_CMD_GET_PARAMETER      = 0xE0,
    DEOS_CMD_SET_PARAMETER      = 0xE1,
    DEOS_CMD_SAVE_PARAMETERS    = 0xE2,
    DEOS_CMD_RESTORE_DEFAULTS   = 0xE3,
    DEOS_CMD_GET_PARAMETER_INFO = 0xE4
} deos_parameter_command_t;

/*
 * State Registry
 */
typedef enum {
    DEOS_STATE_INIT        = 0x00,
    DEOS_STATE_STANDBY     = 0x01,
    DEOS_STATE_READY       = 0x02,
    DEOS_STATE_ACTIVE      = 0x03,
    DEOS_STATE_CALIBRATING = 0x04,
    DEOS_STATE_SAFE        = 0x05,
    DEOS_STATE_FAULT       = 0x06,
    DEOS_STATE_UNKNOWN     = 0xFF
} deos_state_t;

/*
 * Mode Registry
 */
typedef enum {
    DEOS_MODE_NORMAL     = 0x00,
    DEOS_MODE_MANUAL     = 0x01,
    DEOS_MODE_AUTONOMOUS = 0x02,
    DEOS_MODE_SERVICE    = 0x03,
    DEOS_MODE_TEST       = 0x04,
    DEOS_MODE_UNKNOWN    = 0xFF
} deos_mode_t;

/*
 * Response Result Code Registry
 */
typedef enum {
    DEOS_RESULT_SUCCESS           = 0x00,
    DEOS_RESULT_UNSUPPORTED       = 0x01,
    DEOS_RESULT_INVALID_PARAMETER = 0x02,
    DEOS_RESULT_OUT_OF_RANGE      = 0x03,
    DEOS_RESULT_NOT_ALLOWED       = 0x04,
    DEOS_RESULT_BUSY              = 0x05,
    DEOS_RESULT_NOT_READY         = 0x06,
    DEOS_RESULT_INTERNAL_ERROR    = 0x07
} deos_result_t;

#ifdef __cplusplus
}
#endif

#endif /* DEOS_ICD_H */
