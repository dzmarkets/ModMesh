#ifndef DEVICE_RESET_H
#define DEVICE_RESET_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the smart device reset task.
 * 
 * Monitors FACTORY_RESET_GPIO for:
 * - Short press (<1s): Soft reboot
 * - Double click: Print diagnostics
 * - Long press (>3s): Factory reset (NVS clear + reboot)
 */
void device_reset_init(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_RESET_H
