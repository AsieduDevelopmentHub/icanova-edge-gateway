#include "telemetry/snapshot.h"

#include <stdio.h>
#include <string.h>

#include <Arduino.h>

#include "config/config.h"
#include "devices/dse7320_registers.h"
#include "devices/modbus_device.h"
#include "modbus/modbus_client.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "ota/build_info.h"
#endif

namespace {

TelemetrySnapshot gSnap;
uint32_t gLastRefreshMs = 0;
bool gAutoRefresh = false;  // CLI owns the bus until "set telemetry on"
void (*gIdleHook)() = nullptr;
constexpr uint32_t kRefreshIntervalMs = 2500;

void yieldIdle() {
  if (gIdleHook) {
    gIdleHook();
  }
}

void setErr(const char *msg) {
  strncpy(gSnap.lastError, msg ? msg : "", sizeof(gSnap.lastError) - 1);
  gSnap.lastError[sizeof(gSnap.lastError) - 1] = '\0';
}

void applyDecoded(const char *key, const DseDecodedValue &v) {
  if (!v.ok || v.invalid) {
    return;
  }
  if (strcmp(key, "op_mode") == 0) {
    gSnap.hasOpMode = true;
    gSnap.opModeCode = static_cast<uint16_t>(v.rawU);
    strncpy(gSnap.opModeName, DseRegisters::opModeName(gSnap.opModeCode),
            sizeof(gSnap.opModeName) - 1);
  } else if (strcmp(key, "eng_status") == 0) {
    gSnap.hasEngStatus = true;
    gSnap.engStatusCode = static_cast<uint16_t>(v.rawU);
    strncpy(gSnap.engStatusName, DseRegisters::engStatusName(gSnap.engStatusCode),
            sizeof(gSnap.engStatusName) - 1);
  } else if (strcmp(key, "eng_rpm") == 0) {
    gSnap.hasEngRpm = true;
    gSnap.engRpm = v.engineering;
  } else if (strcmp(key, "battery_v") == 0) {
    gSnap.hasBatteryV = true;
    gSnap.batteryV = v.engineering;
  } else if (strcmp(key, "gen_freq") == 0) {
    gSnap.hasGenFreq = true;
    gSnap.genFreq = v.engineering;
  } else if (strcmp(key, "gen_van") == 0) {
    gSnap.hasGenVan = true;
    gSnap.genVan = v.engineering;
  } else if (strcmp(key, "gen_vbn") == 0) {
    gSnap.hasGenVbn = true;
    gSnap.genVbn = v.engineering;
  } else if (strcmp(key, "gen_vcn") == 0) {
    gSnap.hasGenVcn = true;
    gSnap.genVcn = v.engineering;
  } else if (strcmp(key, "gen_ia") == 0) {
    gSnap.hasGenIa = true;
    gSnap.genIa = v.engineering;
  } else if (strcmp(key, "gen_ib") == 0) {
    gSnap.hasGenIb = true;
    gSnap.genIb = v.engineering;
  } else if (strcmp(key, "gen_ic") == 0) {
    gSnap.hasGenIc = true;
    gSnap.genIc = v.engineering;
  } else if (strcmp(key, "gen_p_tot") == 0) {
    gSnap.hasGenPTot = true;
    gSnap.genPTotW = v.engineering;
  } else if (strcmp(key, "gen_pct_kw") == 0) {
    gSnap.hasGenPctKw = true;
    gSnap.genPctKw = v.engineering;
  } else if (strcmp(key, "gen_vab") == 0) {
    gSnap.hasGenVab = true;
    gSnap.genVab = v.engineering;
  } else if (strcmp(key, "gen_vbc") == 0) {
    gSnap.hasGenVbc = true;
    gSnap.genVbc = v.engineering;
  } else if (strcmp(key, "gen_vca") == 0) {
    gSnap.hasGenVca = true;
    gSnap.genVca = v.engineering;
  } else if (strcmp(key, "oil_press") == 0) {
    gSnap.hasOil = true;
    gSnap.oilKpa = v.engineering;
  } else if (strcmp(key, "oil_temp") == 0) {
    gSnap.hasOilTemp = true;
    gSnap.oilTempC = v.engineering;
  } else if (strcmp(key, "coolant_temp") == 0) {
    gSnap.hasCoolant = true;
    gSnap.coolantC = v.engineering;
  } else if (strcmp(key, "fuel_level") == 0) {
    gSnap.hasFuel = true;
    gSnap.fuelPct = v.engineering;
  } else if (strcmp(key, "mains_freq") == 0) {
    gSnap.hasMainsHz = true;
    gSnap.mainsHz = v.engineering;
  } else if (strcmp(key, "mains_van") == 0) {
    gSnap.hasMainsVan = true;
    gSnap.mainsVan = v.engineering;
  } else if (strcmp(key, "mains_vbn") == 0) {
    gSnap.hasMainsVbn = true;
    gSnap.mainsVbn = v.engineering;
  } else if (strcmp(key, "mains_vcn") == 0) {
    gSnap.hasMainsVcn = true;
    gSnap.mainsVcn = v.engineering;
  } else if (strcmp(key, "mains_vab") == 0) {
    gSnap.hasMainsVab = true;
    gSnap.mainsVab = v.engineering;
  } else if (strcmp(key, "mains_vbc") == 0) {
    gSnap.hasMainsVbc = true;
    gSnap.mainsVbc = v.engineering;
  } else if (strcmp(key, "mains_vca") == 0) {
    gSnap.hasMainsVca = true;
    gSnap.mainsVca = v.engineering;
  } else if (strcmp(key, "charge_alt_v") == 0) {
    gSnap.hasChargeAlt = true;
    gSnap.chargeAltV = v.engineering;
  } else if (strcmp(key, "gen_pa") == 0) {
    gSnap.hasGenPa = true;
    gSnap.genPaW = v.engineering;
  } else if (strcmp(key, "gen_pb") == 0) {
    gSnap.hasGenPb = true;
    gSnap.genPbW = v.engineering;
  } else if (strcmp(key, "gen_pc") == 0) {
    gSnap.hasGenPc = true;
    gSnap.genPcW = v.engineering;
  } else if (strcmp(key, "gen_sa") == 0) {
    gSnap.hasGenSa = true;
    gSnap.genSaVa = v.engineering;
  } else if (strcmp(key, "gen_sb") == 0) {
    gSnap.hasGenSb = true;
    gSnap.genSbVa = v.engineering;
  } else if (strcmp(key, "gen_sc") == 0) {
    gSnap.hasGenSc = true;
    gSnap.genScVa = v.engineering;
  } else if (strcmp(key, "gen_s_tot") == 0) {
    gSnap.hasGenSTot = true;
    gSnap.genSTotVa = v.engineering;
  } else if (strcmp(key, "gen_qa") == 0) {
    gSnap.hasGenQa = true;
    gSnap.genQaVar = v.engineering;
  } else if (strcmp(key, "gen_qb") == 0) {
    gSnap.hasGenQb = true;
    gSnap.genQbVar = v.engineering;
  } else if (strcmp(key, "gen_qc") == 0) {
    gSnap.hasGenQc = true;
    gSnap.genQcVar = v.engineering;
  } else if (strcmp(key, "gen_q_tot") == 0) {
    gSnap.hasGenQTot = true;
    gSnap.genQTotVar = v.engineering;
  } else if (strcmp(key, "gen_pfa") == 0) {
    gSnap.hasGenPfa = true;
    gSnap.genPfa = v.engineering;
  } else if (strcmp(key, "gen_pfb") == 0) {
    gSnap.hasGenPfb = true;
    gSnap.genPfb = v.engineering;
  } else if (strcmp(key, "gen_pfc") == 0) {
    gSnap.hasGenPfc = true;
    gSnap.genPfc = v.engineering;
  } else if (strcmp(key, "gen_pf") == 0) {
    gSnap.hasGenPf = true;
    gSnap.genPf = v.engineering;
  } else if (strcmp(key, "eng_status_alt") == 0) {
    gSnap.hasEngStatusAlt = true;
    gSnap.engStatusAltCode = static_cast<uint16_t>(v.rawU);
  } else if (strcmp(key, "ctrl_status") == 0) {
    gSnap.hasCtrlStatus = true;
    gSnap.ctrlStatusCode = static_cast<uint16_t>(v.rawU);
  }
}

void ingestBatch(uint16_t batchStart, const uint16_t *values, uint8_t valueCount) {
  if (!values || valueCount == 0) {
    return;
  }
  const uint16_t batchEnd = static_cast<uint16_t>(batchStart + valueCount);
  for (size_t i = 0; i < DseRegisters::count(); i++) {
    const DseRegister &r = DseRegisters::at(i);
    if (r.kind == DseDecodeKind::AlarmPacked ||
        r.kind == DseDecodeKind::Bitmask) {
      continue;
    }
    const uint8_t need = static_cast<uint8_t>(r.width);
    if (r.address < batchStart ||
        static_cast<uint16_t>(r.address + need) > batchEnd) {
      continue;
    }
    const uint8_t off = static_cast<uint8_t>(r.address - batchStart);
    applyDecoded(r.key, DseRegisters::decode(r, values + off, need));
  }
}

bool readBatch(uint8_t slaveId, uint16_t address, uint16_t quantity) {
  ModbusTxnInfo info;
  info.quiet = true;
  info.address = address;
  info.quantity = quantity;
  const ModbusResponse resp =
      modbus.readRegisters(slaveId, 0x03, address, quantity, &info);
  if (resp.result != ModbusResult::Ok) {
    return false;
  }
  ingestBatch(address, resp.values, resp.valueCount);
  return true;
}

void collectAlarms(uint8_t slaveId) {
  gSnap.alarmCount = 0;
  ModbusTxnInfo info;
  info.quiet = true;
  info.address = 39425;
  info.quantity = 21;
  const ModbusResponse resp =
      modbus.readRegisters(slaveId, 0x03, 39425, 21, &info);
  if (resp.result != ModbusResult::Ok) {
    return;
  }
  for (uint8_t i = 0; i < resp.valueCount && gSnap.alarmCount < 16; i++) {
    const uint16_t addr = static_cast<uint16_t>(39425 + i);
    const uint16_t raw = resp.values[i];
    for (uint8_t slot = 0; slot < 4 && gSnap.alarmCount < 16; slot++) {
      const uint8_t nibble =
          static_cast<uint8_t>((raw >> (slot * 4)) & 0xF);
      if (nibble == 0 || nibble == 1 || nibble == 15) {
        continue;
      }
      TelemetryAlarm &a = gSnap.alarms[gSnap.alarmCount++];
      a.address = addr;
      a.slot = slot;
      a.nibble = nibble;
      strncpy(a.name, DseRegisters::alarmSlotName(addr, slot),
              sizeof(a.name) - 1);
      strncpy(a.state, DseRegisters::alarmStateName(nibble),
              sizeof(a.state) - 1);
    }
  }
}

void updateHeadline() {
  if (gSnap.alarmCount > 0) {
    strncpy(gSnap.headline, "Alarm", sizeof(gSnap.headline) - 1);
    return;
  }
  if (gSnap.hasEngStatus) {
    if (gSnap.engStatusCode == 3) {
      strncpy(gSnap.headline, "Running", sizeof(gSnap.headline) - 1);
      return;
    }
    if (gSnap.engStatusCode == 0 || gSnap.engStatusCode == 5) {
      strncpy(gSnap.headline, "Stopped", sizeof(gSnap.headline) - 1);
      return;
    }
    strncpy(gSnap.headline, gSnap.engStatusName, sizeof(gSnap.headline) - 1);
    return;
  }
  if (gSnap.ok) {
    strncpy(gSnap.headline, "Online", sizeof(gSnap.headline) - 1);
  } else {
    strncpy(gSnap.headline, "Offline", sizeof(gSnap.headline) - 1);
  }
}

void appendFloat(char *&p, char *end, const char *key, bool has, float v,
                 int decimals) {
  if (p >= end) {
    return;
  }
  if (!has) {
    p += snprintf(p, static_cast<size_t>(end - p), "\"%s\":null,", key);
    return;
  }
  char num[24];
  dtostrf(static_cast<double>(v), 0, decimals, num);
  p += snprintf(p, static_cast<size_t>(end - p), "\"%s\":%s,", key, num);
}

void refreshNow() {
  if (gSnap.busy) {
    return;
  }
  gSnap.busy = true;

  const uint8_t slaveId = devices.selected().slaveId;
  gSnap.slaveId = slaveId;
  gSnap.uptimeMs = millis();

  uint8_t ok = 0;
  uint8_t fail = 0;

  auto batch = [&](uint16_t addr, uint16_t qty) {
    yieldIdle();
    if (readBatch(slaveId, addr, qty)) {
      ok++;
    } else {
      fail++;
    }
    const uint32_t until = millis() + 20;
    while (static_cast<int32_t>(until - millis()) > 0) {
      yieldIdle();
      delay(1);
    }
  };

  // Same ranges as CLI iterate basic + power + alarms
  batch(768, 8);
  batch(1024, 32);
  batch(1056, 16);
  batch(1408, 16);
  batch(1536, 23);

  collectAlarms(slaveId);

  gSnap.ok = (ok > 0 && fail == 0);
  if (fail == 0 && ok > 0) {
    strncpy(gSnap.link, "ok", sizeof(gSnap.link) - 1);
    setErr("");
  } else if (ok > 0) {
    strncpy(gSnap.link, "degraded", sizeof(gSnap.link) - 1);
    setErr("partial Modbus batch failure");
  } else {
    strncpy(gSnap.link, "fail", sizeof(gSnap.link) - 1);
    setErr("Modbus refresh failed");
  }

  updateHeadline();
  gSnap.refreshedAtMs = millis();
  gSnap.busy = false;
}

}  // namespace

namespace Telemetry {

void begin() {
  gSnap = TelemetrySnapshot{};
  strncpy(gSnap.wifiMode, "off", sizeof(gSnap.wifiMode) - 1);
  strncpy(gSnap.link, "unknown", sizeof(gSnap.link) - 1);
  strncpy(gSnap.headline, "Offline", sizeof(gSnap.headline) - 1);
  gLastRefreshMs = 0;
}

void setWifiInfo(const char *mode, const char *ip, int8_t rssi) {
  if (mode) {
    strncpy(gSnap.wifiMode, mode, sizeof(gSnap.wifiMode) - 1);
  }
  if (ip) {
    strncpy(gSnap.wifiIp, ip, sizeof(gSnap.wifiIp) - 1);
  }
  gSnap.rssi = rssi;
}

void setAutoRefresh(bool enabled) { gAutoRefresh = enabled; }

bool autoRefresh() { return gAutoRefresh; }

void setIdleHook(void (*fn)()) { gIdleHook = fn; }

void poll() {
  const uint32_t now = millis();
  gSnap.ageMs =
      gSnap.refreshedAtMs ? (now - gSnap.refreshedAtMs) : 0;
  gSnap.uptimeMs = now;
  // DSE map reads against the ET1641 block the CLI for seconds and collide
  // with bench probes. Auto-refresh stays off until explicitly enabled,
  // and only runs against the DSE profile.
  if (!gAutoRefresh || gSnap.busy) {
    return;
  }
  if (strcmp(devices.selected().id, "dse7320") != 0) {
    return;
  }
  if (gLastRefreshMs != 0 && (now - gLastRefreshMs) < kRefreshIntervalMs) {
    return;
  }
  gLastRefreshMs = now;
  refreshNow();
  gSnap.ageMs = 0;
}

const TelemetrySnapshot &get() {
  gSnap.ageMs =
      gSnap.refreshedAtMs ? (millis() - gSnap.refreshedAtMs) : 0;
  gSnap.uptimeMs = millis();
  return gSnap;
}

size_t writeStatusJson(char *buf, size_t bufSize) {
  if (!buf || bufSize < 64) {
    return 0;
  }
  const TelemetrySnapshot &s = get();
  char *p = buf;
  char *end = buf + bufSize;

  p += snprintf(p, static_cast<size_t>(end - p),
                "{\"ok\":%s,\"ageMs\":%lu,\"slaveId\":%u,\"rssi\":%d,"
                "\"wifiMode\":\"%s\",\"wifiIp\":\"%s\",\"uptimeMs\":%lu,"
                "\"lastError\":\"%s\",\"link\":\"%s\",\"headline\":\"%s\",",
                s.ok ? "true" : "false", static_cast<unsigned long>(s.ageMs),
                s.slaveId, static_cast<int>(s.rssi), s.wifiMode, s.wifiIp,
                static_cast<unsigned long>(s.uptimeMs), s.lastError, s.link,
                s.headline);

  if (s.hasOpMode) {
    p += snprintf(p, static_cast<size_t>(end - p),
                  "\"opMode\":{\"code\":%u,\"name\":\"%s\"},", s.opModeCode,
                  s.opModeName);
  } else {
    p += snprintf(p, static_cast<size_t>(end - p), "\"opMode\":null,");
  }
  if (s.hasEngStatus) {
    p += snprintf(p, static_cast<size_t>(end - p),
                  "\"engStatus\":{\"code\":%u,\"name\":\"%s\"},",
                  s.engStatusCode, s.engStatusName);
  } else {
    p += snprintf(p, static_cast<size_t>(end - p), "\"engStatus\":null,");
  }

  appendFloat(p, end, "engRpm", s.hasEngRpm, s.engRpm, 0);
  appendFloat(p, end, "batteryV", s.hasBatteryV, s.batteryV, 1);
  appendFloat(p, end, "genFreq", s.hasGenFreq, s.genFreq, 1);
  appendFloat(p, end, "genVan", s.hasGenVan, s.genVan, 1);
  appendFloat(p, end, "genVbn", s.hasGenVbn, s.genVbn, 1);
  appendFloat(p, end, "genVcn", s.hasGenVcn, s.genVcn, 1);
  appendFloat(p, end, "genIa", s.hasGenIa, s.genIa, 1);
  appendFloat(p, end, "genIb", s.hasGenIb, s.genIb, 1);
  appendFloat(p, end, "genIc", s.hasGenIc, s.genIc, 1);
  appendFloat(p, end, "genPTotW", s.hasGenPTot, s.genPTotW, 0);
  appendFloat(p, end, "genPTotKw", s.hasGenPTot, s.genPTotW / 1000.0f, 2);
  appendFloat(p, end, "genPctKw", s.hasGenPctKw, s.genPctKw, 1);

  p += snprintf(p, static_cast<size_t>(end - p), "\"alarms\":[");
  for (uint8_t i = 0; i < s.alarmCount; i++) {
    const TelemetryAlarm &a = s.alarms[i];
    p += snprintf(p, static_cast<size_t>(end - p),
                  "%s{\"addr\":%u,\"slot\":%u,\"nibble\":%u,\"name\":\"%s\","
                  "\"state\":\"%s\"}",
                  i ? "," : "", a.address, a.slot, a.nibble, a.name, a.state);
  }
  p += snprintf(p, static_cast<size_t>(end - p), "]}");

  if (p >= end) {
    buf[bufSize - 1] = '\0';
    return bufSize - 1;
  }
  return static_cast<size_t>(p - buf);
}

size_t writeHealthJson(char *buf, size_t bufSize) {
  if (!buf || bufSize < 32) {
    return 0;
  }
  const TelemetrySnapshot &s = get();
#if defined(ARDUINO_ARCH_ESP32)
  const int n = snprintf(
      buf, bufSize,
      "{\"wifi\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"modbusLastOk\":%s,"
      "\"link\":\"%s\",\"uptimeMs\":%lu,\"ageMs\":%lu,\"fw\":\"%s\",\"git\":\"%s\"}",
      s.wifiMode, s.wifiIp, static_cast<int>(s.rssi),
      s.ok ? "true" : "false", s.link,
      static_cast<unsigned long>(s.uptimeMs),
      static_cast<unsigned long>(s.ageMs), FW_VERSION, GIT_HASH);
#else
  const int n = snprintf(
      buf, bufSize,
      "{\"wifi\":\"%s\",\"ip\":\"%s\",\"rssi\":%d,\"modbusLastOk\":%s,"
      "\"link\":\"%s\",\"uptimeMs\":%lu,\"ageMs\":%lu}",
      s.wifiMode, s.wifiIp, static_cast<int>(s.rssi),
      s.ok ? "true" : "false", s.link,
      static_cast<unsigned long>(s.uptimeMs),
      static_cast<unsigned long>(s.ageMs));
#endif
  return n > 0 ? static_cast<size_t>(n) : 0;
}

}  // namespace Telemetry
