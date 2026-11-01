#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <stddef.h>
#include <stdint.h>

#include <WebServer.h>

namespace Ota {

void begin();
void poll();

// Soft-AP streaming handlers (register with WebServer).
void handleStatus(WebServer &server);
void handleUpload(WebServer &server);
void handleUploadDone(WebServer &server);

// Soft-AP: trigger cloud check + optional apply (POST body: {"apply":true}).
void handleCloud(WebServer &server);

// Background cloud check when ota.enable is on.
void checkCloud(bool applyIfNewer);

const char *version();
const char *gitHash();
const char *lastError();
bool busy();
int progressPct();
const char *runningSlot();

}  // namespace Ota

#endif
