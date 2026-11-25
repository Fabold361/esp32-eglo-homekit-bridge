// =====================================================================
//  BlueKit configuration
//  1. Copy this file and name the copy  secrets.h  (same folder)
//  2. Replace every "..." value with your own data (see README)
//  secrets.h is in .gitignore, so your data never ends up on GitHub.
// =====================================================================
#pragma once

// ---- WiFi (2.4 GHz – the ESP32 can't do 5 GHz) ----
#define WIFI_SSID_S       "YOUR_WIFI_NAME"
#define WIFI_PASS_S       "YOUR_WIFI_PASSWORD"

// ---- Lamp (run tools/get_awox_mesh.py to get these) ----
#define LAMP_MAC_S        "aa:bb:cc:dd:ee:ff"   // MAC address of the lamp (lowercase)
#define MESH_NAME_S       "YOUR_MESH_NAME"      // client_id
#define MESH_PASS_S       "YOUR_MESH_PASSWORD"  // access_token of the "zigbee" credential
#define MESH_ID_S         1234                  // Mesh-ID of the lamp (number, no quotes)

// ---- Apple Home ----
#define ACCESSORY_NAME_S  "Lamp"                // name shown in the Home app
#define PAIRING_CODE_S    "46637726"            // 8 digits, no dashes -> code 466-37-726
                                                // not allowed: 12345678, 87654321, 11111111 ...
