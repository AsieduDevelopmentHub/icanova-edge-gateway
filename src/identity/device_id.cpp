#if defined(ARDUINO_ARCH_ESP32)

#include "identity/device_id.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "config/settings.h"

namespace DeviceId {

const char *assigned() {
  const char *id = Settings::get().deviceId;
  return id && id[0] ? id : "";
}

void hwMacHex(char out[13]) {
  uint8_t mac[6] = {};
  const uint64_t efuse = ESP.getEfuseMac();
  memcpy(mac, &efuse, 6);
  for (int i = 0; i < 6; i++) {
    snprintf(out + i * 2, 3, "%02x", mac[i]);
  }
  out[12] = '\0';
}

bool linkIdentity(char *out, size_t outN) {
  if (!out || outN < 5) {
    return false;
  }
  const char *id = assigned();
  if (id[0]) {
    snprintf(out, outN, "%s", id);
    return true;
  }
  if (outN < 13) {
    return false;
  }
  hwMacHex(out);
  return true;
}

}  // namespace DeviceId

#endif
