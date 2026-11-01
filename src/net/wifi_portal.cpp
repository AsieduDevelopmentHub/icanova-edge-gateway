#if defined(ARDUINO_ARCH_ESP32)

#include "net/wifi_portal.h"

#include <WiFi.h>
#include <esp_wifi.h>
#include <lwip/dns.h>
#include <stdio.h>
#include <string.h>

#include <esp_system.h>

#include "config/config.h"
#include "config/settings.h"
#include "telemetry/snapshot.h"
#include "util/str_util.h"

namespace {

char gMode[12] = "off";
char gIp[16] = "0.0.0.0";
char gApSsid[33] = {};
char gLastStaSsid[33] = {};
uint8_t gLastBssid[6] = {};
int32_t gLastChannel = 0;
bool gHaveBssid = false;
bool gWantAp = false;
bool gWantSta = false;
bool gApUp = false;
bool gEventHooked = false;
int gApChannel = 0;
uint32_t gStaAttemptStart = 0;
uint32_t gStaRestUntil = 0;
uint8_t gStaFails = 0;
uint8_t gLastFailReason = 0;

void buildApSsid() {
  uint64_t mac = ESP.getEfuseMac();
  const uint16_t suffix =
      static_cast<uint16_t>((mac >> 32) ^ (mac & 0xFFFFu)) & 0xFFFFu;
  snprintf(gApSsid, sizeof(gApSsid), "%s-%04X", WIFI_AP_SSID_PREFIX, suffix);
}

void copyText(char *dst, size_t n, const char *src) {
  if (!dst || n == 0) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = '\0';
}

void refreshInfo() {
  const wifi_mode_t radio = WiFi.getMode();
  int8_t rssi = 0;
  if (radio == WIFI_OFF) {
    copyText(gMode, sizeof(gMode), "off");
    copyText(gIp, sizeof(gIp), "0.0.0.0");
  } else if (gWantAp && !gWantSta) {
    copyText(gMode, sizeof(gMode), "AP");
    copyText(gIp, sizeof(gIp), WiFi.softAPIP().toString().c_str());
  } else if (!gWantAp && (radio & WIFI_STA) != 0) {
    copyText(gMode, sizeof(gMode), "STA");
    if (WiFi.status() == WL_CONNECTED) {
      copyText(gIp, sizeof(gIp), WiFi.localIP().toString().c_str());
      rssi = static_cast<int8_t>(WiFi.RSSI());
    } else {
      copyText(gIp, sizeof(gIp), "0.0.0.0");
    }
  } else {
    copyText(gMode, sizeof(gMode), "AP+STA");
    if (WiFi.status() == WL_CONNECTED) {
      copyText(gIp, sizeof(gIp), WiFi.localIP().toString().c_str());
      rssi = static_cast<int8_t>(WiFi.RSSI());
    } else {
      copyText(gIp, sizeof(gIp), WiFi.softAPIP().toString().c_str());
    }
  }
  Telemetry::setWifiInfo(gMode, gIp, rssi);
}

struct RadioPlan {
  bool ap;
  bool sta;
};

RadioPlan planFor(const GatewayConfig &c) {
  const char *m = c.wifiMode[0] ? c.wifiMode : "auto";
  if (strEqualsIgnoreCase(m, "off") == 0) return {false, false};
  if (strEqualsIgnoreCase(m, "ap") == 0) return {true, false};
  if (strEqualsIgnoreCase(m, "sta") == 0) return {false, true};
  if (strEqualsIgnoreCase(m, "apsta") == 0) return {true, true};
  return {true, c.staSsid[0] != '\0'};
}

const char *staLink() {
  const wifi_mode_t radio = WiFi.getMode();
  if ((radio & WIFI_STA) == 0) return "not started";
  if (!gWantSta) return "idle";
  if (!Settings::get().staSsid[0]) return "no SSID configured";
  switch (WiFi.status()) {
    case WL_CONNECTED:
      return "connected";
    case WL_NO_SSID_AVAIL:
      return "not found";
    case WL_CONNECT_FAILED:
      return "authentication failed";
    case WL_CONNECTION_LOST:
      return "connection lost";
    case WL_IDLE_STATUS:
      return "connecting";
    case WL_DISCONNECTED:
      return "disconnected";
    default:
      return "not connected";
  }
}

const char *failReasonName(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
      return "auth expired";
    case WIFI_REASON_AUTH_LEAVE:
      return "auth leave";
    case WIFI_REASON_ASSOC_EXPIRE:
      return "assoc expired";
    case WIFI_REASON_ASSOC_TOOMANY:
      return "too many stations";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4-way handshake timeout";
    case WIFI_REASON_MIC_FAILURE:
      return "MIC failure";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
      return "4-way IE mismatch";
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return "802.1X auth failed";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "beacon timeout";
    case WIFI_REASON_NO_AP_FOUND:
      return "SSID not found";
    case WIFI_REASON_AUTH_FAIL:
      return "authentication failed";
    case WIFI_REASON_ASSOC_FAIL:
      return "association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "handshake timeout";
    case WIFI_REASON_CONNECTION_FAIL:
      return "connection failed";
    default:
      return "disconnected";
  }
}

bool handshakeLikeFail(uint8_t reason) {
  return reason == WIFI_REASON_AUTH_FAIL ||
         reason == WIFI_REASON_AUTH_EXPIRE ||
         reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
         reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
         reason == WIFI_REASON_802_1X_AUTH_FAILED ||
         reason == WIFI_REASON_CONNECTION_FAIL ||
         reason == WIFI_REASON_ASSOC_FAIL;
}

void applyRadioDefaults() {
  WiFi.setSleep(false);
#if defined(WIFI_AUTH_WPA_PSK)
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
#endif
#ifdef WIFI_FAST_SCAN
  if ((WiFi.getMode() & WIFI_STA) != 0) {
    WiFi.setScanMethod(WIFI_FAST_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  }
#endif
  wifi_country_t country = {};
  country.cc[0] = '0';
  country.cc[1] = '1';
  country.cc[2] = '\0';
  country.schan = 1;
  country.nchan = 13;
  country.max_tx_power = 20;
  country.policy = WIFI_COUNTRY_POLICY_MANUAL;
  esp_wifi_set_country(&country);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

void applyApPhy() {
  // 11n HT IEs after AP start are a common reason phones/laptops skip the BSS.
  const uint8_t proto = WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G;
  if ((WiFi.getMode() & WIFI_AP) != 0) {
    esp_wifi_set_protocol(WIFI_IF_AP, proto);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
  }
  if ((WiFi.getMode() & WIFI_STA) != 0) {
    esp_wifi_set_protocol(WIFI_IF_STA, proto);
    esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
  }
}

int8_t setTxPowerDbm() {
  for (int8_t q = 68; q >= 40; q = static_cast<int8_t>(q - 4)) {
    if (esp_wifi_set_max_tx_power(q) == ESP_OK) {
      int8_t got = 0;
      esp_wifi_get_max_tx_power(&got);
      return static_cast<int8_t>(got / 4);
    }
  }
  return 0;
}

bool programSoftAp(int ch) {
  const GatewayConfig &c = Settings::get();
  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  const size_t ssidLen = strlen(gApSsid);
  if (ssidLen == 0 || ssidLen > 32) {
    return false;
  }
  memcpy(conf.ap.ssid, gApSsid, ssidLen);
  conf.ap.ssid_len = static_cast<uint8_t>(ssidLen);
  conf.ap.channel = static_cast<uint8_t>(ch);
  conf.ap.ssid_hidden = 0;
  conf.ap.max_connection = 4;
  conf.ap.beacon_interval = 100;
  conf.ap.ftm_responder = false;
  const bool openAp = strlen(c.apPass) < 8;
  if (openAp) {
    conf.ap.authmode = WIFI_AUTH_OPEN;
  } else {
    strncpy(reinterpret_cast<char *>(conf.ap.password), c.apPass,
            sizeof(conf.ap.password) - 1);
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP;
  }
  if (!WiFi.enableAP(true)) {
    return false;
  }
  if (esp_wifi_set_config(WIFI_IF_AP, &conf) == ESP_OK) {
    return true;
  }
  if (openAp) {
    return false;
  }
  conf.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
  conf.ap.pairwise_cipher = WIFI_CIPHER_TYPE_TKIP_CCMP;
  return esp_wifi_set_config(WIFI_IF_AP, &conf) == ESP_OK;
}

void dumpAp() {
  wifi_config_t conf;
  memset(&conf, 0, sizeof(conf));
  if (esp_wifi_get_config(WIFI_IF_AP, &conf) != ESP_OK) {
    Serial.println(F("[WiFi] AP config read failed"));
    return;
  }
  int8_t txq = 0;
  esp_wifi_get_max_tx_power(&txq);
  Serial.print(F("[WiFi] AP cfg hidden="));
  Serial.print(conf.ap.ssid_hidden);
  Serial.print(F(" ch="));
  Serial.print(conf.ap.channel);
  Serial.print(F(" auth="));
  Serial.print(static_cast<int>(conf.ap.authmode));
  Serial.print(F(" beacon="));
  Serial.print(conf.ap.beacon_interval);
  Serial.print(F(" tx="));
  Serial.print(txq / 4);
  Serial.println(F(" dBm"));
}

bool startSoftAp(int channel, bool waitBeacons) {
  int ch = channel > 0 ? channel : 6;
  if (ch < 1 || ch > 11) {
    ch = 6;
  }
  applyApPhy();
  const GatewayConfig &c = Settings::get();
  const char *pass = strlen(c.apPass) < 8 ? nullptr : c.apPass;
  WiFi.softAP(gApSsid, pass, ch, 0, 4);
  const bool ok = programSoftAp(ch);
  // DHCP must start after set_config. Doing it first leaves phones on
  // "Obtaining IP address" after the password is accepted.
  const bool dhcp = WiFi.softAPConfig(
      IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1),
      IPAddress(255, 255, 255, 0), IPAddress(192, 168, 4, 2));
  gApUp = ok;
  gApChannel = ch;
  if (!ok) {
    Serial.println(F("[WiFi] SoftAP failed to start"));
    return false;
  }
  const int8_t dbm = setTxPowerDbm();
  Serial.print(F("[WiFi] TX "));
  Serial.print(static_cast<int>(dbm));
  Serial.println(F(" dBm"));
  if (dhcp) {
    Serial.println(F("[WiFi] AP DHCP 192.168.4.2+  then open http://192.168.4.1"));
  } else {
    Serial.println(F("[WiFi] AP DHCP failed — clients will sit on Obtaining IP"));
  }
  if (waitBeacons) {
    delay(400);
    dumpAp();
  }
  return true;
}

void reassertSoftAp() {
  if (!gWantAp) {
    return;
  }
  const int ch = WiFi.channel() > 0 ? WiFi.channel() : (gApChannel > 0 ? gApChannel : 1);
  if (gApUp && gApChannel == ch && WiFi.softAPSSID().length() > 0) {
    return;
  }
  startSoftAp(ch, false);
  Serial.print(F("[WiFi] AP reassert "));
  Serial.print(gApSsid);
  Serial.print(F(" ch "));
  Serial.println(ch);
}

void startStaJoin() {
  const GatewayConfig &c = Settings::get();
  if (!c.staSsid[0]) {
    return;
  }
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  delay(80);
  if (strcmp(gLastStaSsid, c.staSsid) != 0) {
    gHaveBssid = false;
    gLastChannel = 0;
    copyText(gLastStaSsid, sizeof(gLastStaSsid), c.staSsid);
  }
  if (gHaveBssid && gLastChannel > 0) {
    Serial.print(F("[WiFi] STA "));
    Serial.print(c.staSsid);
    Serial.print(F(" ch "));
    Serial.println(static_cast<int>(gLastChannel));
    WiFi.begin(c.staSsid, c.staPass, gLastChannel, gLastBssid, true);
  } else {
    Serial.print(F("[WiFi] STA "));
    Serial.println(c.staSsid);
    WiFi.begin(c.staSsid, c.staPass);
  }
  gStaAttemptStart = millis();
  gStaRestUntil = 0;
}

void scheduleStaRetry(uint32_t delayMs) {
  WiFi.setAutoReconnect(false);
  gStaAttemptStart = 0;
  gStaRestUntil = millis() + delayMs;
}

void giveUpSta(const char *why) {
  gWantSta = false;
  gStaAttemptStart = 0;
  gStaRestUntil = 0;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  Serial.print(F("[WiFi] STA stopped ("));
  Serial.print(why);
  Serial.println(F(")."));
  Serial.println(F("Keep the setup AP: wifi mode ap"));
  Serial.println(F("Join without the AP: wifi mode sta"));
  Serial.println(F("Retry this network: wifi join <ssid> [password]"));
  if (gWantAp) {
    reassertSoftAp();
  }
}

void manageSta() {
  if (!gWantSta || !Settings::get().staSsid[0]) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    gStaFails = 0;
    gStaAttemptStart = 0;
    return;
  }
  const uint32_t now = millis();
  if (now < gStaRestUntil) {
    return;
  }
  // A phone on the setup AP needs beacon time. Pause extra retries while
  // someone is associated, otherwise the next scan drops them.
  if (gWantAp && WiFi.softAPgetStationNum() > 0 && gStaFails > 0) {
    gStaRestUntil = now + 4000;
    return;
  }
  if (gStaFails >= 6) {
    giveUpSta("handshake kept failing");
    return;
  }
  const wl_status_t st = WiFi.status();
  const bool attemptExpired =
      gStaAttemptStart != 0 && (now - gStaAttemptStart) > 12000;
  if (st == WL_CONNECT_FAILED || st == WL_NO_SSID_AVAIL || attemptExpired) {
    gStaFails = static_cast<uint8_t>(gStaFails < 12 ? gStaFails + 1 : 12);
    if (st == WL_NO_SSID_AVAIL) {
      gHaveBssid = false;
    }
    const uint32_t rest = gStaFails >= 3 ? 8000 : 4000;
    Serial.print(F("[WiFi] STA "));
    Serial.print(staLink());
    if (gLastFailReason) {
      Serial.print(F(" ("));
      Serial.print(failReasonName(gLastFailReason));
      Serial.print(')');
    }
    Serial.print(F("; retry in "));
    Serial.print(rest / 1000);
    Serial.println(F("s"));
    scheduleStaRetry(rest);
    return;
  }
  if (gStaAttemptStart == 0) {
    startStaJoin();
  }
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    Serial.print(F("[WiFi] phone/laptop joined AP ("));
    Serial.print(WiFi.softAPgetStationNum());
    Serial.println(F(" client)"));
    return;
  }
  if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    Serial.println(F("[WiFi] AP client left"));
    return;
  }
  if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
    gLastChannel = info.wifi_sta_connected.channel;
    memcpy(gLastBssid, info.wifi_sta_connected.bssid, 6);
    gHaveBssid = gLastChannel > 0;
    gStaFails = 0;
    gLastFailReason = 0;
    if (gWantAp) {
      reassertSoftAp();
    }
    return;
  }
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    return;
  }
  if (event != ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    return;
  }
  const uint8_t reason = static_cast<uint8_t>(info.wifi_sta_disconnected.reason);
  // Reason 8 is our own WiFi.disconnect(). Treating it as a failure immediately
  // starts another join and the serial loop never yields to the CLI.
  if (reason == WIFI_REASON_ASSOC_LEAVE || !gWantSta) {
    return;
  }
  gLastFailReason = reason;
  gStaFails = static_cast<uint8_t>(gStaFails < 12 ? gStaFails + 1 : 12);
  Serial.print(F("[WiFi] STA drop: "));
  Serial.print(failReasonName(reason));
  Serial.print(F(" ("));
  Serial.print(static_cast<unsigned>(reason));
  Serial.println(')');
  if (handshakeLikeFail(reason)) {
    gHaveBssid = false;
    scheduleStaRetry(gWantAp ? 6000 : 3000);
    return;
  }
  if (reason == WIFI_REASON_NO_AP_FOUND) {
    gHaveBssid = false;
    scheduleStaRetry(gWantAp ? 8000 : 4000);
    return;
  }
  scheduleStaRetry(4000);
}

void printMac(const uint8_t mac[6]) {
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3],
                mac[4], mac[5]);
}

const char *resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "power-on";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "interrupt watchdog";
    case ESP_RST_TASK_WDT:
      return "task watchdog";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deep sleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unknown";
  }
}

const char *authName(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN:
      return "open";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:
      return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
      return "WPA2/WPA3";
    default:
      return "secured";
  }
}

}  // namespace

namespace WifiPortal {

void startRadio() {
  const GatewayConfig &c = Settings::get();
  if (c.apSsid[0]) {
    copyText(gApSsid, sizeof(gApSsid), c.apSsid);
  } else {
    buildApSsid();
  }

  const RadioPlan plan = planFor(c);
  gWantAp = plan.ap;
  gWantSta = plan.sta && c.staSsid[0];
  gStaFails = 0;
  gStaAttemptStart = 0;
  gStaRestUntil = 0;
  if (!gEventHooked) {
    WiFi.onEvent(onWifiEvent);
    gEventHooked = true;
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.setHostname(Settings::hostname());

  // ESP32-C3 SoftAP does not beacon in WIFI_AP. Keep the STA interface up
  // and idle. WIFI_OFF / disconnect(true) is what made "AP up" invisible.
  wifi_mode_t mode = WIFI_OFF;
  if (plan.ap) {
    mode = WIFI_AP_STA;
  } else if (plan.sta) {
    mode = WIFI_STA;
  }

  const wifi_mode_t prev = WiFi.getMode();
  if (mode == WIFI_OFF) {
    WiFi.disconnect(false, false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
  } else {
    if (!plan.sta && (prev & WIFI_STA) != 0) {
      WiFi.setAutoReconnect(false);
      WiFi.disconnect(false, false);
    }
    if (!plan.ap && (prev & WIFI_AP) != 0) {
      WiFi.softAPdisconnect(true);
    }
    if (prev != mode) {
      WiFi.mode(mode);
      delay(120);
    }
    applyRadioDefaults();
  }

  // Soft-AP first, then a short beacon window, then STA. On ESP32-C3 the
  // single radio stops AP beacons for the whole station scan; starting AP
  // after WiFi.begin also aborts the join.
  if (plan.ap) {
    const int ch = plan.sta && gHaveBssid && gLastChannel >= 1 && gLastChannel <= 11
                       ? static_cast<int>(gLastChannel)
                       : 6;
    if (startSoftAp(ch, true)) {
      Serial.print(F("[WiFi] AP "));
      Serial.print(gApSsid);
      Serial.print(F("  "));
      Serial.print(WiFi.softAPIP());
      Serial.print(F(" ch "));
      Serial.println(ch);
      Serial.println(F("[WiFi] Look for this SSID on 2.4 GHz. Do not switch mode yet."));
      if (strlen(c.apPass) < 8) {
        Serial.println(F("[WiFi] AP is open (no password)."));
      }
    }
  } else {
    gApUp = false;
    gApChannel = 0;
  }

  if (plan.sta && c.staSsid[0]) {
    if (plan.ap) {
      delay(250);
    }
    startStaJoin();
  } else if (plan.sta) {
    Serial.println(F("[WiFi] STA (no SSID configured)"));
  }
  if (!plan.ap && !plan.sta) {
    Serial.println(F("[WiFi] radio off"));
  }

  refreshInfo();
}

void begin() {
  Settings::setWifiHook(startRadio);
  startRadio();
}

void holdStaDns() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  // Soft-AP start/channel hop can clear STA DNS on ESP32.
  ip_addr_t primary;
  ip_addr_t secondary;
  IP_ADDR4(&primary, 1, 1, 1, 1);
  IP_ADDR4(&secondary, 8, 8, 8, 8);
  const ip_addr_t *current = dns_getserver(0);
  if (current && ip_addr_cmp(current, &primary)) {
    return;
  }
  dns_setserver(0, &primary);
  dns_setserver(1, &secondary);
  Serial.println(F("[WiFi] STA DNS set to 1.1.1.1"));
}

void poll() {
  refreshInfo();
  holdStaDns();
  manageSta();
}

const char *mode() { return gMode; }
const char *ssid() { return gApSsid; }
const char *ip() { return gIp; }
int8_t rssi() {
  if (WiFi.status() == WL_CONNECTED) {
    return static_cast<int8_t>(WiFi.RSSI());
  }
  return 0;
}

void printBrief() {
  refreshInfo();
  const wifi_mode_t radio = WiFi.getMode();
  Serial.print(F("Wi-Fi: AP "));
  if ((radio & WIFI_AP) == 0) {
    Serial.print(F("off"));
  } else {
    Serial.print(gApSsid[0] ? gApSsid : WiFi.softAPSSID().c_str());
    Serial.print(F(" ch"));
    Serial.print(WiFi.channel());
    Serial.print(' ');
    Serial.print(WiFi.softAPIP());
  }
  Serial.print(F(" | STA "));
  if ((radio & WIFI_STA) == 0) {
    Serial.println(F("off"));
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("connected "));
    Serial.println(WiFi.localIP());
    return;
  }
  Serial.println(staLink());
}

void printLine() {
  refreshInfo();
  const wifi_mode_t radio = WiFi.getMode();
  Serial.print(F("radio "));
  Serial.print(gMode);
  Serial.print(F(" | AP "));
  if ((radio & WIFI_AP) == 0) {
    Serial.print(F("off"));
  } else {
    Serial.print(gApSsid[0] ? gApSsid : WiFi.softAPSSID().c_str());
    Serial.print(F(" ch"));
    Serial.print(WiFi.channel());
  }
  Serial.print(F(" | STA "));
  Serial.print(staLink());
  if (WiFi.status() == WL_CONNECTED && (radio & WIFI_STA) != 0) {
    Serial.print(' ');
    Serial.print(WiFi.localIP());
    Serial.print(' ');
    Serial.print(static_cast<int>(WiFi.RSSI()));
    Serial.print(F(" dBm"));
  }
  Serial.println();
}

void printBoard() {
  refreshInfo();
  uint8_t staMac[6] = {};
  uint8_t apMac[6] = {};
  WiFi.macAddress(staMac);
  WiFi.softAPmacAddress(apMac);
  const uint32_t up = millis() / 1000UL;

  Serial.println(F("## BOARD"));
  Serial.print(F("Hostname: "));
  Serial.println(Settings::hostname());
  Serial.print(F("Chip: "));
  Serial.print(ESP.getChipModel());
  Serial.print(F(" rev "));
  Serial.println(ESP.getChipRevision());
  Serial.print(F("CPU: "));
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(F(" MHz"));
  Serial.print(F("Flash: "));
  Serial.print(ESP.getFlashChipSize() / 1024UL);
  Serial.println(F(" KB"));
  Serial.print(F("Heap free: "));
  Serial.print(ESP.getFreeHeap());
  Serial.println(F(" bytes"));
  Serial.print(F("SDK: "));
  Serial.println(ESP.getSdkVersion());
  Serial.print(F("STA MAC: "));
  printMac(staMac);
  Serial.println();
  Serial.print(F("AP MAC: "));
  printMac(apMac);
  Serial.println();
  Serial.print(F("Reset: "));
  Serial.println(resetReason());
  Serial.print(F("Uptime: "));
  Serial.print(up / 3600UL);
  Serial.print(':');
  const uint32_t mins = (up / 60UL) % 60UL;
  const uint32_t secs = up % 60UL;
  if (mins < 10) Serial.print('0');
  Serial.print(mins);
  Serial.print(':');
  if (secs < 10) Serial.print('0');
  Serial.println(secs);
  printLine();
}

void printStatus() {
  refreshInfo();
  const GatewayConfig &c = Settings::get();
  const wifi_mode_t radio = WiFi.getMode();
  uint8_t staMac[6] = {};
  uint8_t apMac[6] = {};
  WiFi.macAddress(staMac);
  WiFi.softAPmacAddress(apMac);

  Serial.println(F("## WIFI"));
  Serial.print(F("Saved mode: "));
  Serial.println(c.wifiMode[0] ? c.wifiMode : "auto");
  Serial.print(F("Radio: "));
  Serial.println(gMode);
  Serial.print(F("Hostname: "));
  Serial.println(Settings::hostname());

  Serial.println(F("Access point:"));
  if ((radio & WIFI_AP) == 0) {
    Serial.println(F("  status: off"));
  } else {
    Serial.println(F("  status: up"));
    Serial.print(F("  SSID: "));
    Serial.println(WiFi.softAPSSID());
    Serial.print(F("  IP: "));
    Serial.println(WiFi.softAPIP());
    Serial.print(F("  MAC: "));
    printMac(apMac);
    Serial.println();
    Serial.print(F("  Clients: "));
    Serial.println(WiFi.softAPgetStationNum());
    Serial.print(F("  Channel: "));
    Serial.println(WiFi.channel());
    if (gWantSta && WiFi.status() != WL_CONNECTED) {
      Serial.println(F("  note: AP beacon pauses during station scan"));
    }
  }

  Serial.println(F("Station:"));
  Serial.print(F("  status: "));
  Serial.println(staLink());
  Serial.print(F("  configured SSID: "));
  Serial.println(c.staSsid[0] ? c.staSsid : "(none)");
  Serial.print(F("  MAC: "));
  printMac(staMac);
  Serial.println();
  if ((radio & WIFI_STA) != 0 && WiFi.status() == WL_CONNECTED) {
    Serial.print(F("  joined SSID: "));
    Serial.println(WiFi.SSID());
    Serial.print(F("  BSSID: "));
    printMac(WiFi.BSSID());
    Serial.println();
    Serial.print(F("  IP: "));
    Serial.println(WiFi.localIP());
    Serial.print(F("  Gateway: "));
    Serial.println(WiFi.gatewayIP());
    Serial.print(F("  Mask: "));
    Serial.println(WiFi.subnetMask());
    Serial.print(F("  DNS: "));
    Serial.println(WiFi.dnsIP());
    Serial.print(F("  RSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
    Serial.print(F("  Channel: "));
    Serial.println(WiFi.channel());
  } else {
    Serial.println(F("  IP: 0.0.0.0"));
    Serial.println(F("  RSSI: n/a"));
    if (gLastFailReason) {
      Serial.print(F("  last fail: "));
      Serial.print(failReasonName(gLastFailReason));
      Serial.print(F(" ("));
      Serial.print(static_cast<unsigned>(gLastFailReason));
      Serial.println(')');
    }
  }
  Serial.println(F("Commands: wifi mode ap|sta|apsta|auto|off"));
  Serial.println(F("          wifi join <ssid> [password]"));
  Serial.println(F("          wifi disconnect | wifi scan"));
}

void scan() {
  if (WiFi.getMode() == WIFI_OFF) {
    Serial.println(F("Radio is off. Use: wifi mode apsta"));
    return;
  }
  const wifi_mode_t prev = WiFi.getMode();
  if ((prev & WIFI_STA) == 0) {
    WiFi.mode(static_cast<wifi_mode_t>(prev | WIFI_STA));
    delay(100);
  }
  if ((prev & WIFI_AP) != 0) {
    Serial.println(F("Scanning (setup AP hides until this finishes)..."));
  } else {
    Serial.println(F("Scanning..."));
  }
  const int n = WiFi.scanNetworks();
  if (n < 0) {
    Serial.println(F("Scan failed."));
  } else if (n == 0) {
    Serial.println(F("No networks found."));
  } else {
    const int shown = n > 20 ? 20 : n;
    for (int i = 0; i < shown; i++) {
      Serial.printf("%2d  %4d dBm  ch %-2d  %-10s  %s\n", i + 1, WiFi.RSSI(i),
                    WiFi.channel(i), authName(WiFi.encryptionType(i)),
                    WiFi.SSID(i).c_str());
    }
    if (n > shown) {
      Serial.print(F("("));
      Serial.print(n - shown);
      Serial.println(F(" more not shown)"));
    }
  }
  WiFi.scanDelete();
  if (gWantAp) {
    reassertSoftAp();
  } else if ((prev & WIFI_STA) == 0) {
    WiFi.mode(prev);
  }
}

void waitForSta(uint32_t timeoutMs) {
  if ((WiFi.getMode() & WIFI_STA) == 0) {
    printLine();
    return;
  }
  if (!Settings::get().staSsid[0]) {
    printLine();
    return;
  }
  const uint32_t start = millis();
  while ((millis() - start) < timeoutMs) {
    manageSta();
    if (WiFi.status() == WL_CONNECTED) {
      holdStaDns();
      refreshInfo();
      printLine();
      return;
    }
    delay(200);
  }
  refreshInfo();
  printLine();
}

void disconnectSta() {
  if ((WiFi.getMode() & WIFI_STA) == 0) {
    Serial.println(F("Station radio is not started."));
    return;
  }
  gWantSta = false;
  gStaAttemptStart = 0;
  gStaRestUntil = 0;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false);
  refreshInfo();
  Serial.println(F("Station disconnected. Saved SSID is unchanged."));
  Serial.println(F("It joins again on reboot, wifi mode apsta, or: wifi join <ssid> [password]"));
  printLine();
}

}  // namespace WifiPortal

#endif  // ARDUINO_ARCH_ESP32
