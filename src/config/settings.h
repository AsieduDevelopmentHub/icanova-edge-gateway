#pragma once

#include <stddef.h>
#include <stdint.h>

// Runtime configuration. On ESP32 the source of truth is /etc/gateway.conf.

struct GatewayConfig {
  char hostname[24];
  char device[16];
  uint8_t slave;
  uint32_t baud;
  uint8_t parity;
  uint8_t fc;
  uint32_t settleUs;
  uint32_t predelayUs;
  bool telemetry;
  char apSsid[33];
  char apPass[64];
  char staSsid[33];
  char staPass[64];
  // auto | ap | sta | apsta | off. auto keeps the AP and joins STA when an SSID is set.
  char wifiMode[8];
  uint16_t idleMin;
  // Station uplink. Off until a host and a 128-bit key are set.
  bool uplinkOn;
  char uplinkHost[64];
  uint16_t uplinkPort;
  char uplinkPath[48];
  uint16_t uplinkPeriodS;
  uint8_t uplinkBatch;
  uint8_t uplinkKey[16];
  bool uplinkKeySet;
  // Bearer for HTTPS JSON to Convex. Empty keeps the LAN NCU1 collector path.
  char uplinkBearer[65];
  // Optional site coordinates (decimal degrees). NaN means unset.
  float siteLat;
  float siteLon;
  bool siteLatSet;
  bool siteLonSet;
  // Cloud OTA pull.
  bool otaEnable;
  uint16_t otaCheckPeriodS;
  char otaChannel[16];
};

namespace Settings {

bool begin();
bool loaded();
const GatewayConfig &get();
const char *hostname();
const char *path();
bool put(const char *key, const char *value, char *err, size_t errN);
bool reload(char *err, size_t errN);
bool save(char *err, size_t errN);
bool format(const char *key, char *out, size_t outN, bool maskSecrets);
void printKeys();
void printSummary();
void setWifiHook(void (*fn)());
// While held, wifi.* changes are saved but the radio is not restarted.
// The matching release applies them once.
void holdApply(bool hold);

}  // namespace Settings
