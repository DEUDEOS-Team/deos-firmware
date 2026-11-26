#ifndef BMS_GATEWAY_H
#define BMS_GATEWAY_H

#include <deos/deos_icd.h>
#include <stdint.h>

/**
 * @brief Run the BMS Gateway application.
 * 
 * This function initializes the CAN interface and runs the 
 * Gateway proxy logic between DEOS CAN-FD and Classic CAN.
 * 
 * @return int 0 on success, negative on error.
 */
int run_bms_gateway(void);

#endif /* BMS_GATEWAY_H */
