#pragma once

#if defined(ARDUINO_ARCH_ESP32)

#include <stddef.h>

namespace DeviceId {

/** Assigned product serial / device id used for cloud site linking (may be empty). */
const char *assigned();

/** Always the efuse MAC as 12 lowercase hex (hardware inventory). */
void hwMacHex(char out[13]);

/**
 * Cloud link identity: assigned device.id if set, otherwise efuse MAC.
 * outN should be at least 33.
 */
bool linkIdentity(char *out, size_t outN);

}  // namespace DeviceId

#endif
