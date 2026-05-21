//
// File Path: ESP-NOW-MeshCore/components/device_reset/include/device_reset.h
// Brief:     Header file for the generic Device Reset component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-05-04
// UpdateDate: 2026-05-05
//

#ifndef DEVICE_RESET_H
#define DEVICE_RESET_H

/**
 * @brief Initializes the factory reset background task.
 * 
 *        This task continuously monitors the reset button during normal 
 *        operation and triggers a full memory wipe and reboot if held for 3s.
 */
void device_reset_init(void);

#endif // DEVICE_RESET_H
