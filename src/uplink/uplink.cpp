#include "uplink/uplink.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_system.h>
#include <math.h>
#include <mbedtls/gcm.h>
#include <stdio.h>
#include <string.h>

#include "config/settings.h"
#include "ota/build_info.h"
#include "telemetry/snapshot.h"
#include "uplink/gts_roots.h"

namespace {

constexpr size_t kMaxSamples = 8;
constexpr size_t kPlainMax = 6144;
constexpr uint32_t kHeapFloor = 24576;
constexpr uint32_t kConnectMs = 3000;
constexpr uint32_t kBackoffCapS = 900;

struct Sample {
  uint32_t tMs = 0;
  uint16_t ageS = 0;
  uint8_t link = 0;
  uint64_t flags = 0;
  uint8_t mode = 0;
  uint8_t alarms = 0;
  int16_t rpm = 0;
  int16_t batt10 = 0;
  int16_t hz10 = 0;
  int16_t kw100 = 0;
  int16_t van10 = 0;
  int16_t vbn10 = 0;
  int16_t vcn10 = 0;
  int16_t vab10 = 0;
  int16_t vbc10 = 0;
  int16_t vca10 = 0;
  int16_t ia10 = 0;
  int16_t ib10 = 0;
  int16_t ic10 = 0;
  int16_t pct100 = 0;
  int16_t oil = 0;
  int16_t oilc = 0;
  int16_t cool = 0;
  int16_t fuel = 0;
  int16_t mhz10 = 0;
  int16_t mvan10 = 0;
  int16_t mvbn10 = 0;
  int16_t mvcn10 = 0;
  int16_t mvab10 = 0;
  int16_t mvbc10 = 0;
  int16_t mvca10 = 0;
  uint16_t eng = 0;
  int16_t chg10 = 0;
  int32_t pa = 0;
  int32_t pb = 0;
  int32_t pc = 0;
  int32_t sa = 0;
  int32_t sb = 0;
  int32_t sc = 0;
  int32_t stot = 0;
  int32_t qa = 0;
  int32_t qb = 0;
  int32_t qc = 0;
  int32_t qtot = 0;
  int16_t pfa = 0;
  int16_t pfb = 0;
  int16_t pfc = 0;
  int16_t pf = 0;
  uint16_t eng2 = 0;
  uint16_t ctrl = 0;
};

constexpr uint64_t kHasRpm = 1ull;
constexpr uint64_t kHasBatt = 2ull;
constexpr uint64_t kHasHz = 4ull;
constexpr uint64_t kHasKw = 8ull;
constexpr uint64_t kHasVan = 16ull;
constexpr uint64_t kHasMode = 32ull;
constexpr uint64_t kHasVbn = 1ull << 6;
constexpr uint64_t kHasVcn = 1ull << 7;
constexpr uint64_t kHasVab = 1ull << 8;
constexpr uint64_t kHasVbc = 1ull << 9;
constexpr uint64_t kHasVca = 1ull << 10;
constexpr uint64_t kHasIa = 1ull << 11;
constexpr uint64_t kHasIb = 1ull << 12;
constexpr uint64_t kHasIc = 1ull << 13;
constexpr uint64_t kHasPct = 1ull << 14;
constexpr uint64_t kHasOil = 1ull << 15;
constexpr uint64_t kHasOilC = 1ull << 16;
constexpr uint64_t kHasCool = 1ull << 17;
constexpr uint64_t kHasFuel = 1ull << 18;
constexpr uint64_t kHasMhz = 1ull << 19;
constexpr uint64_t kHasMvan = 1ull << 20;
constexpr uint64_t kHasMvbn = 1ull << 21;
constexpr uint64_t kHasMvcn = 1ull << 22;
constexpr uint64_t kHasMvab = 1ull << 23;
constexpr uint64_t kHasMvbc = 1ull << 24;
constexpr uint64_t kHasMvca = 1ull << 25;
constexpr uint64_t kHasEng = 1ull << 26;
constexpr uint64_t kHasChg = 1ull << 27;
constexpr uint64_t kHasPa = 1ull << 28;
constexpr uint64_t kHasPb = 1ull << 29;
constexpr uint64_t kHasPc = 1ull << 30;
constexpr uint64_t kHasSa = 1ull << 31;
constexpr uint64_t kHasSb = 1ull << 32;
constexpr uint64_t kHasSc = 1ull << 33;
constexpr uint64_t kHasStot = 1ull << 34;
constexpr uint64_t kHasQa = 1ull << 35;
constexpr uint64_t kHasQb = 1ull << 36;
constexpr uint64_t kHasQc = 1ull << 37;
constexpr uint64_t kHasQtot = 1ull << 38;
constexpr uint64_t kHasPfa = 1ull << 39;
constexpr uint64_t kHasPfb = 1ull << 40;
constexpr uint64_t kHasPfc = 1ull << 41;
constexpr uint64_t kHasPf = 1ull << 42;
constexpr uint64_t kHasEng2 = 1ull << 43;
constexpr uint64_t kHasCtrl = 1ull << 44;

Sample gSamples[kMaxSamples];
uint8_t gCount = 0;
uint32_t gBoot = 0;
uint32_t gEpoch = 0;
uint32_t gSeq = 0;
uint32_t gSent = 0;
uint32_t gFailed = 0;
uint32_t gDropped = 0;
uint32_t gOversized = 0;
uint32_t gLastSampleMs = 0;
uint32_t gNextTryMs = 0;
uint32_t gBackoffS = 5;
bool gForce = false;
char gLast[40] = "idle";

uint8_t linkCode(const char *link) {
  if (!link) return 0;
  if (strcmp(link, "ok") == 0) return 1;
  if (strcmp(link, "degraded") == 0) return 2;
  if (strcmp(link, "fail") == 0) return 3;
  return 0;
}

const char *linkName(uint8_t code) {
  if (code == 1) return "ok";
  if (code == 2) return "degraded";
  if (code == 3) return "fail";
  return "unknown";
}

bool storeI16(float engineering, float scale, int16_t *out) {
  if (!isfinite(engineering)) {
    return false;
  }
  const float scaled = engineering * scale;
  if (scaled > 32767.f || scaled < -32768.f) {
    return false;
  }
  *out = static_cast<int16_t>(lroundf(scaled));
  return true;
}

bool storeI32(float engineering, float scale, int32_t *out) {
  if (!isfinite(engineering)) {
    return false;
  }
  const double scaled = static_cast<double>(engineering) * static_cast<double>(scale);
  if (scaled > 2147483647.0 || scaled < -2147483648.0) {
    return false;
  }
  *out = static_cast<int32_t>(lround(scaled));
  return true;
}

uint32_t loadEpoch() {
  Preferences prefs;
  if (!prefs.begin("dse-uplink", false)) {
    return 0;
  }
  uint32_t epoch = prefs.getUInt("epoch", 0);
  if (epoch < 0xffffffffu) {
    epoch += 1;
  }
  if (prefs.putUInt("epoch", epoch) == 0) {
    prefs.end();
    return 0;
  }
  prefs.end();
  return epoch;
}

void setExtra32(Sample &s, uint64_t bit, bool present, float engineering, float scale, int32_t *out) {
  if (!present) {
    return;
  }
  if (storeI32(engineering, scale, out)) {
    s.flags |= bit;
  }
}

void setExtra(Sample &s, uint64_t bit, bool present, float engineering, float scale, int16_t *out) {
  if (!present) {
    return;
  }
  if (storeI16(engineering, scale, out)) {
    s.flags |= bit;
  }
}

void capture(const TelemetrySnapshot &snap) {
  if (gCount == kMaxSamples) {
    memmove(&gSamples[0], &gSamples[1], (kMaxSamples - 1) * sizeof(Sample));
    gCount = kMaxSamples - 1;
    gDropped++;
  }
  Sample &s = gSamples[gCount++];
  s = Sample{};
  s.tMs = snap.uptimeMs;
  s.ageS = snap.ageMs > 65535000ul ? 65535 : static_cast<uint16_t>(snap.ageMs / 1000ul);
  s.link = linkCode(snap.link);
  s.alarms = snap.alarmCount;
  if (snap.hasOpMode) {
    s.flags |= kHasMode;
    s.mode = static_cast<uint8_t>(snap.opModeCode > 255 ? 255 : snap.opModeCode);
  }
  if (snap.hasEngRpm) {
    s.flags |= kHasRpm;
    s.rpm = snap.engRpm > 32000.f ? 32000 : static_cast<int16_t>(snap.engRpm);
  }
  if (snap.hasBatteryV) {
    s.flags |= kHasBatt;
    s.batt10 = static_cast<int16_t>(snap.batteryV * 10.f);
  }
  if (snap.hasGenFreq) {
    s.flags |= kHasHz;
    s.hz10 = static_cast<int16_t>(snap.genFreq * 10.f);
  }
  if (snap.hasGenPTot) {
    s.flags |= kHasKw;
    s.kw100 = static_cast<int16_t>((snap.genPTotW / 1000.f) * 100.f);
  }
  if (snap.hasGenVan) {
    s.flags |= kHasVan;
    if (!storeI16(snap.genVan, 10.f, &s.van10)) {
      s.flags &= ~kHasVan;
    }
  }
  setExtra(s, kHasVbn, snap.hasGenVbn, snap.genVbn, 10.f, &s.vbn10);
  setExtra(s, kHasVcn, snap.hasGenVcn, snap.genVcn, 10.f, &s.vcn10);
  setExtra(s, kHasVab, snap.hasGenVab, snap.genVab, 10.f, &s.vab10);
  setExtra(s, kHasVbc, snap.hasGenVbc, snap.genVbc, 10.f, &s.vbc10);
  setExtra(s, kHasVca, snap.hasGenVca, snap.genVca, 10.f, &s.vca10);
  setExtra(s, kHasIa, snap.hasGenIa, snap.genIa, 10.f, &s.ia10);
  setExtra(s, kHasIb, snap.hasGenIb, snap.genIb, 10.f, &s.ib10);
  setExtra(s, kHasIc, snap.hasGenIc, snap.genIc, 10.f, &s.ic10);
  setExtra(s, kHasPct, snap.hasGenPctKw, snap.genPctKw, 100.f, &s.pct100);
  setExtra(s, kHasOil, snap.hasOil, snap.oilKpa, 1.f, &s.oil);
  setExtra(s, kHasOilC, snap.hasOilTemp, snap.oilTempC, 1.f, &s.oilc);
  setExtra(s, kHasCool, snap.hasCoolant, snap.coolantC, 1.f, &s.cool);
  setExtra(s, kHasFuel, snap.hasFuel, snap.fuelPct, 1.f, &s.fuel);
  setExtra(s, kHasMhz, snap.hasMainsHz, snap.mainsHz, 10.f, &s.mhz10);
  setExtra(s, kHasMvan, snap.hasMainsVan, snap.mainsVan, 10.f, &s.mvan10);
  setExtra(s, kHasMvbn, snap.hasMainsVbn, snap.mainsVbn, 10.f, &s.mvbn10);
  setExtra(s, kHasMvcn, snap.hasMainsVcn, snap.mainsVcn, 10.f, &s.mvcn10);
  setExtra(s, kHasMvab, snap.hasMainsVab, snap.mainsVab, 10.f, &s.mvab10);
  setExtra(s, kHasMvbc, snap.hasMainsVbc, snap.mainsVbc, 10.f, &s.mvbc10);
  setExtra(s, kHasMvca, snap.hasMainsVca, snap.mainsVca, 10.f, &s.mvca10);
  if (snap.hasEngStatus) {
    s.flags |= kHasEng;
    s.eng = snap.engStatusCode;
  }
  setExtra(s, kHasChg, snap.hasChargeAlt, snap.chargeAltV, 10.f, &s.chg10);
  setExtra32(s, kHasPa, snap.hasGenPa, snap.genPaW, 1.f, &s.pa);
  setExtra32(s, kHasPb, snap.hasGenPb, snap.genPbW, 1.f, &s.pb);
  setExtra32(s, kHasPc, snap.hasGenPc, snap.genPcW, 1.f, &s.pc);
  setExtra32(s, kHasSa, snap.hasGenSa, snap.genSaVa, 1.f, &s.sa);
  setExtra32(s, kHasSb, snap.hasGenSb, snap.genSbVa, 1.f, &s.sb);
  setExtra32(s, kHasSc, snap.hasGenSc, snap.genScVa, 1.f, &s.sc);
  setExtra32(s, kHasStot, snap.hasGenSTot, snap.genSTotVa, 1.f, &s.stot);
  setExtra32(s, kHasQa, snap.hasGenQa, snap.genQaVar, 1.f, &s.qa);
  setExtra32(s, kHasQb, snap.hasGenQb, snap.genQbVar, 1.f, &s.qb);
  setExtra32(s, kHasQc, snap.hasGenQc, snap.genQcVar, 1.f, &s.qc);
  setExtra32(s, kHasQtot, snap.hasGenQTot, snap.genQTotVar, 1.f, &s.qtot);
  setExtra(s, kHasPfa, snap.hasGenPfa, snap.genPfa, 100.f, &s.pfa);
  setExtra(s, kHasPfb, snap.hasGenPfb, snap.genPfb, 100.f, &s.pfb);
  setExtra(s, kHasPfc, snap.hasGenPfc, snap.genPfc, 100.f, &s.pfc);
  setExtra(s, kHasPf, snap.hasGenPf, snap.genPf, 100.f, &s.pf);
  if (snap.hasEngStatusAlt) {
    s.flags |= kHasEng2;
    s.eng2 = snap.engStatusAltCode;
  }
  if (snap.hasCtrlStatus) {
    s.flags |= kHasCtrl;
    s.ctrl = snap.ctrlStatusCode;
  }
}

int appendOpt(char *p, char *end, const char *key, bool present, long value) {
  if (present) {
    return snprintf(p, static_cast<size_t>(end - p), "\"%s\":%ld,", key, value);
  }
  return snprintf(p, static_cast<size_t>(end - p), "\"%s\":null,", key);
}

size_t buildPlain(char *buf, size_t cap, uint32_t seq, uint8_t count) {
  char *p = buf;
  char *end = buf + cap;
  p += snprintf(p, static_cast<size_t>(end - p),
                "{\"boot\":%lu,\"seq\":%lu,\"n\":%u,\"s\":[",
                static_cast<unsigned long>(gBoot),
                static_cast<unsigned long>(seq), count);
  for (uint8_t i = 0; i < count && p < end; i++) {
    const Sample &s = gSamples[i];
    p += snprintf(p, static_cast<size_t>(end - p),
                  "%s{\"t\":%lu,\"age\":%u,\"link\":\"%s\",\"alm\":%u,",
                  i ? "," : "", static_cast<unsigned long>(s.tMs), s.ageS,
                  linkName(s.link), s.alarms);
    p += appendOpt(p, end, "mode", s.flags & kHasMode, s.mode);
    p += appendOpt(p, end, "rpm", s.flags & kHasRpm, s.rpm);
    p += appendOpt(p, end, "batt10", s.flags & kHasBatt, s.batt10);
    p += appendOpt(p, end, "hz10", s.flags & kHasHz, s.hz10);
    p += appendOpt(p, end, "kw100", s.flags & kHasKw, s.kw100);
    p += appendOpt(p, end, "van10", s.flags & kHasVan, s.van10);
    p += appendOpt(p, end, "vbn10", s.flags & kHasVbn, s.vbn10);
    p += appendOpt(p, end, "vcn10", s.flags & kHasVcn, s.vcn10);
    p += appendOpt(p, end, "vab10", s.flags & kHasVab, s.vab10);
    p += appendOpt(p, end, "vbc10", s.flags & kHasVbc, s.vbc10);
    p += appendOpt(p, end, "vca10", s.flags & kHasVca, s.vca10);
    p += appendOpt(p, end, "ia10", s.flags & kHasIa, s.ia10);
    p += appendOpt(p, end, "ib10", s.flags & kHasIb, s.ib10);
    p += appendOpt(p, end, "ic10", s.flags & kHasIc, s.ic10);
    p += appendOpt(p, end, "pct100", s.flags & kHasPct, s.pct100);
    p += appendOpt(p, end, "oil", s.flags & kHasOil, s.oil);
    p += appendOpt(p, end, "oilc", s.flags & kHasOilC, s.oilc);
    p += appendOpt(p, end, "cool", s.flags & kHasCool, s.cool);
    p += appendOpt(p, end, "fuel", s.flags & kHasFuel, s.fuel);
    p += appendOpt(p, end, "mhz10", s.flags & kHasMhz, s.mhz10);
    p += appendOpt(p, end, "mvan10", s.flags & kHasMvan, s.mvan10);
    p += appendOpt(p, end, "mvbn10", s.flags & kHasMvbn, s.mvbn10);
    p += appendOpt(p, end, "mvcn10", s.flags & kHasMvcn, s.mvcn10);
    p += appendOpt(p, end, "mvab10", s.flags & kHasMvab, s.mvab10);
    p += appendOpt(p, end, "mvbc10", s.flags & kHasMvbc, s.mvbc10);
    p += appendOpt(p, end, "mvca10", s.flags & kHasMvca, s.mvca10);
    p += appendOpt(p, end, "eng", s.flags & kHasEng, s.eng);
    p += appendOpt(p, end, "chg10", s.flags & kHasChg, s.chg10);
    p += appendOpt(p, end, "pa", s.flags & kHasPa, s.pa);
    p += appendOpt(p, end, "pb", s.flags & kHasPb, s.pb);
    p += appendOpt(p, end, "pc", s.flags & kHasPc, s.pc);
    p += appendOpt(p, end, "sa", s.flags & kHasSa, s.sa);
    p += appendOpt(p, end, "sb", s.flags & kHasSb, s.sb);
    p += appendOpt(p, end, "sc", s.flags & kHasSc, s.sc);
    p += appendOpt(p, end, "stot", s.flags & kHasStot, s.stot);
    p += appendOpt(p, end, "qa", s.flags & kHasQa, s.qa);
    p += appendOpt(p, end, "qb", s.flags & kHasQb, s.qb);
    p += appendOpt(p, end, "qc", s.flags & kHasQc, s.qc);
    p += appendOpt(p, end, "qtot", s.flags & kHasQtot, s.qtot);
    p += appendOpt(p, end, "pfa", s.flags & kHasPfa, s.pfa);
    p += appendOpt(p, end, "pfb", s.flags & kHasPfb, s.pfb);
    p += appendOpt(p, end, "pfc", s.flags & kHasPfc, s.pfc);
    p += appendOpt(p, end, "pf", s.flags & kHasPf, s.pf);
    p += appendOpt(p, end, "eng2", s.flags & kHasEng2, s.eng2);
    p += appendOpt(p, end, "ctrl", s.flags & kHasCtrl, s.ctrl);
    if (p > buf && p[-1] == ',') {
      p--;
    }
    p += snprintf(p, static_cast<size_t>(end - p), "}");
  }
  p += snprintf(p, static_cast<size_t>(end - p), "]}");
  if (p >= end) {
    return 0;
  }
  return static_cast<size_t>(p - buf);
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

size_t buildCloud(char *buf, size_t cap, uint32_t seq, uint8_t count) {
  char id[13];
  identityHex(id);
  char *p = buf;
  char *end = buf + cap;
  p += snprintf(p, static_cast<size_t>(end - p),
                "{\"v\":2,\"identity\":\"%s\",\"boot\":%lu,\"epoch\":%lu,\"seq\":%lu,\"samples\":[",
                id, static_cast<unsigned long>(gBoot), static_cast<unsigned long>(gEpoch),
                static_cast<unsigned long>(seq));
  for (uint8_t i = 0; i < count && p < end; i++) {
    const Sample &s = gSamples[i];
    p += snprintf(p, static_cast<size_t>(end - p),
                  "%s{\"t\":%lu,\"age\":%u,\"link\":\"%s\",\"alm\":%u,",
                  i ? "," : "", static_cast<unsigned long>(s.tMs), s.ageS,
                  linkName(s.link), s.alarms);
    p += appendOpt(p, end, "mode", s.flags & kHasMode, s.mode);
    p += appendOpt(p, end, "rpm", s.flags & kHasRpm, s.rpm);
    p += appendOpt(p, end, "batt10", s.flags & kHasBatt, s.batt10);
    p += appendOpt(p, end, "hz10", s.flags & kHasHz, s.hz10);
    p += appendOpt(p, end, "kw100", s.flags & kHasKw, s.kw100);
    p += appendOpt(p, end, "van10", s.flags & kHasVan, s.van10);
    p += appendOpt(p, end, "vbn10", s.flags & kHasVbn, s.vbn10);
    p += appendOpt(p, end, "vcn10", s.flags & kHasVcn, s.vcn10);
    p += appendOpt(p, end, "vab10", s.flags & kHasVab, s.vab10);
    p += appendOpt(p, end, "vbc10", s.flags & kHasVbc, s.vbc10);
    p += appendOpt(p, end, "vca10", s.flags & kHasVca, s.vca10);
    p += appendOpt(p, end, "ia10", s.flags & kHasIa, s.ia10);
    p += appendOpt(p, end, "ib10", s.flags & kHasIb, s.ib10);
    p += appendOpt(p, end, "ic10", s.flags & kHasIc, s.ic10);
    p += appendOpt(p, end, "pct100", s.flags & kHasPct, s.pct100);
    p += appendOpt(p, end, "oil", s.flags & kHasOil, s.oil);
    p += appendOpt(p, end, "oilc", s.flags & kHasOilC, s.oilc);
    p += appendOpt(p, end, "cool", s.flags & kHasCool, s.cool);
    p += appendOpt(p, end, "fuel", s.flags & kHasFuel, s.fuel);
    p += appendOpt(p, end, "mhz10", s.flags & kHasMhz, s.mhz10);
    p += appendOpt(p, end, "mvan10", s.flags & kHasMvan, s.mvan10);
    p += appendOpt(p, end, "mvbn10", s.flags & kHasMvbn, s.mvbn10);
    p += appendOpt(p, end, "mvcn10", s.flags & kHasMvcn, s.mvcn10);
    p += appendOpt(p, end, "mvab10", s.flags & kHasMvab, s.mvab10);
    p += appendOpt(p, end, "mvbc10", s.flags & kHasMvbc, s.mvbc10);
    p += appendOpt(p, end, "mvca10", s.flags & kHasMvca, s.mvca10);
    p += appendOpt(p, end, "eng", s.flags & kHasEng, s.eng);
    p += appendOpt(p, end, "chg10", s.flags & kHasChg, s.chg10);
    p += appendOpt(p, end, "pa", s.flags & kHasPa, s.pa);
    p += appendOpt(p, end, "pb", s.flags & kHasPb, s.pb);
    p += appendOpt(p, end, "pc", s.flags & kHasPc, s.pc);
    p += appendOpt(p, end, "sa", s.flags & kHasSa, s.sa);
    p += appendOpt(p, end, "sb", s.flags & kHasSb, s.sb);
    p += appendOpt(p, end, "sc", s.flags & kHasSc, s.sc);
    p += appendOpt(p, end, "stot", s.flags & kHasStot, s.stot);
    p += appendOpt(p, end, "qa", s.flags & kHasQa, s.qa);
    p += appendOpt(p, end, "qb", s.flags & kHasQb, s.qb);
    p += appendOpt(p, end, "qc", s.flags & kHasQc, s.qc);
    p += appendOpt(p, end, "qtot", s.flags & kHasQtot, s.qtot);
    p += appendOpt(p, end, "pfa", s.flags & kHasPfa, s.pfa);
    p += appendOpt(p, end, "pfb", s.flags & kHasPfb, s.pfb);
    p += appendOpt(p, end, "pfc", s.flags & kHasPfc, s.pfc);
    p += appendOpt(p, end, "pf", s.flags & kHasPf, s.pf);
    p += appendOpt(p, end, "eng2", s.flags & kHasEng2, s.eng2);
    p += appendOpt(p, end, "ctrl", s.flags & kHasCtrl, s.ctrl);
    if (p > buf && p[-1] == ',') {
      p--;
    }
    p += snprintf(p, static_cast<size_t>(end - p), "}");
  }
  p += snprintf(p, static_cast<size_t>(end - p), "]}");
  if (p >= end) {
    return 0;
  }
  return static_cast<size_t>(p - buf);
}

bool seal(const uint8_t *plain, size_t plainLen, uint8_t count, uint32_t seq,
          uint8_t *packet, size_t *packetLen) {
  const GatewayConfig &cfg = Settings::get();
  if (plainLen > kPlainMax || plainLen > 65535) {
    return false;
  }
  uint8_t mac[6] = {};
  const uint64_t efuse = ESP.getEfuseMac();
  memcpy(mac, &efuse, 6);
  packet[0] = 'N';
  packet[1] = 'C';
  packet[2] = 'U';
  packet[3] = '1';
  packet[4] = 1;
  packet[5] = count;
  memcpy(packet + 6, mac, 6);
  memcpy(packet + 12, &seq, 4);
  esp_fill_random(packet + 16, 12);
  packet[28] = static_cast<uint8_t>(plainLen & 0xff);
  packet[29] = static_cast<uint8_t>((plainLen >> 8) & 0xff);

  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  const int keyRc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, cfg.uplinkKey,
                                       128);
  uint8_t tag[16];
  const int cryptRc =
      keyRc == 0 ? mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plainLen,
                                            packet + 16, 12, packet, 30, plain,
                                            packet + 30, 16, tag)
                 : -1;
  mbedtls_gcm_free(&gcm);
  if (cryptRc != 0) {
    return false;
  }
  memcpy(packet + 30 + plainLen, tag, 16);
  *packetLen = 30 + plainLen + 16;
  return true;
}

void noteFail(const char *why) {
  gFailed++;
  snprintf(gLast, sizeof(gLast), "%s", why);
  gNextTryMs = millis() + gBackoffS * 1000ul;
  const uint32_t step =
      gBackoffS > (kBackoffCapS / 3) ? kBackoffCapS : gBackoffS * 3;
  gBackoffS = step > kBackoffCapS ? kBackoffCapS : step;
}

struct HttpReply {
  int code;
  char why[24];
};

void setWhy(HttpReply *reply, const char *why) {
  snprintf(reply->why, sizeof(reply->why), "%s", why);
}

HttpReply httpPost(const uint8_t *packet, size_t packetLen) {
  HttpReply reply{};
  reply.code = 0;
  const GatewayConfig &cfg = Settings::get();
  const bool cloud = cfg.uplinkBearer[0] != '\0';
  if (cloud && ESP.getFreeHeap() < 50000) {
    setWhy(&reply, "low heap");
    return reply;
  }
  WiFiClient plain;
  WiFiClientSecure secure;
  Client *client = &plain;
  bool connected = false;
  if (cloud) {
    secure.setCACert(GTS_ROOTS);
    secure.setTimeout(8);
    connected = secure.connect(cfg.uplinkHost, cfg.uplinkPort, 8000);
    client = &secure;
  } else {
    plain.setTimeout(3);
    connected = plain.connect(cfg.uplinkHost, cfg.uplinkPort, kConnectMs);
    client = &plain;
  }
  if (!connected) {
    setWhy(&reply, "connect failed");
    client->stop();
    return reply;
  }
  char head[360];
  const int headN = snprintf(
      head, sizeof(head),
      "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: %s\r\n"
      "%s%s%s"
      "Content-Length: %u\r\nConnection: close\r\n\r\n",
      cfg.uplinkPath, cfg.uplinkHost,
      cloud ? "application/json" : "application/octet-stream",
      cloud ? "Authorization: Bearer " : "",
      cloud ? cfg.uplinkBearer : "",
      cloud ? "\r\n" : "",
      static_cast<unsigned>(packetLen));
  if (headN < 0 || headN >= static_cast<int>(sizeof(head)) ||
      client->write(reinterpret_cast<const uint8_t *>(head), static_cast<size_t>(headN)) !=
          static_cast<size_t>(headN) ||
      client->write(packet, packetLen) != packetLen) {
    setWhy(&reply, "send failed");
    client->stop();
    return reply;
  }
  char line[64];
  const int got = client->readBytesUntil('\n', line, sizeof(line) - 1);
  client->stop();
  if (got <= 0) {
    setWhy(&reply, "no response");
    return reply;
  }
  line[got] = '\0';
  int code = 0;
  if (sscanf(line, "HTTP/%*s %d", &code) != 1) {
    setWhy(&reply, "bad status");
    return reply;
  }
  reply.code = code;
  snprintf(reply.why, sizeof(reply.why), "http %d", code);
  return reply;
}

bool postBatch(const uint8_t *packet, size_t packetLen, uint8_t sent) {
  if (ESP.getFreeHeap() < kHeapFloor) {
    noteFail("low heap");
    return false;
  }
  const HttpReply reply = httpPost(packet, packetLen);
  if (reply.code < 200 || reply.code > 299) {
    noteFail(reply.why[0] ? reply.why : "send failed");
    return false;
  }
  const GatewayConfig &cfg = Settings::get();
  gSent++;
  gSeq++;
  if (sent < gCount) {
    memmove(&gSamples[0], &gSamples[sent], (gCount - sent) * sizeof(Sample));
    gCount = static_cast<uint8_t>(gCount - sent);
  } else {
    gCount = 0;
  }
  gBackoffS = 5;
  gForce = gCount > 0;
  snprintf(gLast, sizeof(gLast), "http %d", reply.code);
  gNextTryMs = gCount == 0
                   ? millis() + static_cast<uint32_t>(cfg.uplinkPeriodS) * 1000ul
                   : millis();
  return true;
}

uint8_t fitSamples(char *buf, size_t cap,
                   size_t (*build)(char *, size_t, uint32_t, uint8_t), size_t *outLen) {
  for (uint8_t n = gCount; n > 0; n--) {
    const size_t len = build(buf, cap, gSeq + 1, n);
    if (len > 0) {
      *outLen = len;
      return n;
    }
  }
  *outLen = 0;
  return 0;
}

bool stationUp() {
  return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

}  // namespace

namespace Uplink {

void begin() {
  gEpoch = loadEpoch();
  gBoot = esp_random();
  if (gBoot == 0) {
    gBoot = 1;
  }
  snprintf(gLast, sizeof(gLast), "idle");
}

void kick() { gForce = true; }

void heartbeat() {
  const GatewayConfig &cfg = Settings::get();
  if (!cfg.uplinkOn) {
    Serial.println(F("Uplink is off."));
  }
  if (!cfg.uplinkHost[0]) {
    Serial.println(F("Heartbeat failed: no host."));
    return;
  }
  if (!cfg.uplinkBearer[0] && !cfg.uplinkKeySet) {
    Serial.println(F("Heartbeat failed: key missing."));
    return;
  }
  if (!stationUp()) {
    Serial.println(F("Heartbeat failed: STA not connected."));
    return;
  }
  if (ESP.getFreeHeap() < kHeapFloor) {
    Serial.println(F("Heartbeat failed: low heap."));
    return;
  }
  Serial.print(F("Heartbeat "));
  Serial.print(cfg.uplinkHost);
  Serial.print(':');
  Serial.print(cfg.uplinkPort);
  Serial.print(' ');
  Serial.println(cfg.uplinkPath);
  HttpReply reply{};
  if (cfg.uplinkBearer[0]) {
    char id[13];
    identityHex(id);
    char body[160];
    const int n = snprintf(
        body, sizeof(body),
        "{\"kind\":\"heartbeat\",\"identity\":\"%s\",\"fw\":\"%s\",\"git\":\"%s\"}",
        id, FW_VERSION, GIT_HASH);
    if (n <= 0 || n >= static_cast<int>(sizeof(body))) {
      Serial.println(F("Heartbeat failed: packet."));
      return;
    }
    reply = httpPost(reinterpret_cast<const uint8_t *>(body), static_cast<size_t>(n));
  } else {
    char plain[96];
    const int plainN = snprintf(plain, sizeof(plain),
                                 "{\"kind\":\"heartbeat\",\"boot\":%lu,\"seq\":0,\"n\":0}",
                                 static_cast<unsigned long>(gBoot));
    if (plainN <= 0 || plainN >= static_cast<int>(sizeof(plain))) {
      Serial.println(F("Heartbeat failed: packet."));
      return;
    }
    static uint8_t packet[30 + 96 + 16];
    size_t packetLen = 0;
    if (!seal(reinterpret_cast<uint8_t *>(plain), static_cast<size_t>(plainN), 0, 0,
              packet, &packetLen)) {
      Serial.println(F("Heartbeat failed: encrypt."));
      return;
    }
    reply = httpPost(packet, packetLen);
  }
  if (reply.code >= 200 && reply.code <= 299) {
    Serial.print(F("Heartbeat ok: "));
    Serial.println(reply.why);
    return;
  }
  Serial.print(F("Heartbeat failed: "));
  Serial.println(reply.why[0] ? reply.why : "send failed");
}

void printStatus() {
  const GatewayConfig &cfg = Settings::get();
  const uint32_t now = millis();
  Serial.print(F("uplink: "));
  Serial.println(cfg.uplinkOn ? F("on") : F("off"));
  Serial.print(F("host: "));
  if (cfg.uplinkHost[0]) {
    Serial.print(cfg.uplinkHost);
    Serial.print(':');
    Serial.print(cfg.uplinkPort);
    Serial.println(cfg.uplinkPath);
  } else {
    Serial.println(F("(none)"));
  }
  Serial.print(F("key: "));
  Serial.println(cfg.uplinkKeySet ? F("set") : F("missing"));
  Serial.print(F("bearer: "));
  Serial.println(cfg.uplinkBearer[0] ? F("set") : F("missing"));
  Serial.print(F("epoch: "));
  Serial.println(gEpoch);
  Serial.print(F("batch: "));
  Serial.print(gCount);
  Serial.print('/');
  Serial.print(cfg.uplinkBatch);
  Serial.print(F("  period: "));
  Serial.print(cfg.uplinkPeriodS);
  Serial.println(F("s"));
  Serial.print(F("seq: "));
  Serial.print(gSeq);
  Serial.print(F("  sent: "));
  Serial.print(gSent);
  Serial.print(F("  failed: "));
  Serial.print(gFailed);
  Serial.print(F("  dropped: "));
  Serial.print(gDropped);
  Serial.print(F("  oversized: "));
  Serial.println(gOversized);
  Serial.print(F("station: "));
  Serial.println(stationUp() ? F("up") : F("down"));
  Serial.print(F("heap: "));
  Serial.println(ESP.getFreeHeap());
  Serial.print(F("last: "));
  Serial.println(gLast);
  if (gNextTryMs > now) {
    Serial.print(F("next try: "));
    Serial.print((gNextTryMs - now) / 1000ul);
    Serial.println(F("s"));
  }
}

void poll() {
  const GatewayConfig &cfg = Settings::get();
  const uint32_t now = millis();
  const TelemetrySnapshot &snap = Telemetry::get();
  if (snap.busy) {
    return;
  }
  if (cfg.uplinkOn && snap.refreshedAtMs != 0) {
    const uint32_t periodMs = static_cast<uint32_t>(cfg.uplinkPeriodS) * 1000ul;
    if (gLastSampleMs == 0 || (now - gLastSampleMs) >= periodMs) {
      capture(snap);
      gLastSampleMs = now;
    }
  }
  if (!cfg.uplinkOn || !cfg.uplinkHost[0] || gCount == 0) {
    return;
  }
  if (!cfg.uplinkBearer[0] && !cfg.uplinkKeySet) {
    return;
  }
  const uint8_t want = cfg.uplinkBatch == 0 ? 1 : cfg.uplinkBatch;
  if (!gForce && gCount < want) {
    return;
  }
  if (!gForce && gNextTryMs != 0 && now < gNextTryMs) {
    return;
  }
  if (!stationUp()) {
    snprintf(gLast, sizeof(gLast), "no station");
    gNextTryMs = now + 15000ul;
    return;
  }
  if (cfg.uplinkBearer[0]) {
    static char body[7168];
    size_t bodyLen = 0;
    const uint8_t sent = fitSamples(body, sizeof(body), buildCloud, &bodyLen);
    if (sent == 0) {
      gOversized++;
      noteFail(gCount == 1 ? "sample too large" : "batch too large");
      return;
    }
    postBatch(reinterpret_cast<const uint8_t *>(body), bodyLen, sent);
    return;
  }
  static char plain[kPlainMax];
  size_t plainLen = 0;
  const uint8_t sent = fitSamples(plain, sizeof(plain), buildPlain, &plainLen);
  if (sent == 0) {
    gOversized++;
    noteFail(gCount == 1 ? "sample too large" : "batch too large");
    return;
  }
  static uint8_t packet[30 + kPlainMax + 16];
  size_t packetLen = 0;
  if (!seal(reinterpret_cast<uint8_t *>(plain), plainLen, sent, gSeq + 1, packet,
            &packetLen)) {
    noteFail("encrypt failed");
    return;
  }
  postBatch(packet, packetLen, sent);
}

}  // namespace Uplink

#endif
