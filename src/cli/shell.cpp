#include "cli/shell.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "auth/auth.h"
#include "cli/cli.h"
#include "console/tty.h"
#include "config/settings.h"
#include "fs/editor.h"
#include "fs/vfs.h"
#include "util/str_util.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <Esp.h>
#include <WiFi.h>

#include "net/wifi_portal.h"
#include "uplink/uplink.h"
#endif

namespace {

bool eq(const char *a, const char *b) { return strEqualsIgnoreCase(a, b) == 0; }

char *trim(char *s) {
  if (!s) return s;
  while (*s == ' ' || *s == '\t') s++;
  char *e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t')) *--e = '\0';
  return s;
}

bool onePath(char *rest, char **path) {
  *path = strtok(rest, " \t");
  if (!*path) return false;
  return strtok(nullptr, " \t") == nullptr;
}

bool twoPaths(char *rest, char **a, char **b) {
  *a = strtok(rest, " \t");
  *b = strtok(nullptr, " \t");
  if (!*a || !*b) return false;
  return strtok(nullptr, " \t") == nullptr;
}

const char *kCmds[] = {
    "help",       "status",    "config",    "devices",     "device",
    "regs",       "get",       "poll",      "iterate",     "watch",
    "alarms",     "scf",       "write16",   "write06",     "read",
    "readhex",    "rawread",   "rs485raw",  "set",         "crc-test",
    "diagnostic", "repeat",    "dcem",      "envolt",      "logout",
    "exit",       "whoami",    "passwd",    "user",        "hostname",
    "uptime",     "reboot",    "restart",   "info",        "board",
    "wifi",       "uplink",    "heartbeat", "clear",       "cls",
    "pwd",        "ls",
    "cd",
    "cat",        "mkdir",     "rmdir",     "rm",          "cp",
    "mv",         "edit",      nullptr,
};

bool startsWith(const char *word, const char *prefix, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (!word[i]) return false;
    if (tolower(static_cast<unsigned char>(word[i])) !=
        tolower(static_cast<unsigned char>(prefix[i]))) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool shellComplete(char *line, size_t cap, size_t *len, size_t *cursor) {
  if (!cli.isShell() || !line || !len || !cursor) {
    return false;
  }
  for (size_t i = 0; i < *len; i++) {
    if (line[i] == ' ' || line[i] == '\t') {
      return false;
    }
  }
  const char *hits[48];
  int n = 0;
  for (int i = 0; kCmds[i] && n < 48; i++) {
    if (startsWith(kCmds[i], line, *len)) {
      hits[n++] = kCmds[i];
    }
  }
  if (n == 0) {
    return false;
  }
  if (n == 1) {
    snprintf(line, cap, "%s ", hits[0]);
    *len = strlen(line);
    *cursor = *len;
    return false;
  }
  Serial.println();
  int col = 0;
  for (int i = 0; i < n; i++) {
    char cell[20];
    snprintf(cell, sizeof(cell), "%-16s", hits[i]);
    Serial.print(cell);
    if (++col == 4) {
      Serial.println();
      col = 0;
    }
  }
  if (col) {
    Serial.println();
  }
  return true;
}

void Cli::cmdLogout() {
#if defined(ARDUINO_ARCH_ESP32)
  dropSession(true);
#else
  Serial.println(F("No login session on this board."));
#endif
}

void Cli::cmdPasswd(char *rest) {
  (void)rest;
#if !defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("Accounts are on the ESP32 console."));
  return;
#endif
  _addingUser = false;
  _mode = ConsoleMode::AskCur;
}

void Cli::cmdUser(char *rest) {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)rest;
  Serial.println(F("Accounts are on the ESP32 console."));
  return;
#endif
  char *sub = strtok(rest, " \t");
  if (!sub || eq(sub, "list")) {
    Auth::printUsers();
    return;
  }
  char *name = strtok(nullptr, " \t");
  if (eq(sub, "add")) {
    if (!name) {
      Serial.println(F("Usage: user add <name>"));
      return;
    }
    char err[72];
    if (!Auth::prepareAdd(name, err, sizeof(err))) {
      Serial.println(err);
      return;
    }
    strncpy(_loginUser, name, sizeof(_loginUser) - 1);
    _loginUser[sizeof(_loginUser) - 1] = '\0';
    _addingUser = true;
    _mode = ConsoleMode::AskNew;
    return;
  }
  if (eq(sub, "del")) {
    if (!name) {
      Serial.println(F("Usage: user del <name>"));
      return;
    }
    char err[72];
    if (!Auth::delUser(name, err, sizeof(err))) {
      Serial.println(err);
    } else {
      Serial.println(F("User deleted."));
    }
    return;
  }
  Serial.println(F("Usage: user list | user add <name> | user del <name>"));
}

void Cli::cmdWhoami() {
#if defined(ARDUINO_ARCH_ESP32)
  Serial.println(Auth::user());
#else
  Serial.println(F("console"));
#endif
}

void Cli::cmdHostname(char *rest) {
  char *name = strtok(rest, " \t");
  if (!name) {
    Serial.println(Settings::hostname());
    return;
  }
#if !defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("Hostname is saved on the ESP32 console."));
  return;
#else
  if (strtok(nullptr, " \t")) {
    Serial.println(F("Usage: hostname [name]"));
    return;
  }
  if (persistSetting("hostname", name)) {
    Serial.print(F("Hostname: "));
    Serial.println(Settings::hostname());
  }
#endif
}

void Cli::cmdClear() { tty.clearScreen(); }

void Cli::cmdReboot(char *rest) {
  (void)rest;
#if !defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("Reboot is available on the ESP32."));
#else
  Serial.println(F("Rebooting..."));
  Serial.flush();
  delay(80);
  ESP.restart();
#endif
}

void Cli::cmdInfo(char *rest) {
  (void)rest;
#if !defined(ARDUINO_ARCH_ESP32)
  Serial.println(F("Board info is on the ESP32 console."));
#else
  WifiPortal::printBoard();
#endif
}

void Cli::cmdWifi(char *rest) {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)rest;
  Serial.println(F("Wi-Fi commands are on the ESP32 console."));
  return;
#else
  char *sub = strtok(rest, " \t");
  if (!sub || eq(sub, "status")) {
    WifiPortal::printStatus();
    return;
  }
  if (eq(sub, "scan")) {
    WifiPortal::scan();
    return;
  }
  if (eq(sub, "disconnect")) {
    WifiPortal::disconnectSta();
    return;
  }
  if (eq(sub, "mode")) {
    char *mode = strtok(nullptr, " \t");
    if (!mode) {
      Serial.println(F("Usage: wifi mode auto|ap|sta|apsta|off"));
      return;
    }
    if (persistSetting("wifi.mode", mode)) {
      Serial.println(F("Wi-Fi mode applied."));
      WifiPortal::printLine();
    }
    return;
  }
  if (eq(sub, "join")) {
    char *ssid = strtok(nullptr, " \t");
    char *pass = strtok(nullptr, "");
    if (!ssid || !ssid[0]) {
      Serial.println(F("Usage: wifi join <ssid> [password]"));
      return;
    }
    if (!pass) {
      pass = const_cast<char *>("");
    } else {
      pass = trim(pass);
    }
    Settings::holdApply(true);
    bool ok = persistSetting("wifi.sta_ssid", ssid);
    if (ok) {
      ok = persistSetting("wifi.sta_pass", pass);
    }
    if (ok) {
      const char *cur = Settings::get().wifiMode;
      if (eq(cur, "ap") || eq(cur, "off")) {
        ok = persistSetting("wifi.mode", "apsta");
      }
    }
    Settings::holdApply(false);
    if (ok) {
      Serial.println(F("Joining..."));
      WifiPortal::waitForSta(18000);
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("Still not connected. Try: wifi mode sta"));
        Serial.println(F("Then: wifi join <ssid> [password]"));
        Serial.println(F("Or set the password with: config set wifi.sta_pass ..."));
      }
    }
    return;
  }
  Serial.println(F("Usage: wifi | wifi status | wifi scan"));
  Serial.println(F("       wifi mode auto|ap|sta|apsta|off"));
  Serial.println(F("       wifi join <ssid> [password] | wifi disconnect"));
#endif
}

void Cli::cmdUptime() {
  const uint32_t s = millis() / 1000UL;
  const uint32_t h = s / 3600UL;
  const uint32_t m = (s / 60UL) % 60UL;
  const uint32_t sec = s % 60UL;
  Serial.print(h);
  Serial.print(':');
  if (m < 10) Serial.print('0');
  Serial.print(m);
  Serial.print(':');
  if (sec < 10) Serial.print('0');
  Serial.println(sec);
}

void Cli::cmdPwd() { Serial.println(Vfs::cwd()); }

void Cli::cmdLs(char *rest) {
  char *path = strtok(rest, " \t");
  Vfs::ls(path);
}

void Cli::cmdCd(char *rest) {
  char *path = nullptr;
  if (rest && rest[0]) {
    if (!onePath(rest, &path)) {
      Serial.println(F("Usage: cd [path]"));
      return;
    }
  }
  Vfs::cd(path);
}

void Cli::cmdCat(char *rest) {
  char *path = nullptr;
  if (!onePath(rest, &path)) {
    Serial.println(F("Usage: cat <file>"));
    return;
  }
  Vfs::cat(path);
}

void Cli::cmdMkdir(char *rest) {
  char *path = nullptr;
  if (!onePath(rest, &path)) {
    Serial.println(F("Usage: mkdir <path>"));
    return;
  }
  if (Vfs::makeDir(path)) {
    Serial.println(F("Created."));
  }
}

void Cli::cmdRmdir(char *rest) {
  char *path = nullptr;
  if (!onePath(rest, &path)) {
    Serial.println(F("Usage: rmdir <path>"));
    return;
  }
  if (Vfs::removeDir(path)) {
    Serial.println(F("Removed."));
  }
}

void Cli::cmdRm(char *rest) {
  char *path = nullptr;
  if (!onePath(rest, &path)) {
    Serial.println(F("Usage: rm <file>"));
    return;
  }
  if (Vfs::rm(path)) {
    Serial.println(F("Removed."));
  }
}

void Cli::cmdCp(char *rest) {
  char *a = nullptr;
  char *b = nullptr;
  if (!twoPaths(rest, &a, &b)) {
    Serial.println(F("Usage: cp <src> <dst>"));
    return;
  }
  if (Vfs::cp(a, b)) {
    Serial.println(F("Copied."));
  }
}

void Cli::cmdMv(char *rest) {
  char *a = nullptr;
  char *b = nullptr;
  if (!twoPaths(rest, &a, &b)) {
    Serial.println(F("Usage: mv <src> <dst>"));
    return;
  }
  if (Vfs::mv(a, b)) {
    Serial.println(F("Moved."));
  }
}

void Cli::cmdEdit(char *rest) {
  char *path = nullptr;
  if (!onePath(rest, &path)) {
    Serial.println(F("Usage: edit <file>"));
    return;
  }
  if (Editor::open(path)) {
    _mode = ConsoleMode::Editor;
  }
}

void Cli::cmdConfig(char *rest) {
#if !defined(ARDUINO_ARCH_ESP32)
  if (!rest || !rest[0]) {
    printConfig();
    return;
  }
  Serial.println(F("Persistent config is on the ESP32 console."));
  return;
#else
  char *sub = strtok(rest, " \t");
  if (!sub) {
    printConfig();
    return;
  }
  if (eq(sub, "keys")) {
    Settings::printKeys();
    return;
  }
  if (eq(sub, "path")) {
    Serial.println(Settings::path());
    return;
  }
  if (eq(sub, "save")) {
    char err[80];
    if (!Settings::save(err, sizeof(err))) {
      Serial.println(err);
    } else {
      Serial.println(F("Saved."));
    }
    return;
  }
  if (eq(sub, "reload")) {
    char err[80];
    if (!Settings::reload(err, sizeof(err))) {
      Serial.println(err);
      return;
    }
    adoptSettings();
    Serial.println(F("Reloaded and applied."));
    return;
  }
  if (eq(sub, "get")) {
    char *key = strtok(nullptr, " \t");
    if (!key) {
      Serial.println(F("Usage: config get <key>"));
      return;
    }
    char val[68];
    if (!Settings::format(key, val, sizeof(val), false)) {
      Serial.println(F("Unknown key. Try: config keys"));
      return;
    }
    Serial.println(val);
    return;
  }
  if (eq(sub, "set")) {
    char *key = strtok(nullptr, " \t");
    char *val = strtok(nullptr, "");
    if (!key) {
      Serial.println(F("Usage: config set <key> <value>"));
      return;
    }
    if (!val) {
      val = const_cast<char *>("");
    } else {
      val = trim(val);
    }
    if (persistSetting(key, val)) {
      Serial.println(F("Updated."));
    }
    return;
  }
  Serial.println(F("Usage: config [keys|path|get|set|save|reload]"));
#endif
}

bool Cli::dispatchShell(const char *cmd, char *rest) {
  if (eq(cmd, "logout") || eq(cmd, "exit")) {
    cmdLogout();
    return true;
  }
  if (eq(cmd, "whoami")) {
    cmdWhoami();
    return true;
  }
  if (eq(cmd, "passwd")) {
    cmdPasswd(rest);
    return true;
  }
  if (eq(cmd, "user")) {
    cmdUser(rest);
    return true;
  }
  if (eq(cmd, "hostname")) {
    cmdHostname(rest);
    return true;
  }
  if (eq(cmd, "uptime")) {
    cmdUptime();
    return true;
  }
  if (eq(cmd, "reboot") || eq(cmd, "restart")) {
    cmdReboot(rest);
    return true;
  }
  if (eq(cmd, "info") || eq(cmd, "board")) {
    cmdInfo(rest);
    return true;
  }
  if (eq(cmd, "wifi")) {
    cmdWifi(rest);
    return true;
  }
  if (eq(cmd, "heartbeat")) {
#if defined(ARDUINO_ARCH_ESP32)
    Uplink::heartbeat();
#else
    Serial.println(F("Uplink heartbeat is on the ESP32 console."));
#endif
    return true;
  }
  if (eq(cmd, "clear") || eq(cmd, "cls")) {
    cmdClear();
    return true;
  }
  if (eq(cmd, "pwd")) {
    cmdPwd();
    return true;
  }
  if (eq(cmd, "ls") || eq(cmd, "dir")) {
    cmdLs(rest);
    return true;
  }
  if (eq(cmd, "cd")) {
    cmdCd(rest);
    return true;
  }
  if (eq(cmd, "cat") || eq(cmd, "type")) {
    cmdCat(rest);
    return true;
  }
  if (eq(cmd, "mkdir")) {
    cmdMkdir(rest);
    return true;
  }
  if (eq(cmd, "rmdir")) {
    cmdRmdir(rest);
    return true;
  }
  if (eq(cmd, "rm")) {
    cmdRm(rest);
    return true;
  }
  if (eq(cmd, "cp")) {
    cmdCp(rest);
    return true;
  }
  if (eq(cmd, "mv")) {
    cmdMv(rest);
    return true;
  }
  if (eq(cmd, "edit")) {
    cmdEdit(rest);
    return true;
  }
  if (eq(cmd, "config")) {
    cmdConfig(rest);
    return true;
  }
  return false;
}
