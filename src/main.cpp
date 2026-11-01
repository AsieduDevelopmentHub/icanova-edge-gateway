#include <Arduino.h>

#include "cli/cli.h"
#include "config/config.h"
#include "modbus/modbus_client.h"
#include "rs485/rs485.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "net/http_api.h"
#include "net/wifi_portal.h"
#include "ota/ota.h"
#include "telemetry/snapshot.h"
#include "uplink/uplink.h"
#endif

void setup() {
  Serial.begin(USB_SERIAL_BAUD);
#if defined(ARDUINO_ARCH_ESP32)
  // Drop writes while nobody is listening. Once the host is open, the loop
  // raises this so console echo cannot be discarded.
  Serial.setTxTimeoutMs(0);
#endif

  rs485.begin();
  modbus.begin();
  cli.begin();

#if defined(ARDUINO_ARCH_ESP32)
  Telemetry::begin();
  Telemetry::setIdleHook([]() { cli.poll(); });
  WifiPortal::begin();
  Ota::begin();
  HttpApi::begin();
  Uplink::begin();
#else
  cli.present();
#endif
}

void loop() {
  // Drain USB console first so echo stays snappy even when telemetry or
  // uplink work later in the loop takes longer.
  cli.poll();

#if defined(ARDUINO_ARCH_ESP32)
  // ESP32-C3 HWCDC: Serial's bool can flicker. Debounce so we don't spam the
  // banner or skip RX polling while the user is typing.
  static bool bannerShown = false;
  static uint32_t lastHostOpenMs = 0;
  const bool hostOpen = static_cast<bool>(Serial);

  if (hostOpen) {
    lastHostOpenMs = millis();
    if (!bannerShown) {
      Serial.setTxTimeoutMs(50);
      delay(150);
      Serial.println();
      Serial.println(F("[USB CDC connected]"));
      cli.present();
      bannerShown = true;
    }
  } else if (bannerShown && (millis() - lastHostOpenMs) > 3000) {
    bannerShown = false;
    Serial.setTxTimeoutMs(0);
    cli.onHostDown();
  }

  WifiPortal::poll();
  HttpApi::poll();
  Telemetry::poll();
  Uplink::poll();
  Ota::poll();
  cli.poll();
#endif
}
