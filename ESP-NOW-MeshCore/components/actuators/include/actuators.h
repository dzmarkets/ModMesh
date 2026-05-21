//
// File Path: ESP-NOW-MeshCore/components/actuators/include/actuators.h
// Brief:     Header file for actuators component.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-26
// UpdateDate: 2026-05-05
//

#ifndef ACTUATORS_H
#define ACTUATORS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all hardware actuators (Relays, Motors, LEDs)
 */
void actuators_init(void);

/**
 * @brief Parse the incoming payload and trigger the correct hardware action
 * @param payload The incoming message string from the mesh network
 */
void actuators_execute(const char *payload);

#ifdef __cplusplus
}
#endif

#endif // ACTUATORS_H
