#if defined(ARDUINO_ARCH_ESP32)

#include "ota/ota.h"

#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/settings.h"
#include "ota/build_info.h"
#include "uplink/gts_roots.h"

namespace Ota {
namespace {

bool gBusy = false;
bool gRebootPending = false;
int gProgress = 0;
char gErr[96] = "";
uint32_t gLastCheckMs = 0;
size_t gUploadWritten = 0;

void setErr(const char *msg) {
  snprintf(gErr, sizeof(gErr), "%s", msg ? msg : "");
}

void clearErr() { gErr[0] = '\0'; }

const char *slotLabel(const esp_partition_t *part) {
  if (!part) {
    return "unknown";
  }
  if (part->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
    return "ota_0";
  }
  if (part->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
    return "ota_1";
  }
  return "app";
}

void identityHex(char out[13]) {
  uint8_t mac[6] = {};
  const uint64_t efuse = ESP.getEfuseMac();
  memcpy(mac, &efuse, 6);
  for (int i = 0; i < 6; i++) {
    snprintf(out + i * 2, 3, "%02x", mac[i]);
  }
  out[12] = '\0';
}

bool parseJsonString(const char *json, const char *key, char *out, size_t outN) {
  if (!json || !key || !out || outN < 2) {
    return false;
  }
  char needle[48];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char *at = strstr(json, needle);
  if (!at) {
    return false;
  }
  at = strchr(at + strlen(needle), ':');
  if (!at) {
    return false;
  }
  at++;
  while (*at == ' ' || *at == '\t') {
    at++;
  }
  if (*at != '"') {
    return false;
  }
  at++;
  size_t i = 0;
  while (*at && *at != '"' && i + 1 < outN) {
    if (*at == '\\' && at[1]) {
      at++;
    }
    out[i++] = *at++;
  }
  out[i] = '\0';
  return i > 0;
}

bool parseJsonU32(const char *json, const char *key, uint32_t *out) {
  if (!json || !key || !out) {
    return false;
  }
  char needle[48];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char *at = strstr(json, needle);
  if (!at) {
    return false;
  }
  at = strchr(at + strlen(needle), ':');
  if (!at) {
    return false;
  }
  at++;
  while (*at == ' ' || *at == '\t') {
    at++;
  }
  char *end = nullptr;
  const unsigned long v = strtoul(at, &end, 10);
  if (end == at) {
    return false;
  }
  *out = static_cast<uint32_t>(v);
  return true;
}

bool hexNibble(char c, uint8_t *out) {
  if (c >= '0' && c <= '9') {
    *out = static_cast<uint8_t>(c - '0');
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    *out = static_cast<uint8_t>(c - 'a' + 10);
    return true;
  }
  if (c >= 'A' && c <= 'F') {
    *out = static_cast<uint8_t>(c - 'A' + 10);
    return true;
  }
  return false;
}

bool parseSha256Hex(const char *hex, uint8_t out[32]) {
  if (!hex || strlen(hex) != 64) {
    return false;
  }
  for (int i = 0; i < 32; i++) {
    uint8_t hi = 0;
    uint8_t lo = 0;
    if (!hexNibble(hex[i * 2], &hi) || !hexNibble(hex[i * 2 + 1], &lo)) {
      return false;
    }
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

bool stationReady() {
  return WiFi.status() == WL_CONNECTED;
}

bool downloadAndFlash(const char *url, uint32_t expectedSize, const uint8_t expectedSha[32]) {
  if (!url || !url[0]) {
    setErr("missing url");
    return false;
  }
  WiFiClientSecure client;
  client.setCACert(GTS_ROOTS);
  client.setTimeout(20);
  HTTPClient http;
  if (!http.begin(client, url)) {
    setErr("http begin failed");
    return false;
  }
  http.setTimeout(20000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    setErr("download http error");
    http.end();
    return false;
  }
  const int len = http.getSize();
  if (expectedSize > 0 && len > 0 && static_cast<uint32_t>(len) != expectedSize) {
    setErr("size mismatch");
    http.end();
    return false;
  }
  const size_t contentLen = len > 0 ? static_cast<size_t>(len) : expectedSize;
  if (contentLen == 0 || contentLen > 0x180000) {
    setErr("bad size");
    http.end();
    return false;
  }
  if (!Update.begin(contentLen)) {
    setErr("Update.begin failed");
    http.end();
    return false;
  }
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t written = 0;
  gProgress = 0;
  while (http.connected() && written < contentLen) {
    const size_t avail = stream->available();
    if (!avail) {
      delay(1);
      continue;
    }
    const size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
    if (n == 0) {
      break;
    }
    if (Update.write(buf, n) != n) {
      setErr("Update.write failed");
      Update.abort();
      mbedtls_sha256_free(&sha);
      http.end();
      return false;
    }
    mbedtls_sha256_update(&sha, buf, n);
    written += n;
    gProgress = static_cast<int>((written * 100) / contentLen);
  }
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  http.end();
  if (written != contentLen) {
    setErr("short download");
    Update.abort();
    return false;
  }
  if (memcmp(digest, expectedSha, 32) != 0) {
    setErr("sha256 mismatch");
    Update.abort();
    return false;
  }
  if (!Update.end(true)) {
    setErr("Update.end failed");
    return false;
  }
  gProgress = 100;
  return true;
}

}  // namespace

void begin() {
  clearErr();
  gBusy = false;
  gProgress = 0;
  gUploadWritten = 0;
  gRebootPending = false;
  gLastCheckMs = 0;
}

void poll() {
  if (gRebootPending) {
    gRebootPending = false;
    delay(250);
    ESP.restart();
    return;
  }
  const GatewayConfig &cfg = Settings::get();
  if (!cfg.otaEnable || gBusy || !stationReady() || !cfg.uplinkBearer[0] ||
      !cfg.uplinkHost[0]) {
    return;
  }
  const uint32_t periodMs =
      static_cast<uint32_t>(cfg.otaCheckPeriodS) * 1000ul;
  if (gLastCheckMs != 0 && (millis() - gLastCheckMs) < periodMs) {
    return;
  }
  gLastCheckMs = millis();
  checkCloud(true);
}

const char *version() { return FW_VERSION; }
const char *gitHash() { return GIT_HASH; }
const char *lastError() { return gErr; }
bool busy() { return gBusy; }
int progressPct() { return gProgress; }

const char *runningSlot() {
  return slotLabel(esp_ota_get_running_partition());
}

void handleStatus(WebServer &server) {
  char body[320];
  snprintf(body, sizeof(body),
           "{\"fw\":\"%s\",\"git\":\"%s\",\"slot\":\"%s\",\"busy\":%s,"
           "\"progress\":%d,\"error\":%s%s%s,\"otaEnable\":%s,\"channel\":\"%s\"}",
           FW_VERSION, GIT_HASH, runningSlot(), gBusy ? "true" : "false",
           gProgress, gErr[0] ? "\"" : "null", gErr[0] ? gErr : "",
           gErr[0] ? "\"" : "", Settings::get().otaEnable ? "true" : "false",
           Settings::get().otaChannel[0] ? Settings::get().otaChannel : "stable");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", body);
}

void handleUpload(WebServer &server) {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    clearErr();
    gBusy = true;
    gProgress = 0;
    gUploadWritten = 0;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      setErr("Update.begin failed");
      gBusy = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (gBusy && !gErr[0]) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        setErr("Update.write failed");
        Update.abort();
        gBusy = false;
      } else {
        gUploadWritten += upload.currentSize;
        if (upload.totalSize > 0) {
          gProgress = static_cast<int>((gUploadWritten * 100) / upload.totalSize);
        }
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (gBusy && !gErr[0]) {
      if (!Update.end(true)) {
        setErr("Update.end failed");
        gBusy = false;
      } else {
        gProgress = 100;
      }
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    setErr("upload aborted");
    Update.abort();
    gBusy = false;
  }
}

void handleUploadDone(WebServer &server) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (gErr[0]) {
    char body[160];
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}", gErr);
    gBusy = false;
    server.send(500, "application/json", body);
    return;
  }
  if (!Update.hasError() && gProgress == 100) {
    gBusy = false;
    gRebootPending = true;
    server.send(200, "application/json",
                "{\"ok\":true,\"rebooting\":true}");
    return;
  }
  gBusy = false;
  server.send(500, "application/json", "{\"ok\":false,\"error\":\"incomplete\"}");
}

void handleCloud(WebServer &server) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  bool apply = true;
  if (server.hasArg("plain")) {
    const String body = server.arg("plain");
    if (body.indexOf("\"apply\":false") >= 0 || body.indexOf("\"apply\": false") >= 0) {
      apply = false;
    }
  }
  if (gBusy) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"busy\"}");
    return;
  }
  checkCloud(apply);
  char body[240];
  snprintf(body, sizeof(body),
           "{\"ok\":%s,\"error\":%s%s%s,\"progress\":%d,\"rebooting\":%s}",
           gErr[0] ? "false" : "true", gErr[0] ? "\"" : "null",
           gErr[0] ? gErr : "", gErr[0] ? "\"" : "", gProgress,
           gRebootPending ? "true" : "false");
  server.send(gErr[0] ? 502 : 200, "application/json", body);
}

void checkCloud(bool applyIfNewer) {
  if (gBusy) {
    setErr("busy");
    return;
  }
  clearErr();
  const GatewayConfig &cfg = Settings::get();
  if (!cfg.uplinkBearer[0] || !cfg.uplinkHost[0]) {
    setErr("uplink not configured");
    return;
  }
  if (!stationReady()) {
    setErr("STA not connected");
    return;
  }
  gBusy = true;
  gProgress = 0;

  char id[13];
  identityHex(id);
  const char *channel = cfg.otaChannel[0] ? cfg.otaChannel : "stable";
  char path[96];
  snprintf(path, sizeof(path), "/ncu/ota/check?channel=%s&current=%s&identity=%s",
           channel, FW_VERSION, id);

  WiFiClientSecure client;
  client.setCACert(GTS_ROOTS);
  client.setTimeout(12);
  if (!client.connect(cfg.uplinkHost, cfg.uplinkPort ? cfg.uplinkPort : 443, 8000)) {
    setErr("connect failed");
    gBusy = false;
    return;
  }
  char req[420];
  const int reqN = snprintf(
      req, sizeof(req),
      "GET %s HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\n"
      "Connection: close\r\n\r\n",
      path, cfg.uplinkHost, cfg.uplinkBearer);
  if (reqN < 0 || reqN >= static_cast<int>(sizeof(req)) ||
      client.write(reinterpret_cast<const uint8_t *>(req),
                   static_cast<size_t>(reqN)) != static_cast<size_t>(reqN)) {
    setErr("send failed");
    client.stop();
    gBusy = false;
    return;
  }
  char statusLine[96];
  const int got = client.readBytesUntil('\n', statusLine, sizeof(statusLine) - 1);
  if (got <= 0) {
    setErr("no response");
    client.stop();
    gBusy = false;
    return;
  }
  statusLine[got] = '\0';
  int code = 0;
  sscanf(statusLine, "HTTP/%*s %d", &code);
  // Skip headers.
  while (client.connected()) {
    char line[128];
    const int n = client.readBytesUntil('\n', line, sizeof(line) - 1);
    if (n <= 0) {
      break;
    }
    line[n] = '\0';
    if (n <= 2) {
      break;
    }
  }
  char body[1536];
  size_t bodyN = 0;
  while (client.connected() && client.available() && bodyN + 1 < sizeof(body)) {
    body[bodyN++] = static_cast<char>(client.read());
  }
  body[bodyN] = '\0';
  client.stop();

  if (code == 204 || strstr(body, "\"upToDate\":true") ||
      strstr(body, "\"status\":\"up-to-date\"")) {
    clearErr();
    gBusy = false;
    gProgress = 0;
    return;
  }
  if (code != 200) {
    setErr("check failed");
    gBusy = false;
    return;
  }

  char version[40] = "";
  char shaHex[72] = "";
  char url[768] = "";
  uint32_t size = 0;
  parseJsonString(body, "version", version, sizeof(version));
  parseJsonString(body, "sha256", shaHex, sizeof(shaHex));
  parseJsonString(body, "url", url, sizeof(url));
  parseJsonU32(body, "size", &size);
  if (!version[0] || !shaHex[0] || !url[0] || size == 0) {
    setErr("bad check payload");
    gBusy = false;
    return;
  }
  if (strcmp(version, FW_VERSION) == 0) {
    clearErr();
    gBusy = false;
    return;
  }
  if (!applyIfNewer) {
    clearErr();
    gBusy = false;
    // Leave a soft message in error for UI: available version.
    snprintf(gErr, sizeof(gErr), "available:%s", version);
    return;
  }

  uint8_t sha[32];
  if (!parseSha256Hex(shaHex, sha)) {
    setErr("bad sha256");
    gBusy = false;
    return;
  }
  if (!downloadAndFlash(url, size, sha)) {
    gBusy = false;
    return;
  }
  clearErr();
  gBusy = false;
  gRebootPending = true;
}

}  // namespace Ota

#endif
