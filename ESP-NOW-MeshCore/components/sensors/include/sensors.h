//
// File Path: ESP-NOW-MeshCore/components/sensors/include/sensors.h
// Brief:     Header file for sensors component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all hardware sensors (I2C, ADC, GPIO, etc.)
 */
void sensors_init(void);

/**
 * @brief Read data from all sensors and format it as a string
 * @param buffer Buffer to hold the formatted string
 * @param max_len Maximum length of the buffer
 */
void sensors_read(char *buffer, size_t max_len);

/**
 * @brief Forcefully resets all internal software states to their initial conditions.
 *        Used during a network-wide emergency reset to prevent nodes from rebroadcasting
 *        stale states after a peer is factory reset.
 */
void sensors_force_initial_state(void);

#ifdef __cplusplus
}
#endif

#endif // SENSORS_H
