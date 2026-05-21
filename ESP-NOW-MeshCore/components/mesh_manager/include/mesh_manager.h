//
// File Path: ESP-NOW-MeshCore/components/mesh_manager/include/mesh_manager.h
// Brief:     Header file for mesh_manager component.
//            Manages live peer tracking AND persistent NVS storage.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-30
// UpdateDate: 2026-05-05
//

#ifndef MESH_MANAGER_H
#define MESH_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the mesh manager.
 *        Loads the persisted device count from NVS.
 *        If no value is stored (first boot / erased), starts at 1.
 */
void mesh_manager_init(void);

/**
 * @brief Notify the manager that a new peer MAC has been observed.
 *        If this MAC has not been seen before and the count is below
 *        MESH_MAX_DEVICES, the device count is incremented and the
 *        new count is written to NVS.
 * @param peer_mac 6-byte MAC address of the newly discovered peer.
 * @return true  if the peer was new (count increased).
 * @return false if the peer was already known (count unchanged).
 */
bool mesh_manager_register_peer(const uint8_t *peer_mac);

/**
 * @brief Return the current number of known devices in the mesh
 *        (includes this device itself).
 */
int mesh_manager_get_device_count(void);

/**
 * @brief Return true when the mesh has at least MESH_MIN_DEVICES devices.
 *        Note: This reflects historical registration, not live connectivity.
 */
bool mesh_manager_is_ready(void);

/**
 * @brief Update the activity timestamp for a specific peer.
 *        If the peer is new, it will be registered.
 * @param mac  The 6-byte MAC address of the peer.
 * @param name Optional name of the peer (can be NULL if unknown).
 */
void mesh_manager_update_peer_activity(const uint8_t *mac, const char *name);

/**
 * @brief Check for peers that haven't sent a message recently and mark them offline.
 *        Prints a warning log for each newly offline peer.
 */
void mesh_manager_check_timeouts(void);

/**
 * @brief Returns the number of peers currently marked as online.
 */
int mesh_manager_get_online_peer_count(void);

/**
 * @brief Returns the number of peers that have been heard at least once
 *        since this boot (i.e., last_seen_ms != 0).
 *        Used to distinguish a cold-boot "waiting for first heartbeat" state
 *        from a genuine "peer went offline" partial mesh event.
 */
int mesh_manager_get_ever_seen_count(void);

/**
 * @brief Returns true if at least one remote peer has been seen recently.
 */
bool mesh_manager_is_any_peer_online(void);

/**
 * @brief Returns true if the payload from this MAC is different from the last one seen.
 *        Updates the internal last_payload buffer.
 */
bool mesh_manager_is_payload_new(const uint8_t *mac, const char *payload);

/**
 * @brief Erase all persisted peer data from NVS and clear the in-RAM peer table.
 *
 *        Use this function to perform a factory reset of the mesh membership.
 *        After calling this, the device will restart discovery from scratch,
 *        accepting all new peers it hears as if it has never been part of a mesh.
 *
 *        IMPORTANT: This must be called on ALL remaining nodes when permanently
 *        removing a device from the mesh network. Otherwise, those nodes will
 *        continue to show a PARTIAL MESH (blinking green) indefinitely because
 *        they still expect the removed device to be present.
 *
 *        Typical trigger: Hold a dedicated factory-reset button for 5+ seconds
 *        during device boot before any networking tasks start.
 */
void mesh_manager_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif // MESH_MANAGER_H
