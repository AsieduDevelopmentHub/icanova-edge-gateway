#if defined(ARDUINO_ARCH_ESP32)

#include "net/http_api.h"

#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "config/settings.h"
#include "devices/dse_scf.h"
#include "modbus/modbus_client.h"
#include "net/gencomm_catalog.h"
#include "net/overview_html.h"
#include "ota/ota.h"
#include "telemetry/snapshot.h"
#include "uplink/gts_roots.h"

namespace {

WebServer server(HTTP_SERVER_PORT);
char jsonBuf[4096];

void sendCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleOptions() {
  sendCors();
  server.send(204);
}

void handleRoot() {
  server.send_P(200, "text/html", OVERVIEW_HTML);
}

void handleStatus() {
  sendCors();
  // Avoid Modbus refresh mid-request; serve last snapshot only.
  const size_t n = Telemetry::writeStatusJson(jsonBuf, sizeof(jsonBuf));
  if (n == 0) {
    server.send(500, "application/json", "{\"error\":\"json\"}");
    return;
  }
  server.send(200, "application/json", jsonBuf);
}

void handleHealth() {
  sendCors();
  const size_t n = Telemetry::writeHealthJson(jsonBuf, sizeof(jsonBuf));
  if (n == 0) {
    server.send(500, "application/json", "{\"error\":\"json\"}");
    return;
  }
  server.send(200, "application/json", jsonBuf);
}

void handleNotFound() {
  sendCors();
  server.send(404, "application/json", "{\"error\":\"not found\"}");
}

uint8_t modbusSlave() {
  const uint8_t slave = Settings::get().slave;
  return slave == 0 ? 1 : slave;
}

const char *resultToken(ModbusResult result) {
  switch (result) {
    case ModbusResult::Ok:
      return "ok";
    case ModbusResult::Timeout:
      return "timeout";
    case ModbusResult::CrcError:
      return "crc";
    case ModbusResult::Exception:
      return "exception";
    case ModbusResult::InvalidArgs:
      return "rejected";
    default:
      return "error";
  }
}

void sendJson(int code, const char *body) {
  sendCors();
  server.send(code, "application/json", body);
}

bool parseWords(const String &body, uint16_t *words, uint16_t *count) {
  const int open = body.indexOf('[');
  const int close = body.indexOf(']', open + 1);
  if (open < 0 || close < 0) {
    return false;
  }
  *count = 0;
  int i = open + 1;
  while (i < close && *count < 16) {
    while (i < close && (body[i] == ' ' || body[i] == ',')) {
      i++;
    }
    if (i >= close) {
      break;
    }
    char *end = nullptr;
    const long value = strtol(body.c_str() + i, &end, 0);
    if (end == body.c_str() + i || value < 0 || value > 65535) {
      return false;
    }
    words[(*count)++] = static_cast<uint16_t>(value);
    i = static_cast<int>(end - body.c_str());
  }
  return *count > 0;
}

bool rangeHitsScfPair(uint16_t address, uint16_t quantity) {
  const uint32_t end = static_cast<uint32_t>(address) + quantity;
  return address <= DseScf::kRegKeyInv && end > DseScf::kRegKey;
}

void handleCatalog() {
  sendCors();
  server.send_P(200, "application/json", GENCOMM_CATALOG,
                strlen_P(GENCOMM_CATALOG));
}

void handleRead() {
  if (!server.hasArg("addr") || !server.hasArg("qty")) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"addr and qty required\"}");
    return;
  }
  const long address = strtol(server.arg("addr").c_str(), nullptr, 0);
  const long quantity = strtol(server.arg("qty").c_str(), nullptr, 0);
  if (address < 0 || address > 65535 || quantity < 1 || quantity > 32) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"addr 0..65535, qty 1..32\"}");
    return;
  }
  ModbusTxnInfo info;
  info.quiet = true;
  info.address = static_cast<uint16_t>(address);
  info.quantity = static_cast<uint16_t>(quantity);
  const ModbusResponse resp = modbus.readRegisters(
      modbusSlave(), 0x03, info.address, info.quantity, &info);
  int n = snprintf(jsonBuf, sizeof(jsonBuf),
                   "{\"result\":\"%s\",\"addr\":%ld,\"exception\":%u,\"words\":[",
                   resultToken(resp.result), address,
                   static_cast<unsigned>(resp.exceptionCode));
  if (resp.result == ModbusResult::Ok) {
    for (uint8_t i = 0; i < resp.valueCount && n > 0 &&
                        n < static_cast<int>(sizeof(jsonBuf)) - 16;
         i++) {
      n += snprintf(jsonBuf + n, sizeof(jsonBuf) - static_cast<size_t>(n),
                    "%s%u", i ? "," : "", static_cast<unsigned>(resp.values[i]));
    }
  }
  if (n > 0 && n < static_cast<int>(sizeof(jsonBuf)) - 3) {
    snprintf(jsonBuf + n, sizeof(jsonBuf) - static_cast<size_t>(n), "]}");
  }
  sendJson(resp.result == ModbusResult::Ok ? 200 : 502, jsonBuf);
}

void handleWrite() {
  const String body = server.arg("plain");
  const int addrAt = body.indexOf("\"addr\"");
  if (addrAt < 0) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"addr required\"}");
    return;
  }
  const int colon = body.indexOf(':', addrAt);
  if (colon < 0) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"addr required\"}");
    return;
  }
  const long address = strtol(body.c_str() + colon + 1, nullptr, 0);
  uint16_t words[16];
  uint16_t count = 0;
  if (address < 0 || address > 65535 || !parseWords(body, words, &count)) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"bad addr or words\"}");
    return;
  }
  if (rangeHitsScfPair(static_cast<uint16_t>(address), count)) {
    sendJson(400,
             "{\"result\":\"rejected\",\"error\":\"control keys use /api/control\"}");
    return;
  }
  ModbusTxnInfo info;
  info.quiet = true;
  info.address = static_cast<uint16_t>(address);
  info.quantity = count;
  const ModbusResponse resp = modbus.writeMultipleRegisters(
      modbusSlave(), info.address, words, count, &info);
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"result\":\"%s\",\"addr\":%ld,\"qty\":%u,\"exception\":%u}",
           resultToken(resp.result), address, static_cast<unsigned>(count),
           static_cast<unsigned>(resp.exceptionCode));
  sendJson(resp.result == ModbusResult::Ok ? 200 : 502, jsonBuf);
}

void handleControl() {
  const String body = server.arg("plain");
  const int keyAt = body.indexOf("\"key\"");
  if (keyAt < 0) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"key required\"}");
    return;
  }
  const int colon = body.indexOf(':', keyAt);
  const long key = colon < 0 ? -1 : strtol(body.c_str() + colon + 1, nullptr, 0);
  // 35700 stop and 35776 off both shut the set down.
  if (key == DseScf::kKeyStop || key == 35776) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"stop and off are blocked\"}");
    return;
  }
  if (key < 35700 || key > 35800) {
    sendJson(400, "{\"result\":\"rejected\",\"error\":\"key out of range\"}");
    return;
  }
  ModbusTxnInfo info;
  info.quiet = true;
  info.address = DseScf::kRegKey;
  info.quantity = 2;
  const ModbusResponse resp =
      DseScf::writeKey(modbusSlave(), static_cast<uint16_t>(key), &info);
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"result\":\"%s\",\"key\":%ld,\"exception\":%u}",
           resultToken(resp.result), key,
           static_cast<unsigned>(resp.exceptionCode));
  sendJson(resp.result == ModbusResult::Ok ? 200 : 502, jsonBuf);
}

void handleOtaGet() { Ota::handleStatus(server); }

void handleOtaPost() { Ota::handleUploadDone(server); }

void handleOtaUpload() { Ota::handleUpload(server); }

void handleOtaCloud() { Ota::handleCloud(server); }

void identityHex(char out[13]) {
  uint8_t mac[6] = {};
  const uint64_t efuse = ESP.getEfuseMac();
  memcpy(mac, &efuse, 6);
  for (int i = 0; i < 6; i++) {
    snprintf(out + i * 2, 3, "%02x", mac[i]);
  }
  out[12] = '\0';
}

bool parseFloatField(const String &body, const char *key, float *out) {
  const String needle = String("\"") + key + "\"";
  const int at = body.indexOf(needle);
  if (at < 0) {
    return false;
  }
  const int colon = body.indexOf(':', at);
  if (colon < 0) {
    return false;
  }
  char *end = nullptr;
  const float v = strtof(body.c_str() + colon + 1, &end);
  if (end == body.c_str() + colon + 1) {
    return false;
  }
  *out = v;
  return true;
}

void handleLocationGet() {
  const GatewayConfig &cfg = Settings::get();
  char lat[24] = "null";
  char lon[24] = "null";
  if (cfg.siteLatSet) {
    snprintf(lat, sizeof(lat), "%.6f", static_cast<double>(cfg.siteLat));
  }
  if (cfg.siteLonSet) {
    snprintf(lon, sizeof(lon), "%.6f", static_cast<double>(cfg.siteLon));
  }
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"lat\":%s,\"lon\":%s,\"staConnected\":%s}", lat, lon,
           WiFi.status() == WL_CONNECTED ? "true" : "false");
  sendJson(200, jsonBuf);
}

bool syncLocationToCloud(float lat, float lon, char *err, size_t errN) {
  const GatewayConfig &cfg = Settings::get();
  if (!cfg.uplinkBearer[0] || !cfg.uplinkHost[0]) {
    snprintf(err, errN, "uplink not configured");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(err, errN, "STA not connected");
    return false;
  }
  char id[13];
  identityHex(id);
  char body[160];
  const int bodyN =
      snprintf(body, sizeof(body),
               "{\"identity\":\"%s\",\"latitude\":%.6f,\"longitude\":%.6f}", id,
               static_cast<double>(lat), static_cast<double>(lon));
  if (bodyN <= 0 || bodyN >= static_cast<int>(sizeof(body))) {
    snprintf(err, errN, "body");
    return false;
  }
  WiFiClientSecure client;
  client.setCACert(GTS_ROOTS);
  client.setTimeout(10);
  if (!client.connect(cfg.uplinkHost, cfg.uplinkPort ? cfg.uplinkPort : 443,
                      8000)) {
    snprintf(err, errN, "connect failed");
    return false;
  }
  char head[360];
  const int headN = snprintf(
      head, sizeof(head),
      "POST /ncu/location HTTP/1.1\r\nHost: %s\r\nAuthorization: Bearer %s\r\n"
      "Content-Type: application/json\r\nContent-Length: %d\r\n"
      "Connection: close\r\n\r\n",
      cfg.uplinkHost, cfg.uplinkBearer, bodyN);
  if (headN < 0 || headN >= static_cast<int>(sizeof(head)) ||
      client.write(reinterpret_cast<const uint8_t *>(head),
                   static_cast<size_t>(headN)) != static_cast<size_t>(headN) ||
      client.write(reinterpret_cast<const uint8_t *>(body),
                   static_cast<size_t>(bodyN)) != static_cast<size_t>(bodyN)) {
    snprintf(err, errN, "send failed");
    client.stop();
    return false;
  }
  char line[64];
  const int got = client.readBytesUntil('\n', line, sizeof(line) - 1);
  client.stop();
  if (got <= 0) {
    snprintf(err, errN, "no response");
    return false;
  }
  line[got] = '\0';
  int code = 0;
  sscanf(line, "HTTP/%*s %d", &code);
  if (code < 200 || code >= 300) {
    snprintf(err, errN, "http %d", code);
    return false;
  }
  return true;
}

void handleLocationPost() {
  const String body = server.arg("plain");
  float lat = 0;
  float lon = 0;
  if (!parseFloatField(body, "lat", &lat) &&
      !parseFloatField(body, "latitude", &lat)) {
    sendJson(400, "{\"ok\":false,\"error\":\"lat required\"}");
    return;
  }
  if (!parseFloatField(body, "lon", &lon) &&
      !parseFloatField(body, "longitude", &lon)) {
    sendJson(400, "{\"ok\":false,\"error\":\"lon required\"}");
    return;
  }
  if (lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) {
    sendJson(400, "{\"ok\":false,\"error\":\"out of range\"}");
    return;
  }
  char err[64];
  char latS[24];
  char lonS[24];
  snprintf(latS, sizeof(latS), "%.6f", static_cast<double>(lat));
  snprintf(lonS, sizeof(lonS), "%.6f", static_cast<double>(lon));
  if (!Settings::put("site.lat", latS, err, sizeof(err)) ||
      !Settings::put("site.lon", lonS, err, sizeof(err)) ||
      !Settings::save(err, sizeof(err))) {
    snprintf(jsonBuf, sizeof(jsonBuf), "{\"ok\":false,\"error\":\"%s\"}", err);
    sendJson(500, jsonBuf);
    return;
  }
  bool cloud = body.indexOf("\"sync\":true") >= 0 ||
               body.indexOf("\"sync\": true") >= 0;
  bool cloudOk = false;
  char cloudErr[48] = "";
  if (cloud) {
    cloudOk = syncLocationToCloud(lat, lon, cloudErr, sizeof(cloudErr));
  }
  snprintf(jsonBuf, sizeof(jsonBuf),
           "{\"ok\":true,\"lat\":%.6f,\"lon\":%.6f,\"cloudSynced\":%s%s%s%s}",
           static_cast<double>(lat), static_cast<double>(lon),
           cloud ? (cloudOk ? "true" : "false") : "false",
           cloud && !cloudOk ? ",\"cloudError\":\"" : "",
           cloud && !cloudOk ? cloudErr : "", cloud && !cloudOk ? "\"" : "");
  sendJson(200, jsonBuf);
}

}  // namespace

namespace HttpApi {

void begin() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/health", HTTP_GET, handleHealth);
  server.on("/api/catalog", HTTP_GET, handleCatalog);
  server.on("/api/read", HTTP_GET, handleRead);
  server.on("/api/write", HTTP_POST, handleWrite);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/ota", HTTP_GET, handleOtaGet);
  server.on("/api/ota", HTTP_POST, handleOtaPost, handleOtaUpload);
  server.on("/api/ota/cloud", HTTP_POST, handleOtaCloud);
  server.on("/api/location", HTTP_GET, handleLocationGet);
  server.on("/api/location", HTTP_POST, handleLocationPost);
  server.on("/api/status", HTTP_OPTIONS, handleOptions);
  server.on("/api/health", HTTP_OPTIONS, handleOptions);
  server.on("/api/catalog", HTTP_OPTIONS, handleOptions);
  server.on("/api/read", HTTP_OPTIONS, handleOptions);
  server.on("/api/write", HTTP_OPTIONS, handleOptions);
  server.on("/api/control", HTTP_OPTIONS, handleOptions);
  server.on("/api/ota", HTTP_OPTIONS, handleOptions);
  server.on("/api/ota/cloud", HTTP_OPTIONS, handleOptions);
  server.on("/api/location", HTTP_OPTIONS, handleOptions);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.print(F("[HTTP] Listening on :"));
  Serial.println(HTTP_SERVER_PORT);
}

void poll() { server.handleClient(); }

}  // namespace HttpApi

#endif  // ARDUINO_ARCH_ESP32
