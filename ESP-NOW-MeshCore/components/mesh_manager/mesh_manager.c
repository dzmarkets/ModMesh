//
// File Path: ESP-NOW-MeshCore/components/mesh_manager/mesh_manager.c
// Brief:     Source file for mesh_manager component.
//            Manages live peer tracking AND persistent NVS storage of the full
//            Known Peer List (MAC + Name binary blob) so that the mesh can
//            detect FULL vs PARTIAL connectivity immediately after a reboot.
// Author:    M. YOUCEF Yazid (yazid.youcef@gmail.com)
// Version:   0.3.0
// CreateDate: 2026-04-30
// UpdateDate: 2026-05-05
//

#include "mesh_manager.h"
#include "shared_config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_timer.h"

static const char *TAG = "mesh_mgr";

// =============================================================================
// NVS Key Constants
// =============================================================================
#define NVS_KEY_PEER_LIST "peer_list" // Binary blob key
#define NVS_KEY_PEER_CNT  "peer_cnt"  // uint8_t count of valid entries

// =============================================================================
// NVS Storage Structure
// This is the compact, fixed-size record that is written to Flash memory.
// It intentionally has NO pointers, timestamps, or volatile fields.
// Only the identity data (MAC + Name) that must survive across power cycles.
// =============================================================================
typedef struct {
    uint8_t mac[6];    // 6-byte hardware MAC address of the peer
    char    name[32];  // Human-readable name (e.g., "NODE_A")
} nvs_peer_entry_t;

// =============================================================================
// RAM Peer Table (Volatile - Reset on every boot, populated from NVS)
// This table holds the live runtime state: timestamps, online flags, etc.
// =============================================================================
typedef struct {
    uint8_t  mac[6];
    char     name[32];
    char     last_payload[128]; // Last received message content (for delta detection)
    uint32_t last_seen_ms;      // Timestamp of the last received heartbeat
    bool     is_online;         // True if heard within MESH_PEER_TIMEOUT_MS
} peer_info_t;

static peer_info_t s_peer_table[MESH_MAX_DEVICES];
static int         s_peer_count = 0; // Total known peers (loaded from NVS + newly discovered)

// Broadcast MAC guard -- we never register this as a peer
static const uint8_t k_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// =============================================================================
// Internal NVS Helpers
// =============================================================================

/**
 * @brief Save the current peer table to NVS as a binary blob.
 *        Called every time a NEW peer is registered (not on every activity update).
 *        This is a relatively slow operation (Flash write) and must NOT be called
 *        from a high-frequency path like the sensor or actuator task.
 */
static void save_peers_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(MESH_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed (%s) – peer list not saved", esp_err_to_name(err));
        return;
    }

    // Build a compact NVS-safe array (no pointers, no timestamps)
    nvs_peer_entry_t blob[MESH_MAX_DEVICES];
    memset(blob, 0, sizeof(blob));

    for (int i = 0; i < s_peer_count; i++) {
        memcpy(blob[i].mac,  s_peer_table[i].mac,  6);
        strncpy(blob[i].name, s_peer_table[i].name, sizeof(blob[i].name) - 1);
    }

    // Write the count key first, then the blob
    err = nvs_set_u8(h, NVS_KEY_PEER_CNT, (uint8_t)s_peer_count);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, NVS_KEY_PEER_LIST, blob, sizeof(blob));
    }

    if (err == ESP_OK) {
        nvs_commit(h);
        ESP_LOGI(TAG, "Peer list (%d entries) saved to NVS successfully", s_peer_count);
    } else {
        ESP_LOGW(TAG, "NVS write failed (%s)", esp_err_to_name(err));
    }

    nvs_close(h);
}

/**
 * @brief Load the persisted peer list from NVS into the RAM peer table.
 *        All loaded peers are inserted with is_online=false and last_seen_ms=0.
 *        Returns the number of peers successfully loaded.
 */
static int load_peers_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(MESH_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No NVS peer list found – starting fresh (first boot)");
        return 0;
    }

    // Read the count
    uint8_t count = 0;
    err = nvs_get_u8(h, NVS_KEY_PEER_CNT, &count);
    if (err != ESP_OK || count == 0) {
        ESP_LOGI(TAG, "No peers stored in NVS (count=0)");
        nvs_close(h);
        return 0;
    }

    // Clamp to valid range
    if (count > MESH_MAX_DEVICES) count = MESH_MAX_DEVICES;

    // Read the blob
    nvs_peer_entry_t blob[MESH_MAX_DEVICES];
    memset(blob, 0, sizeof(blob));
    size_t required_size = sizeof(blob);

    err = nvs_get_blob(h, NVS_KEY_PEER_LIST, blob, &required_size);
    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS peer blob read failed (%s)", esp_err_to_name(err));
        return 0;
    }

    // Populate the RAM table. Mark all as OFFLINE until a heartbeat is received.
    int loaded = 0;
    for (int i = 0; i < count; i++) {
        // Sanity check: skip empty MAC entries
        bool is_empty = true;
        for (int j = 0; j < 6; j++) {
            if (blob[i].mac[j] != 0x00) { is_empty = false; break; }
        }
        if (is_empty) continue;

        peer_info_t *p = &s_peer_table[loaded];
        memcpy(p->mac,  blob[i].mac,  6);
        strncpy(p->name, blob[i].name, sizeof(p->name) - 1);
        p->last_seen_ms = 0;         // Unknown until first heartbeat
        p->is_online    = false;     // Assume offline until proven otherwise
        memset(p->last_payload, 0, sizeof(p->last_payload));

        ESP_LOGI(TAG, "Loaded peer %d from NVS: %s (" MACSTR ") [OFFLINE until heartbeat]",
                 loaded + 1, p->name, MAC2STR(p->mac));
        loaded++;
    }

    return loaded;
}

// =============================================================================
// Internal: find a peer by MAC, returns pointer or NULL
// =============================================================================
static peer_info_t* find_peer(const uint8_t *mac)
{
    for (int i = 0; i < s_peer_count; i++) {
        if (memcmp(s_peer_table[i].mac, mac, 6) == 0) {
            return &s_peer_table[i];
        }
    }
    return NULL;
}

// =============================================================================
// Public API
// =============================================================================

void mesh_manager_init(void)
{
    memset(s_peer_table, 0, sizeof(s_peer_table));
    s_peer_count = 0;

    // Load the Known Peer List from NVS.
    // These peers are inserted into the RAM table as OFFLINE.
    // The LED logic will show PARTIAL MESH immediately after boot if any are missing.
    s_peer_count = load_peers_from_nvs();

    ESP_LOGI(TAG, "Mesh manager ready – %d known peers loaded from NVS (max: %d)",
             s_peer_count, MESH_MAX_DEVICES);
}

bool mesh_manager_register_peer(const uint8_t *peer_mac)
{
    if (peer_mac == NULL) return false;

    // Ignore broadcast MAC
    if (memcmp(peer_mac, k_broadcast_mac, 6) == 0) return false;

    // If already in RAM table, this is not a new peer
    if (find_peer(peer_mac) != NULL) return false;

    // Check capacity
    if (s_peer_count >= MESH_MAX_DEVICES) {
        ESP_LOGW(TAG, "Peer table full (%d) – cannot register " MACSTR,
                 MESH_MAX_DEVICES, MAC2STR(peer_mac));
        return false;
    }

    // Insert into RAM table as online (we're seeing them right now)
    peer_info_t *p = &s_peer_table[s_peer_count];
    memcpy(p->mac, peer_mac, 6);
    snprintf(p->name, sizeof(p->name), "PEER_%d", s_peer_count + 1);
    p->last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000);
    p->is_online    = true;
    memset(p->last_payload, 0, sizeof(p->last_payload));
    s_peer_count++;

    ESP_LOGI(TAG, "New peer discovered and registered: " MACSTR " (total known: %d)",
             MAC2STR(peer_mac), s_peer_count);

    // Persist the updated peer list to NVS so it survives reboots
    save_peers_to_nvs();

    return true;
}

void mesh_manager_update_peer_activity(const uint8_t *mac, const char *name)
{
    if (!mac) return;

    peer_info_t *p = find_peer(mac);

    if (!p) {
        // First time we see this MAC – register it (which also saves to NVS)
        mesh_manager_register_peer(mac);
        p = find_peer(mac);
    }

    if (p) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        p->last_seen_ms = now;

        // If the device was offline, it just came back online – log the event
        if (!p->is_online) {
            p->is_online = true;
            ESP_LOGI(TAG, "Device %s (" MACSTR ") is BACK ONLINE",
                     p->name, MAC2STR(p->mac));
        }

        // Update the human-readable name if the payload provided one
        // and it is different from the current placeholder name.
        if (name && strlen(name) > 0) {
            if (strncmp(p->name, name, sizeof(p->name)) != 0) {
                strncpy(p->name, name, sizeof(p->name) - 1);
                p->name[sizeof(p->name) - 1] = '\0';
                // Save again because the name just improved (e.g., "PEER_1" → "NODE_B")
                save_peers_to_nvs();
                ESP_LOGI(TAG, "Peer " MACSTR " name updated to '%s' – NVS synced",
                         MAC2STR(p->mac), p->name);
            }
        }
    }
}

void mesh_manager_check_timeouts(void)
{
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

    for (int i = 0; i < s_peer_count; i++) {
        peer_info_t *p = &s_peer_table[i];

        // Only check peers that have been heard at least once (last_seen_ms != 0)
        // Newly loaded NVS peers with last_seen_ms==0 stay OFFLINE until first heartbeat.
        if (p->is_online && p->last_seen_ms != 0) {
            if ((now - p->last_seen_ms) > MESH_PEER_TIMEOUT_MS) {
                p->is_online = false;
                ESP_LOGW(TAG, "Device %s (" MACSTR ") is OFFLINE (no heartbeat for %lus)",
                         p->name, MAC2STR(p->mac),
                         (unsigned long)MESH_PEER_TIMEOUT_MS / 1000);
            }
        }
    }
}

int mesh_manager_get_online_peer_count(void)
{
    int online = 0;
    for (int i = 0; i < s_peer_count; i++) {
        if (s_peer_table[i].is_online) online++;
    }
    return online;
}

int mesh_manager_get_ever_seen_count(void)
{
    int seen = 0;
    for (int i = 0; i < s_peer_count; i++) {
        // A peer has been "seen" if it has a non-zero last_seen timestamp
        if (s_peer_table[i].last_seen_ms != 0) seen++;
    }
    return seen;
}

bool mesh_manager_is_any_peer_online(void)
{
    return mesh_manager_get_online_peer_count() > 0;
}

int mesh_manager_get_device_count(void)
{
    // Total known devices = this node (1) + all stored peers
    return s_peer_count + 1;
}

bool mesh_manager_is_ready(void)
{
    return mesh_manager_get_device_count() >= MESH_MIN_DEVICES;
}

bool mesh_manager_is_payload_new(const uint8_t *mac, const char *payload)
{
    if (!mac || !payload) return false;

    peer_info_t *p = find_peer(mac);
    if (!p) return true; // New peer, everything is new by definition

    if (strcmp(p->last_payload, payload) == 0) {
        return false; // Identical to last message – suppress re-processing
    }

    // Update the stored payload to the new value
    strncpy(p->last_payload, payload, sizeof(p->last_payload) - 1);
    p->last_payload[sizeof(p->last_payload) - 1] = '\0';
    return true;
}

void mesh_manager_factory_reset(void)
{
    ESP_LOGW(TAG, "=== FACTORY RESET TRIGGERED ===");
    ESP_LOGW(TAG, "Erasing all peer data from NVS and clearing RAM table...");

    // --- Step 1: Erase NVS keys ---
    nvs_handle_t h;
    esp_err_t err = nvs_open(MESH_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_erase_key(h, NVS_KEY_PEER_LIST);
        nvs_erase_key(h, NVS_KEY_PEER_CNT);
        // Also erase the legacy count key if it exists from older firmware
        nvs_erase_key(h, MESH_NVS_KEY_COUNT);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG, "NVS peer data erased successfully");
    } else {
        ESP_LOGW(TAG, "NVS open failed during reset (%s) – may already be empty",
                 esp_err_to_name(err));
    }

    // --- Step 2: Clear the in-RAM peer table ---
    memset(s_peer_table, 0, sizeof(s_peer_table));
    s_peer_count = 0;

    ESP_LOGW(TAG, "=== FACTORY RESET COMPLETE ===");
    ESP_LOGW(TAG, "This node will now rediscover all mesh peers from scratch.");
    ESP_LOGW(TAG, "IMPORTANT: Run this reset on ALL remaining nodes to ensure");
    ESP_LOGW(TAG, "           they stop expecting the removed device.");
}
