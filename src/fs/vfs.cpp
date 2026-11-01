#include "fs/vfs.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "util/str_util.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <LittleFS.h>

namespace {

char gCwd[96] = "/";
bool gMounted = false;

void fail(const __FlashStringHelper *msg) { Serial.println(msg); }

bool knownFile(const char *in, char *out, size_t cap) {
  if (!in || !in[0] || strchr(in, '/') != nullptr) {
    return false;
  }
  const char *dest = nullptr;
  if (strEqualsIgnoreCase(in, "motd") == 0 || strEqualsIgnoreCase(in, "mtd") == 0) {
    dest = "/etc/motd";
  } else if (strEqualsIgnoreCase(in, "conf") == 0 ||
             strEqualsIgnoreCase(in, "gateway.conf") == 0) {
    dest = "/etc/gateway.conf";
  }
  if (!dest) {
    return false;
  }
  strncpy(out, dest, cap - 1);
  out[cap - 1] = '\0';
  return true;
}

}  // namespace

namespace Vfs {

bool begin() {
  if (gMounted) {
    return true;
  }
  if (!LittleFS.begin(true)) {
    gMounted = false;
    return false;
  }
  gMounted = true;
  LittleFS.mkdir("/etc");
  strncpy(gCwd, "/etc", sizeof(gCwd) - 1);
  gCwd[sizeof(gCwd) - 1] = '\0';
  if (!LittleFS.exists("/etc/motd")) {
    File f = LittleFS.open("/etc/motd", "w");
    if (f) {
      f.println("iCaNova Edge Gateway");
      f.println("Family: iCaNova Edge");
      f.println("Imagine. Connect. Innovate.");
      f.println("Imagine. Connect. Innovate.");
      f.println();
      f.println("Industrial edge gateway for RS485 / Modbus RTU monitoring.");
      f.println("Type help for commands.");
      f.println("View:  cat motd     cat conf");
      f.println("Edit:  edit motd    edit conf     then wq to save");
      f.println("After editing /etc/gateway.conf: config reload");
      f.close();
    }
  }
  return true;
}

bool mounted() { return gMounted; }

const char *cwd() { return gCwd; }

bool resolve(const char *in, char *out, size_t cap) {
  if (!out || cap < 2) {
    return false;
  }
  if (!in) {
    in = "";
  }
  if (knownFile(in, out, cap)) {
    return true;
  }
  char tmp[128];
  if (in[0] == '/') {
    if (strlen(in) >= sizeof(tmp)) {
      return false;
    }
    strncpy(tmp, in, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
  } else {
    const int wr =
        snprintf(tmp, sizeof(tmp), "%s%s%s", gCwd,
                 (strcmp(gCwd, "/") == 0) ? "" : "/", in);
    if (wr < 0 || wr >= static_cast<int>(sizeof(tmp))) {
      return false;
    }
  }

  const char *parts[16];
  int np = 0;
  char *p = tmp;
  if (*p == '/') {
    p++;
  }
  while (*p && np < 16) {
    char *start = p;
    while (*p && *p != '/') {
      p++;
    }
    if (*p == '/') {
      *p = '\0';
      p++;
    }
    if (start[0] == '\0' || strcmp(start, ".") == 0) {
      continue;
    }
    if (strcmp(start, "..") == 0) {
      if (np > 0) {
        np--;
      }
      continue;
    }
    if (strlen(start) > 32) {
      return false;
    }
    parts[np++] = start;
  }

  if (np == 0) {
    strncpy(out, "/", cap - 1);
    out[cap - 1] = '\0';
    return true;
  }
  size_t used = 0;
  out[0] = '\0';
  for (int i = 0; i < np; i++) {
    const size_t pl = strlen(parts[i]);
    if (used + 1 + pl + 1 > cap) {
      return false;
    }
    out[used++] = '/';
    memcpy(out + used, parts[i], pl);
    used += pl;
    out[used] = '\0';
  }
  return true;
}

bool parentOf(const char *abs, char *out, size_t cap) {
  if (!abs || !out || cap < 2 || strcmp(abs, "/") == 0) {
    return false;
  }
  strncpy(out, abs, cap - 1);
  out[cap - 1] = '\0';
  char *sl = strrchr(out, '/');
  if (!sl) {
    return false;
  }
  if (sl == out) {
    out[1] = '\0';
    return true;
  }
  *sl = '\0';
  return true;
}

bool isDir(const char *abs) {
  if (!gMounted || !abs) {
    return false;
  }
  if (strcmp(abs, "/") == 0) {
    return true;
  }
  File f = LittleFS.open(abs);
  if (!f) {
    return false;
  }
  const bool dir = f.isDirectory();
  f.close();
  return dir;
}

bool ls(const char *path) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char full[96];
  if (!path || !path[0]) {
    strncpy(full, gCwd, sizeof(full) - 1);
    full[sizeof(full) - 1] = '\0';
  } else if (!resolve(path, full, sizeof(full))) {
    fail(F("Bad path."));
    return false;
  }
  File node = LittleFS.open(full);
  if (!node) {
    fail(F("No such file or directory."));
    return false;
  }
  if (!node.isDirectory()) {
    Serial.println(full);
    node.close();
    return true;
  }

  struct Ent {
    char name[36];
    bool dir;
    uint32_t size;
  };
  Ent ents[32];
  int n = 0;
  bool more = false;
  File child = node.openNextFile();
  while (child) {
    const char *nm = child.name();
    const char *base = nm ? nm : "";
    const char *sl = strrchr(base, '/');
    if (sl && sl[1]) {
      base = sl + 1;
    }
    if (base[0] && n < 32) {
      strncpy(ents[n].name, base, sizeof(ents[n].name) - 1);
      ents[n].name[sizeof(ents[n].name) - 1] = '\0';
      ents[n].dir = child.isDirectory();
      ents[n].size = child.isDirectory() ? 0 : child.size();
      n++;
    } else if (base[0]) {
      more = true;
    }
    child.close();
    child = node.openNextFile();
  }
  node.close();

  for (int i = 1; i < n; i++) {
    Ent key = ents[i];
    int j = i - 1;
    while (j >= 0 && strcasecmp(ents[j].name, key.name) > 0) {
      ents[j + 1] = ents[j];
      j--;
    }
    ents[j + 1] = key;
  }

  if (n == 0) {
    Serial.println(F("(empty)"));
  }
  for (int i = 0; i < n; i++) {
    if (ents[i].dir) {
      Serial.print(ents[i].name);
      Serial.println('/');
    } else {
      char line[64];
      snprintf(line, sizeof(line), "%-24s %lu", ents[i].name,
               static_cast<unsigned long>(ents[i].size));
      Serial.println(line);
    }
  }
  if (more) {
    Serial.println(F("(more files not shown)"));
  }
  return true;
}

bool cd(const char *path) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  if (!path || !path[0]) {
    strncpy(gCwd, isDir("/etc") ? "/etc" : "/", sizeof(gCwd) - 1);
    gCwd[sizeof(gCwd) - 1] = '\0';
    return true;
  }
  char full[96];
  if (!resolve(path, full, sizeof(full))) {
    fail(F("Bad path."));
    return false;
  }
  if (!isDir(full)) {
    fail(F("Not a directory."));
    return false;
  }
  strncpy(gCwd, full, sizeof(gCwd) - 1);
  gCwd[sizeof(gCwd) - 1] = '\0';
  return true;
}

bool cat(const char *path) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char full[96];
  if (!resolve(path, full, sizeof(full))) {
    fail(F("Bad path."));
    return false;
  }
  File f = LittleFS.open(full, "r");
  if (!f) {
    Serial.print(F("No such file: "));
    Serial.println(full);
    Serial.println(F("Try: ls    cat motd    cat conf"));
    return false;
  }
  if (f.isDirectory()) {
    f.close();
    Serial.println(F("That is a folder. Use ls."));
    return false;
  }
  size_t shown = 0;
  bool nl = true;
  while (f.available() && shown < 16384) {
    const int c = f.read();
    if (c < 0) {
      break;
    }
    Serial.write(static_cast<uint8_t>(c));
    nl = (c == '\n');
    shown++;
  }
  if (!nl) {
    Serial.println();
  }
  if (f.available()) {
    Serial.println(F("(truncated)"));
  }
  f.close();
  return true;
}

bool makeDir(const char *path) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char full[96];
  if (!resolve(path, full, sizeof(full)) || strcmp(full, "/") == 0) {
    fail(F("Bad path."));
    return false;
  }
  if (LittleFS.exists(full)) {
    fail(F("Already exists."));
    return false;
  }
  char parent[96];
  if (!parentOf(full, parent, sizeof(parent)) || !isDir(parent)) {
    fail(F("Parent folder does not exist."));
    return false;
  }
  if (!LittleFS.mkdir(full)) {
    fail(F("mkdir failed."));
    return false;
  }
  return true;
}

bool removeDir(const char *path) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char full[96];
  if (!resolve(path, full, sizeof(full)) || strcmp(full, "/") == 0) {
    fail(F("Bad path."));
    return false;
  }
  if (!isDir(full)) {
    fail(F("Not a directory."));
    return false;
  }
  File d = LittleFS.open(full);
  File child = d ? d.openNextFile() : File();
  if (child) {
    child.close();
    d.close();
    fail(F("Directory is not empty."));
    return false;
  }
  if (d) {
    d.close();
  }
  if (!LittleFS.rmdir(full)) {
    fail(F("rmdir failed."));
    return false;
  }
  return true;
}

bool rm(const char *path) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char full[96];
  if (!resolve(path, full, sizeof(full))) {
    fail(F("Bad path."));
    return false;
  }
  File f = LittleFS.open(full);
  if (!f) {
    fail(F("No such file."));
    return false;
  }
  if (f.isDirectory()) {
    f.close();
    fail(F("Is a directory. Use rmdir."));
    return false;
  }
  f.close();
  if (!LittleFS.remove(full)) {
    fail(F("rm failed."));
    return false;
  }
  return true;
}

static bool copyFile(const char *src, const char *dst) {
  File in = LittleFS.open(src, "r");
  File out = LittleFS.open(dst, "w");
  if (!in || !out) {
    if (in) in.close();
    if (out) out.close();
    return false;
  }
  uint8_t buf[128];
  while (in.available()) {
    const int n = in.read(buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    if (out.write(buf, static_cast<size_t>(n)) != static_cast<size_t>(n)) {
      in.close();
      out.close();
      return false;
    }
  }
  in.close();
  out.close();
  return true;
}

bool cp(const char *src, const char *dst) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char a[96];
  char b[96];
  if (!resolve(src, a, sizeof(a)) || !resolve(dst, b, sizeof(b))) {
    fail(F("Bad path."));
    return false;
  }
  if (strcmp(a, b) == 0) {
    fail(F("Source and destination are the same."));
    return false;
  }
  File in = LittleFS.open(a, "r");
  if (!in || in.isDirectory()) {
    if (in) in.close();
    fail(F("Source is not a file."));
    return false;
  }
  in.close();
  if (LittleFS.exists(b)) {
    fail(F("Destination exists."));
    return false;
  }
  if (!copyFile(a, b)) {
    fail(F("Copy failed."));
    return false;
  }
  return true;
}

bool mv(const char *src, const char *dst) {
  if (!gMounted) {
    fail(F("Filesystem is not mounted."));
    return false;
  }
  char a[96];
  char b[96];
  if (!resolve(src, a, sizeof(a)) || !resolve(dst, b, sizeof(b))) {
    fail(F("Bad path."));
    return false;
  }
  if (!LittleFS.exists(a)) {
    fail(F("No such file."));
    return false;
  }
  if (LittleFS.exists(b)) {
    fail(F("Destination exists."));
    return false;
  }
  if (!LittleFS.rename(a, b)) {
    fail(F("mv failed."));
    return false;
  }
  return true;
}

void showMotd() {
  if (!gMounted || !LittleFS.exists("/etc/motd")) {
    return;
  }
  Serial.println();
  File f = LittleFS.open("/etc/motd", "r");
  if (!f) {
    return;
  }
  while (f.available()) {
    Serial.write(static_cast<uint8_t>(f.read()));
  }
  f.close();
  Serial.println();
}

}  // namespace Vfs

#else

namespace Vfs {

static void unavailable() {
  Serial.println(F("File commands are on the ESP32 console."));
}

bool begin() { return false; }
bool mounted() { return false; }
const char *cwd() { return "/"; }
bool resolve(const char *, char *out, size_t cap) {
  if (out && cap) out[0] = '\0';
  return false;
}
bool parentOf(const char *, char *out, size_t cap) {
  if (out && cap) out[0] = '\0';
  return false;
}
bool isDir(const char *) { return false; }
bool ls(const char *) {
  unavailable();
  return false;
}
bool cd(const char *) {
  unavailable();
  return false;
}
bool cat(const char *) {
  unavailable();
  return false;
}
bool makeDir(const char *) {
  unavailable();
  return false;
}
bool removeDir(const char *) {
  unavailable();
  return false;
}
bool rm(const char *) {
  unavailable();
  return false;
}
bool cp(const char *, const char *) {
  unavailable();
  return false;
}
bool mv(const char *, const char *) {
  unavailable();
  return false;
}
void showMotd() {}

}  // namespace Vfs

#endif
