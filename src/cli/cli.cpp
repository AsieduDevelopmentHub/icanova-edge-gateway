#include "cli/cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth/auth.h"
#include "brand/product_identity.h"
#include "cli/shell.h"
#include "config/config.h"
#include "config/settings.h"
#include "console/tty.h"
#include "devices/dse7320_registers.h"
#include "devices/dse_scf.h"
#include "devices/modbus_device.h"
#include "fs/editor.h"
#include "fs/vfs.h"
#include "modbus/modbus_client.h"
#include "modbus/modbus_crc.h"
#include "rs485/rs485.h"
#include "util/str_util.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "identity/device_id.h"
#include "net/wifi_portal.h"
#include "telemetry/snapshot.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "uplink/uplink.h"
#endif
#include <WiFi.h>
#endif

#define strcasecmp strEqualsIgnoreCase

Cli cli;

void Cli::begin() {
  devices.begin();
  _functionCode = 0x03;
  _lastInputMs = millis();
  _addingUser = false;
  _loginUser[0] = '\0';
  _pwNew[0] = '\0';
#if defined(ARDUINO_ARCH_ESP32)
  Settings::begin();
  adoptSettings();
  Auth::begin();
  _mode = ConsoleMode::LoginUser;
#else
  applySelectedDevice();
  _mode = ConsoleMode::Shell;
#endif
  tty.begin();
  tty.setCompleter(shellComplete);
}

void Cli::applySelectedDevice() {
  const ModbusDeviceConfig &d = devices.selected();
  _slaveId = d.slaveId;
  _baudRate = d.baudRate;
  rs485.applyConfig(_baudRate, d.dataBits, d.parity, d.stopBits);
}

ModbusTxnInfo Cli::makeTxnInfo() const {
  const ModbusDeviceConfig &d = devices.selected();
  ModbusTxnInfo info;
  info.deviceName = d.name;
  info.deviceSerial = d.serial;
  info.baud = _baudRate;
  info.dataBits = rs485.dataBits();
  info.parity = rs485.parity();
  info.stopBits = rs485.stopBits();
  return info;
}

void Cli::printBrandIntro() {
  // Text wordmark until a final logo is approved (doc/brand/assets/logo).
  // Display strings only — see src/brand/product_identity.h.
  Serial.println();
  Serial.println(F("====================================="));
  Serial.print(F("             "));
  Serial.println(F(ICANOVA_BRAND));
  Serial.print(F("       "));
  Serial.println(F(ICANOVA_PRODUCT_NAME));
  Serial.println(F("====================================="));
  Serial.println(F(ICANOVA_TAGLINE));
  Serial.print(F("Family: "));
  Serial.println(F(ICANOVA_PRODUCT_FAMILY));
  Serial.println();
  Serial.print(F("Board: "));
  Serial.println(F(BOARD_NAME));
#if defined(ARDUINO_ARCH_ESP32)
  {
    char id[33];
    char mac[13];
    DeviceId::linkIdentity(id, sizeof(id));
    DeviceId::hwMacHex(mac);
    Serial.print(F("Device ID: "));
    Serial.println(id);
    Serial.print(F("HW MAC: "));
    Serial.println(mac);
  }
#endif
  Serial.println(F("Communication: RS485 / Modbus RTU"));
}

void Cli::present() {
  const ModbusDeviceConfig &d = devices.selected();
  printBrandIntro();
  Serial.print(F("Selected: "));
  Serial.println(d.name);
  Serial.print(F("Slave ID: "));
  Serial.println(_slaveId);
  Serial.print(F("Baud: "));
  Serial.println(_baudRate);
#if defined(GATEWAY_LITE)
  Serial.println(F("Build: GATEWAY_LITE (Modbus diagnostics; no DSE named map)"));
#endif
#if defined(ARDUINO_ARCH_ESP32)
  Auth::logout();
  Editor::close();
  _addingUser = false;
  memset(_pwNew, 0, sizeof(_pwNew));
  memset(_loginUser, 0, sizeof(_loginUser));
  _mode = ConsoleMode::LoginUser;
  tty.discard();
  Serial.println(F("USB console. Log in to continue."));
  if (!Vfs::mounted()) {
    Serial.println(F("WARNING: config filesystem is not mounted."));
  }
  if (Auth::factoryPassword()) {
    Serial.println(F("Factory login: admin / admin"));
  }
  WifiPortal::printLine();
#else
  Serial.println(F("Type 'help' to start."));
#endif
  showPrompt();
}

void Cli::printHelp() {
  Serial.println(F("Console:"));
  Serial.println(F("  help | logout | whoami | hostname | uptime | passwd | reboot"));
  Serial.println(F("  info | board | clear | cls"));
  Serial.println(F("  wifi | wifi status | wifi scan"));
  Serial.println(F("  wifi mode auto|ap|sta|apsta|off"));
  Serial.println(F("  wifi join <ssid> [password] | wifi disconnect"));
  Serial.println(F("  user list | user add <name> | user del <name>"));
  Serial.println(F("  config | config keys | config get <key> | config set <key> <value>"));
  Serial.println(F("  config save | config reload | config path"));
  Serial.println(F("  pwd | ls | cd | cat motd | cat conf | edit motd | edit conf"));
  Serial.println(F("  mkdir <path> | rmdir <path> | rm <file> | cp <src> <dst> | mv <src> <dst>"));
  Serial.println(F("  Editing: arrows, Home/End, Tab, Ctrl-U, Ctrl-W, Ctrl-L, history"));
  Serial.println(F("Bus:"));
  Serial.println(F("  help | status | config"));
  Serial.println(F("  devices | device list|show|select <key>"));
  Serial.println(F("  regs | registers                  (DSE map list)"));
  Serial.println(F("  get <key|#|addr>                   (named scaled read)"));
  Serial.println(F("  poll [basic|engine|power|alarms|leds|identity|scf|all]"));
  Serial.println(F("  iterate [basic|power|engine|all]   (batch readable dump)"));
  Serial.println(F("  iterate <n> [mode]                 (repeat n times)"));
  Serial.println(F("  watch [n]                          (loop: comm/regs/scf flags/alarms)"));
  Serial.println(F("  alarms                             (active alarms only)"));
  Serial.println(F("  scf list|flags|<cmd>|raw <key>     (DSE control write)"));
  Serial.println(F("  Web Configure tab                  (GenComm page read/write)"));
  Serial.println(F("  uplink | uplink now | uplink heartbeat"));
  Serial.println(F("  write16 <addr> <word> [word...]    (FC16, max 16 words)"));
  Serial.println(F("  write06 <addr> <word>              (FC06 one register)"));
  Serial.println(F("  set slave <id> | set fc <03|04> | set baud <4800|9600|19200>"));
  Serial.println(F("  set parity <none|even|odd>"));
  Serial.println(F("  set settle <us> | set predelay <us>   (RS485 DE timing)"));
  Serial.println(F("  set telemetry on|off                  (DSE web poll; default off)"));
  Serial.println(F("  read <addr> <qty> | readhex <hex> <qty>"));
  Serial.println(F("  rawread <slave> <fc> <hex_addr> <qty>"));
  Serial.println(F("  rs485raw <slave> <fc> <hex_addr> <qty>"));
  Serial.println(F("  rs485raw-repeat <n> <slave> <fc> <hex_addr> <qty>"));
  Serial.println(F("  crc-test"));
  Serial.println(F("  diagnostic on|off"));
  Serial.println(F("  repeat <n> <slave> <fc> <hex_addr> <qty>"));
  Serial.println(F("  dcem                               (ET1641 format + address scan)"));
  Serial.println(F("  envolt [slave]                     (EN-Volt register image scan)"));
  Serial.println();
  Serial.println(F("DSE examples:"));
  Serial.println(F("  iterate | iterate basic | iterate 3 power"));
  Serial.println(F("  watch | watch 8"));
  Serial.println(F("  get battery_v | get eng_rpm | get eng_status"));
  Serial.println(F("  set slave 1 | set baud 9600 | set settle 200"));
  Serial.println(F("  scf flags | scf auto | scf telemetry_start"));
}

void Cli::printStatus() {
  const ModbusDeviceConfig &d = devices.selected();
  Serial.println(F("Status: ready"));
  Serial.print(F("Device: "));
  Serial.println(d.name);
  Serial.print(F("Slave: "));
  Serial.println(_slaveId);
  Serial.print(F("Baud: "));
  Serial.println(_baudRate);
  Serial.print(F("FC: 0x"));
  Serial.println(_functionCode, HEX);
  Serial.print(F("Diagnostic: "));
  Serial.println(modbus.diagnostic() ? F("ON") : F("OFF"));
#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F("User: "));
  Serial.println(Auth::user());
  WifiPortal::printBrief();
#endif
}

void Cli::printConfig() {
  const ModbusDeviceConfig &d = devices.selected();
  Serial.println(F("## CURRENT DEVICE"));
  Serial.print(F("Name: "));
  Serial.println(d.name);
  Serial.print(F("Model: "));
  Serial.println(d.model);
  if (d.serial) {
    Serial.print(F("Serial: "));
    Serial.println(d.serial);
  }
  Serial.println();
  Serial.println(F("## MODBUS"));
  Serial.print(F("Slave ID: "));
  Serial.println(_slaveId);
  Serial.print(F("Baud: "));
  Serial.println(rs485.baud());
  Serial.print(F("Parity: "));
  Serial.println(DeviceRegistry::parityName(rs485.parity()));
  Serial.print(F("Stop bits: "));
  Serial.println(rs485.stopBits());
  Serial.print(F("Function: 0x"));
  if (_functionCode < 0x10) {
    Serial.print('0');
  }
  Serial.println(_functionCode, HEX);
  Serial.println();
  Serial.println(F("## BOARD / RS485"));
  Serial.print(F("Board: "));
  Serial.println(F(BOARD_NAME));
  Serial.print(F("UART: "));
  Serial.println(F(RS485_UART_LABEL));
  Serial.print(F("RX pin: "));
  Serial.println(RS485_RX_PIN);
  Serial.print(F("TX pin: "));
  Serial.println(RS485_TX_PIN);
  Serial.print(F("DE/RE pin: "));
  Serial.println(RS485_DE_RE_PIN);
  Serial.print(F("TX settle us: "));
  Serial.println(rs485.txSettleUs());
  Serial.print(F("TX predelay us: "));
  Serial.println(rs485.txPreDelayUs());
  Serial.print(F("RS485 VCC: "));
  Serial.println(F(MAX485_VCC_NOTE));
  Serial.print(F("RO note: "));
  Serial.println(F(MAX485_RO_NOTE));
  Serial.println(F("USB Serial (CLI) and RS485 UART are separate."));
#if defined(ARDUINO_ARCH_ESP32)
  Serial.println();
  Settings::printSummary();
#endif
}

void Cli::cmdDiagnostic(char *args) {
  char *tok = strtok(args, " \t");
  if (!tok) {
    Serial.print(F("Diagnostic: "));
    Serial.println(modbus.diagnostic() ? F("ON") : F("OFF"));
    Serial.println(F("Usage: diagnostic on|off"));
    return;
  }
  if (strcasecmp(tok, "on") == 0) {
    modbus.setDiagnostic(true);
    Serial.println(F("Diagnostic ON (verbose TX/RX timing)"));
  } else if (strcasecmp(tok, "off") == 0) {
    modbus.setDiagnostic(false);
    Serial.println(F("Diagnostic OFF"));
  } else {
    Serial.println(F("Usage: diagnostic on|off"));
  }
}

void Cli::cmdDevice(char *args) {
  char *tok = strtok(args, " \t");
  if (!tok || strcasecmp(tok, "list") == 0) {
    devices.printList();
    return;
  }
  if (strcasecmp(tok, "show") == 0) {
    devices.printSelected();
    return;
  }
  if (strcasecmp(tok, "select") == 0) {
    char *key = strtok(nullptr, " \t");
    if (!key) {
      Serial.println(F("Usage: device select <name|index>"));
      return;
    }
#if defined(ARDUINO_ARCH_ESP32)
    if (!persistSetting("modbus.device", key)) {
      return;
    }
#else
    if (!devices.selectByKey(key)) {
      Serial.println(F("ERROR: unknown device"));
      return;
    }
    applySelectedDevice();
#endif
    Serial.print(F("Selected "));
    Serial.println(devices.selected().name);
    devices.printSelected();
    return;
  }
  Serial.println(F("Usage: device list|show|select <key>"));
}

void Cli::cmdSet(char *args) {
  char *tok = strtok(args, " \t");
  char *val = strtok(nullptr, " \t");
  if (!tok || !val) {
    Serial.println(F(
        "Usage: set slave|fc|baud|settle|predelay <value>"));
    return;
  }
  if (strcasecmp(tok, "slave") == 0) {
    const long id = strtol(val, nullptr, 0);
    if (id < 1 || id > 247) {
      Serial.println(F("ERROR: slave ID must be 1..247"));
      return;
    }
    _slaveId = static_cast<uint8_t>(id);
    char saved[8];
    snprintf(saved, sizeof(saved), "%u", static_cast<unsigned>(_slaveId));
    persistSetting("modbus.slave", saved);
    Serial.print(F("Slave ID: "));
    Serial.println(_slaveId);
    return;
  }
  if (strcasecmp(tok, "fc") == 0) {
    const long fc = strtol(val, nullptr, 16);
    if (fc != 0x03 && fc != 0x04) {
      Serial.println(F("ERROR: FC must be 03 or 04"));
      return;
    }
    _functionCode = static_cast<uint8_t>(fc);
    persistSetting("modbus.fc", _functionCode == 0x04 ? "04" : "03");
    Serial.print(F("FC set to 0x"));
    Serial.println(_functionCode, HEX);
    return;
  }
  if (strcasecmp(tok, "baud") == 0) {
    const long baud = strtol(val, nullptr, 10);
    if (baud != 4800 && baud != 9600 && baud != 19200) {
      Serial.println(F("ERROR: baud must be 4800, 9600, or 19200"));
      return;
    }
    _baudRate = static_cast<uint32_t>(baud);
    rs485.applyConfig(_baudRate, rs485.dataBits(), rs485.parity(),
                      rs485.stopBits());
    char saved[12];
    snprintf(saved, sizeof(saved), "%lu", static_cast<unsigned long>(_baudRate));
    persistSetting("modbus.baud", saved);
    Serial.print(F("UART baud set to "));
    Serial.println(_baudRate);
    return;
  }
  if (strcasecmp(tok, "settle") == 0) {
    const long us = strtol(val, nullptr, 10);
    if (us < 0 || us > 20000) {
      Serial.println(F("ERROR: settle must be 0..20000 us"));
      return;
    }
    rs485.setTxSettleUs(static_cast<uint32_t>(us));
    char saved[12];
    snprintf(saved, sizeof(saved), "%lu", static_cast<unsigned long>(us));
    persistSetting("modbus.settle_us", saved);
    Serial.print(F("TX settle set to "));
    Serial.print(us);
    Serial.println(F(" us (post uart_wait_tx_done, before DE->RX)"));
    return;
  }
  if (strcasecmp(tok, "predelay") == 0) {
    const long us = strtol(val, nullptr, 10);
    if (us < 0 || us > 5000) {
      Serial.println(F("ERROR: predelay must be 0..5000 us"));
      return;
    }
    rs485.setTxPreDelayUs(static_cast<uint32_t>(us));
    char saved[12];
    snprintf(saved, sizeof(saved), "%lu", static_cast<unsigned long>(us));
    persistSetting("modbus.predelay_us", saved);
    Serial.print(F("TX predelay set to "));
    Serial.print(us);
    Serial.println(F(" us (after DE assert, before write)"));
    return;
  }
  if (strcasecmp(tok, "parity") == 0) {
    uint8_t parity = 0;
    if (strcasecmp(val, "none") == 0 || strcmp(val, "0") == 0) {
      parity = 0;
    } else if (strcasecmp(val, "odd") == 0 || strcmp(val, "1") == 0) {
      parity = 1;
    } else if (strcasecmp(val, "even") == 0 || strcmp(val, "2") == 0) {
      parity = 2;
    } else {
      Serial.println(F("ERROR: parity must be none, even, or odd"));
      return;
    }
    rs485.applyConfig(rs485.baud(), rs485.dataBits(), parity, rs485.stopBits());
    const char *parityName =
        parity == 1 ? "odd" : (parity == 2 ? "even" : "none");
    persistSetting("modbus.parity", parityName);
    Serial.print(F("Parity set to "));
    Serial.println(DeviceRegistry::parityName(parity));
    return;
  }
#if defined(ARDUINO_ARCH_ESP32)
  if (strcasecmp(tok, "telemetry") == 0) {
    if (strcasecmp(val, "on") == 0) {
      Telemetry::setAutoRefresh(true);
    } else if (strcasecmp(val, "off") == 0) {
      Telemetry::setAutoRefresh(false);
    } else {
      Serial.println(F("Usage: set telemetry on|off"));
      return;
    }
    persistSetting("modbus.telemetry",
                   strcasecmp(val, "on") == 0 ? "on" : "off");
    Serial.print(F("Telemetry auto-refresh: "));
    Serial.println(Telemetry::autoRefresh() ? F("ON") : F("OFF"));
    return;
  }
#endif
  Serial.println(F("Unknown set target"));
}

bool Cli::ensureDseSelected() const {
  if (strcasecmp(devices.selected().id, "dse7320") != 0) {
    Serial.println(F("NOTE: named DSE registers apply to dse7320 (7320 / 7320 MKII)."));
    Serial.println(F("      Use: device select dse7320"));
    return false;
  }
  return true;
}

void Cli::readAndPrintRegister(const DseRegister &reg) {
  const uint16_t qty = static_cast<uint16_t>(reg.width);
  ModbusTxnInfo info = makeTxnInfo();
  info.address = reg.address;
  info.quantity = qty;
  // Named reads always use FC03 holding registers (map definition).
  ModbusResponse resp =
      modbus.readRegisters(_slaveId, 0x03, reg.address, qty, &info);
  if (resp.result != ModbusResult::Ok) {
    // A short reply is sometimes torn by the DE edge. One retry is enough.
    delay(30);
    resp = modbus.readRegisters(_slaveId, 0x03, reg.address, qty, &info);
  }
  if (resp.result != ModbusResult::Ok) {
    Serial.print(F("FAIL: "));
    Serial.print(reg.key);
    Serial.print(F(" @"));
    Serial.println(reg.address);
    return;
  }
  const DseDecodedValue decoded =
      DseRegisters::decode(reg, resp.values, resp.valueCount);
  DseRegisters::printDecoded(decoded);
}

void Cli::cmdRegs(char *args) {
  (void)args;
  DseRegisters::printList();
}

void Cli::cmdGet(char *args) {
  char *tok = strtok(args, " \t");
  if (!tok) {
    Serial.println(F("Usage: get <key|#|address>"));
    Serial.println(F("  get battery_v | get 1 | get 1029"));
    return;
  }
  if (!ensureDseSelected()) {
    return;
  }

  const DseRegister *reg = DseRegisters::findByKey(tok);
  if (!reg) {
    char *end = nullptr;
    const long n = strtol(tok, &end, 0);
    if (end && *end == '\0' && n > 0) {
      reg = DseRegisters::findByIndex1(static_cast<size_t>(n));
      if (!reg) {
        reg = DseRegisters::findByAddress(static_cast<uint16_t>(n));
      }
    }
  }
  if (!reg) {
    Serial.println(F("ERROR: unknown register (try 'regs')"));
    return;
  }

  Serial.print(F("GET "));
  DseRegisters::printEntry(*reg);
  readAndPrintRegister(*reg);
}

void Cli::cmdPoll(char *args) {
  char *tok = strtok(args, " \t");
  const char *mode = tok ? tok : "basic";
  if (tok && strcasecmp(mode, "basic") != 0 && strcasecmp(mode, "power") != 0 &&
      strcasecmp(mode, "leds") != 0 && strcasecmp(mode, "all") != 0 &&
      strcasecmp(mode, "engine") != 0 && strcasecmp(mode, "alarms") != 0 &&
      strcasecmp(mode, "identity") != 0 && strcasecmp(mode, "scf") != 0 &&
      strcasecmp(mode, "state") != 0) {
    Serial.println(
        F("Usage: poll [basic|engine|power|alarms|leds|identity|scf|state|all]"));
    return;
  }
  if (!ensureDseSelected()) {
    return;
  }

  auto want = [&](const DseRegister &r) -> bool {
    if (strcasecmp(mode, "all") == 0) {
      return true;
    }
    if (strcasecmp(mode, "leds") == 0) {
      return strncmp(r.key, "led_", 4) == 0;
    }
    if (strcasecmp(mode, "alarms") == 0) {
      return r.kind == DseDecodeKind::AlarmPacked;
    }
    if (strcasecmp(mode, "scf") == 0) {
      return strncmp(r.key, "scf_", 4) == 0;
    }
    if (strcasecmp(mode, "identity") == 0) {
      return strcmp(r.key, "mfr_code") == 0 || strcmp(r.key, "model") == 0 ||
             strcmp(r.key, "serial") == 0;
    }
    if (strcasecmp(mode, "state") == 0) {
      return strcmp(r.key, "op_mode") == 0 || strcmp(r.key, "eng_status") == 0;
    }
    if (strcasecmp(mode, "engine") == 0) {
      return strcmp(r.key, "oil_press") == 0 ||
             strcmp(r.key, "coolant_temp") == 0 ||
             strcmp(r.key, "oil_temp") == 0 || strcmp(r.key, "fuel_level") == 0 ||
             strcmp(r.key, "battery_v") == 0 ||
             strcmp(r.key, "charge_alt_v") == 0 ||
             strcmp(r.key, "eng_rpm") == 0 || strcmp(r.key, "eng_status") == 0 ||
             strcmp(r.key, "op_mode") == 0 || strcmp(r.key, "eng_hours") == 0 ||
             strcmp(r.key, "eng_starts") == 0 ||
             strcmp(r.key, "gen_pct_kw") == 0 ||
             strcmp(r.key, "oil_level") == 0 ||
             strcmp(r.key, "coolant_level") == 0;
    }
    if (strcasecmp(mode, "power") == 0) {
      return strncmp(r.key, "gen_p", 5) == 0 ||
             strncmp(r.key, "gen_q", 5) == 0 ||
             strncmp(r.key, "gen_s", 5) == 0 ||
             strncmp(r.key, "gen_pf", 6) == 0 ||
             strcmp(r.key, "gen_kwh_exp") == 0 ||
             strcmp(r.key, "gen_pct_kw") == 0;
    }
    // basic
    return strcmp(r.key, "battery_v") == 0 || strcmp(r.key, "charge_alt_v") == 0 ||
           strcmp(r.key, "eng_rpm") == 0 || strcmp(r.key, "eng_status") == 0 ||
           strcmp(r.key, "op_mode") == 0 || strcmp(r.key, "gen_freq") == 0 ||
           strcmp(r.key, "gen_van") == 0 || strcmp(r.key, "gen_vbn") == 0 ||
           strcmp(r.key, "gen_vcn") == 0 || strcmp(r.key, "gen_ia") == 0 ||
           strcmp(r.key, "gen_ib") == 0 || strcmp(r.key, "gen_ic") == 0 ||
           strcmp(r.key, "gen_p_tot") == 0 || strcmp(r.key, "gen_pf") == 0 ||
           strcmp(r.key, "gen_pct_kw") == 0 || strcmp(r.key, "oil_press") == 0 ||
           strcmp(r.key, "coolant_temp") == 0 ||
           strcmp(r.key, "fuel_level") == 0 || strcmp(r.key, "mains_freq") == 0 ||
           strcmp(r.key, "mains_van") == 0 || strcmp(r.key, "mains_vbn") == 0 ||
           strcmp(r.key, "mains_vcn") == 0 || strcmp(r.key, "mains_vab") == 0 ||
           strcmp(r.key, "mains_vbc") == 0 || strcmp(r.key, "mains_vca") == 0;
  };

  Serial.print(F("## DSE poll ("));
  Serial.print(mode);
  Serial.println(')');
  size_t n = 0;
  for (size_t i = 0; i < DseRegisters::count(); i++) {
    const DseRegister &r = DseRegisters::at(i);
    if (!want(r)) {
      continue;
    }
    readAndPrintRegister(r);
    n++;
    delay(20);
  }
  Serial.print(F("Polled "));
  Serial.print(n);
  Serial.println(F(" registers."));
}

bool Cli::batchReadPrint(uint16_t address, uint16_t quantity) {
  ModbusTxnInfo info = makeTxnInfo();
  info.address = address;
  info.quantity = quantity;
  info.quiet = true;
  const ModbusResponse resp =
      modbus.readRegisters(_slaveId, 0x03, address, quantity, &info);
  if (resp.result != ModbusResult::Ok) {
    Serial.print(F("BATCH FAIL @"));
    Serial.print(address);
    Serial.print(F(" qty="));
    Serial.print(quantity);
    Serial.print(F(" result="));
    Serial.println(static_cast<int>(resp.result));
    return false;
  }
  DseRegisters::printBatchReadable(address, resp.values, resp.valueCount);
  return true;
}

void Cli::cmdIterate(char *args) {
  // iterate [mode]
  // iterate <n> [mode]
  char *tok1 = strtok(args, " \t");
  char *tok2 = strtok(nullptr, " \t");
  int loops = 1;
  const char *mode = "basic";

  if (tok1) {
    char *end = nullptr;
    const long n = strtol(tok1, &end, 10);
    if (end && *end == '\0' && n >= 1) {
      loops = static_cast<int>(n > 50 ? 50 : n);
      mode = tok2 ? tok2 : "basic";
    } else {
      mode = tok1;
    }
  }

  if (strcasecmp(mode, "basic") != 0 && strcasecmp(mode, "power") != 0 &&
      strcasecmp(mode, "engine") != 0 && strcasecmp(mode, "all") != 0) {
    Serial.println(F("Usage: iterate [basic|power|engine|all]"));
    Serial.println(F("       iterate <n> [basic|power|engine|all]"));
    return;
  }
  if (!ensureDseSelected()) {
    return;
  }

  for (int pass = 0; pass < loops; pass++) {
    if (loops > 1) {
      Serial.print(F("## Iterate "));
      Serial.print(pass + 1);
      Serial.print('/');
      Serial.println(loops);
    } else {
      Serial.print(F("## DSE iterate ("));
      Serial.print(mode);
      Serial.print(F(")  slave="));
      Serial.print(_slaveId);
      Serial.print(F("  baud="));
      Serial.println(_baudRate);
    }

    // Contiguous FC03 batches (max 32 regs). Quiet TX/RX; readable lines only.
    if (strcasecmp(mode, "basic") == 0 || strcasecmp(mode, "engine") == 0 ||
        strcasecmp(mode, "all") == 0) {
      // Identity + mode: 768..775
      batchReadPrint(768, 8);
      delay(30);
      // Engine + generator instrumentation: 1024..1055 (32 regs)
      batchReadPrint(1024, 32);
      delay(30);
      // Phase C power + utility L-N and L-L: 1056..1071
      batchReadPrint(1056, 16);
      delay(30);
      // Engine status. Qty 1 replies are often torn; the 16-register
      // window around 1408 is stable and still covers 1408 and 1418.
      batchReadPrint(1408, 16);
      delay(30);
    }

    if (strcasecmp(mode, "power") == 0 || strcasecmp(mode, "all") == 0) {
      // Derived power / PF block: 1536..1558
      batchReadPrint(1536, 23);
      delay(30);
      // Accumulators: 1798..1809
      batchReadPrint(1798, 12);
      delay(30);
    }

    if (strcasecmp(mode, "engine") == 0 || strcasecmp(mode, "all") == 0) {
      // Extended levels
      batchReadPrint(1357, 3);
      delay(30);
    }

    if (pass + 1 < loops) {
      delay(500);
    }
  }
}

bool Cli::waitOrAbort(uint32_t ms) {
  const uint32_t t0 = millis();
  while ((millis() - t0) < ms) {
    if (Serial.available()) {
      while (Serial.available()) {
        Serial.read();
      }
      return true;
    }
    delay(20);
  }
  return false;
}

void Cli::watchOnce() {
  uint8_t ok = 0;
  uint8_t fail = 0;

  Serial.println(F("--- COMM ---"));
  Serial.print(F("Slave "));
  Serial.print(_slaveId);
  Serial.print(F("  baud "));
  Serial.print(_baudRate);
  Serial.print(F(" 8N1  FC03"));
  Serial.println();

  auto batch = [&](uint16_t addr, uint16_t qty) {
    if (batchReadPrint(addr, qty)) {
      ok++;
    } else {
      fail++;
    }
    delay(25);
  };

  Serial.println(F("--- REGS (batch) ---"));
  batch(768, 8);      // identity / op_mode
  batch(1024, 32);    // engine + gen electrical
  batch(1056, 16);    // phase C + mains L-N and L-L
  batch(1408, 16);    // engine status (short qty-1 replies tear)
  batch(1536, 23);    // power / PF
  batch(1798, 12);    // hours / energy / starts

  Serial.println(F("--- SCF (read flags only, no writes) ---"));
  {
    ModbusTxnInfo info = makeTxnInfo();
    info.quiet = true;
    info.address = DseScf::kRegFlagsBase;
    info.quantity = DseScf::kFlagRegCount;
    const ModbusResponse resp = modbus.readRegisters(
        _slaveId, 0x03, DseScf::kRegFlagsBase, DseScf::kFlagRegCount, &info);
    if (resp.result != ModbusResult::Ok ||
        resp.valueCount < DseScf::kFlagRegCount) {
      Serial.println(F("SCF flags: FAIL"));
      fail++;
    } else {
      ok++;
      Serial.print(F("Flags:"));
      for (uint8_t i = 0; i < DseScf::kFlagRegCount; i++) {
        Serial.print(' ');
        Serial.print(F("0x"));
        Serial.print(resp.values[i], HEX);
      }
      Serial.println();
      Serial.println(F("Supported (read):"));
      for (size_t i = 0; i < DseScf::count(); i++) {
        const DseScfCommand &c = DseScf::at(i);
        if (DseScf::isSupported(c.code, resp.values)) {
          Serial.print(F("  [+] "));
          Serial.println(c.key);
        }
      }
    }
  }

  Serial.println(F("--- ALARMS (active only) ---"));
  {
    ModbusTxnInfo info = makeTxnInfo();
    info.quiet = true;
    info.address = 39425;
    info.quantity = 21;
    const ModbusResponse resp =
        modbus.readRegisters(_slaveId, 0x03, 39425, 21, &info);
    if (resp.result != ModbusResult::Ok) {
      Serial.println(F("Alarm page 154: FAIL"));
      fail++;
    } else {
      ok++;
      for (uint8_t i = 0; i < resp.valueCount; i++) {
        DseRegisters::printAlarmActive(
            static_cast<uint16_t>(39425 + i), resp.values[i]);
      }
    }
  }

  Serial.print(F("Batches OK="));
  Serial.print(ok);
  Serial.print(F(" FAIL="));
  Serial.println(fail);
  if (fail == 0) {
    Serial.println(F("LINK: OK"));
  } else {
    Serial.println(F("LINK: DEGRADED"));
  }
}

void Cli::cmdWatch(char *args) {
  char *tok = strtok(args, " \t");
  int loops = 8;
  if (tok) {
    const long n = strtol(tok, nullptr, 10);
    if (n >= 1 && n <= 40) {
      loops = static_cast<int>(n);
    } else {
      Serial.println(F("Usage: watch [n]   (n=1..40, default 8)"));
      Serial.println(F("       abort: type any key"));
      return;
    }
  }
  if (!ensureDseSelected()) {
    return;
  }

  Serial.println(F("## WATCH loop — comm / regs / scf-flags / alarms"));
  Serial.println(F("    Type any key to abort."));
  for (int pass = 0; pass < loops; pass++) {
    Serial.println();
    Serial.print(F("======== cycle "));
    Serial.print(pass + 1);
    Serial.print('/');
    Serial.print(loops);
    Serial.println(F(" ========"));
    watchOnce();
    if (pass + 1 < loops) {
      Serial.println(F("(next in 3s, any key aborts)"));
      if (waitOrAbort(3000)) {
        Serial.println(F("WATCH aborted."));
        return;
      }
    }
  }
  Serial.println(F("WATCH complete."));
}

void Cli::cmdAlarms(char *args) {
  (void)args;
  if (!ensureDseSelected()) {
    return;
  }
  Serial.println(F("## Active DSE alarms"));
  size_t n = 0;
  for (size_t i = 0; i < DseRegisters::count(); i++) {
    const DseRegister &r = DseRegisters::at(i);
    if (r.kind != DseDecodeKind::AlarmPacked) {
      continue;
    }
    ModbusTxnInfo info = makeTxnInfo();
    info.address = r.address;
    info.quantity = 1;
    const ModbusResponse resp =
        modbus.readRegisters(_slaveId, 0x03, r.address, 1, &info);
    if (resp.result != ModbusResult::Ok || resp.valueCount < 1) {
      Serial.print(F("FAIL: "));
      Serial.println(r.key);
      continue;
    }
    DseRegisters::printAlarmActive(r.address, resp.values[0]);
    n++;
    delay(20);
  }
  Serial.print(F("Checked "));
  Serial.print(n);
  Serial.println(F(" alarm registers."));
}

void Cli::cmdScf(char *args) {
  char *tok = strtok(args, " \t");
  if (!tok || strcasecmp(tok, "list") == 0 || strcasecmp(tok, "help") == 0) {
    DseScf::printList();
    return;
  }
  if (!ensureDseSelected()) {
    return;
  }

  if (strcasecmp(tok, "flags") == 0) {
    ModbusTxnInfo info = makeTxnInfo();
    info.address = DseScf::kRegFlagsBase;
    info.quantity = DseScf::kFlagRegCount;
    const ModbusResponse resp = modbus.readRegisters(
        _slaveId, 0x03, DseScf::kRegFlagsBase, DseScf::kFlagRegCount, &info);
    if (resp.result != ModbusResult::Ok ||
        resp.valueCount < DseScf::kFlagRegCount) {
      Serial.println(F("FAIL: could not read SCF support flags"));
      return;
    }
    Serial.println(F("## SCF support flags (4096-4103)"));
    for (uint8_t i = 0; i < DseScf::kFlagRegCount; i++) {
      Serial.print(F("  scf_f"));
      Serial.print(i);
      Serial.print(F(" @"));
      Serial.print(DseScf::kRegFlagsBase + i);
      Serial.print(F(" = 0x"));
      Serial.println(resp.values[i], HEX);
    }
    Serial.println(F("Supported commands:"));
    for (size_t i = 0; i < DseScf::count(); i++) {
      const DseScfCommand &c = DseScf::at(i);
      const bool ok = DseScf::isSupported(c.code, resp.values);
      Serial.print(ok ? F("  [+] ") : F("  [-] "));
      Serial.print(c.key);
      Serial.print(F("  "));
      Serial.println(c.name);
    }
    return;
  }

  uint16_t keyCode = 0;
  const char *label = nullptr;
  if (strcasecmp(tok, "raw") == 0) {
    char *num = strtok(nullptr, " \t");
    if (!num) {
      Serial.println(F("Usage: scf raw <key>"));
      return;
    }
    keyCode = static_cast<uint16_t>(strtoul(num, nullptr, 0));
    label = "raw";
  } else {
    const DseScfCommand *cmd = DseScf::findByKey(tok);
    if (!cmd) {
      Serial.println(F("ERROR: unknown SCF command (try 'scf list')"));
      return;
    }
    keyCode = cmd->code;
    label = cmd->key;
  }

  if (keyCode == DseScf::kKeyStop ||
      (label && strcasecmp(label, "stop") == 0)) {
    Serial.println(F("BLOCKED: scf stop is disabled while the generator is running."));
    Serial.println(F("         (read-only SCF: use 'scf flags' / 'watch')"));
    return;
  }

  Serial.println(F("WARNING: SCF write can change controller mode / start DG."));
  Serial.print(F("SCF WRITE "));
  Serial.print(label);
  Serial.print(F(" key="));
  Serial.print(keyCode);
  Serial.print(F(" complement="));
  Serial.println(static_cast<uint16_t>(65535u - keyCode));

  // Do not read the support flags in this command. On this 7320 the first
  // FC16 sent right after that FC03 is ignored (silence). `scf flags` still
  // reads them. A second write is only used when the first one is silent.
  ModbusTxnInfo info = makeTxnInfo();
  info.address = DseScf::kRegKey;
  info.quantity = 2;
  ModbusResponse wr;
  for (int attempt = 0; attempt < 3; attempt++) {
    if (attempt > 0) {
      delay(50);
    }
    wr = DseScf::writeKey(_slaveId, keyCode, &info);
    if (wr.result == ModbusResult::Ok || wr.result == ModbusResult::Exception) {
      break;
    }
  }
  if (wr.result == ModbusResult::Ok) {
    Serial.println(F("SCF write ACK OK"));
  } else {
    Serial.print(F("SCF write FAILED result="));
    Serial.print(static_cast<int>(wr.result));
    if (wr.result == ModbusResult::Exception) {
      Serial.print(F(" exception="));
      Serial.print(wr.exceptionCode);
    }
    Serial.println();
  }
}

void Cli::cmdWrite(char *args, bool single) {
  char *addrStr = strtok(args, " \t");
  if (!addrStr) {
    Serial.println(single ? F("Usage: write06 <addr> <word>")
                          : F("Usage: write16 <addr> <word> [word...]"));
    return;
  }
  const long address = strtol(addrStr, nullptr, 0);
  uint16_t values[16];
  uint16_t n = 0;
  while (n < (single ? 1 : 16)) {
    char *w = strtok(nullptr, " \t");
    if (!w) {
      break;
    }
    values[n++] = static_cast<uint16_t>(strtoul(w, nullptr, 0));
  }
  if (address < 0 || address > 0xFFFF || n == 0 || (single && n != 1)) {
    Serial.println(F("ERROR: bad write args"));
    return;
  }
  ModbusTxnInfo info = makeTxnInfo();
  info.address = static_cast<uint16_t>(address);
  info.quantity = n;
  const ModbusResponse wr =
      single ? modbus.writeSingleRegister(_slaveId, info.address, values[0],
                                           &info)
             : modbus.writeMultipleRegisters(_slaveId, info.address, values, n,
                                              &info);
  if (wr.result == ModbusResult::Ok) {
    Serial.println(F("WRITE ACK OK"));
  } else {
    Serial.print(F("WRITE FAILED result="));
    Serial.println(static_cast<int>(wr.result));
  }
}

void Cli::cmdRead(char *args, bool hexAddress) {
  char *addrStr = strtok(args, " \t");
  char *qtyStr = strtok(nullptr, " \t");
  if (!addrStr || !qtyStr) {
    Serial.println(hexAddress ? F("Usage: readhex <hex> <qty>")
                              : F("Usage: read <addr> <qty>"));
    return;
  }
  const long address = strtol(addrStr, nullptr, hexAddress ? 16 : 10);
  const long quantity = strtol(qtyStr, nullptr, 10);
  if (address < 0 || address > 0xFFFF || quantity < 1 || quantity > 32) {
    Serial.println(F("ERROR: bad address/quantity"));
    return;
  }
  ModbusTxnInfo info = makeTxnInfo();
  info.address = static_cast<uint16_t>(address);
  info.quantity = static_cast<uint16_t>(quantity);
  const ModbusResponse resp = modbus.readRegisters(
      _slaveId, _functionCode, static_cast<uint16_t>(address),
      static_cast<uint16_t>(quantity), &info);
  if (resp.result == ModbusResult::Ok &&
      strcasecmp(devices.selected().id, "dse7320") == 0) {
    DseRegisters::annotateRead(static_cast<uint16_t>(address),
                               static_cast<uint16_t>(quantity), resp.values,
                               resp.valueCount);
  }
}

void Cli::cmdRawRead(char *args) {
  char *slaveStr = strtok(args, " \t");
  char *fcStr = strtok(nullptr, " \t");
  char *addrStr = strtok(nullptr, " \t");
  char *qtyStr = strtok(nullptr, " \t");
  if (!slaveStr || !fcStr || !addrStr || !qtyStr) {
    Serial.println(F("Usage: rawread <slave> <fc> <hex_addr> <qty>"));
    return;
  }
  if (modbus.diagnostic()) {
    Serial.println(F("RAW COMMAND TOKENS (rawread):"));
    Serial.print(F("TOKEN 0: "));
    Serial.println(slaveStr);
    Serial.print(F("TOKEN 1: "));
    Serial.println(fcStr);
    Serial.print(F("TOKEN 2: "));
    Serial.println(addrStr);
    Serial.print(F("TOKEN 3: "));
    Serial.println(qtyStr);
  }
  const long slave = strtol(slaveStr, nullptr, 0);
  const long fc = strtol(fcStr, nullptr, 16);
  const long address = strtol(addrStr, nullptr, 16);
  const long quantity = strtol(qtyStr, nullptr, 10);
  if (modbus.diagnostic()) {
    Serial.print(F("PARSED: slave="));
    Serial.print(slave);
    Serial.print(F(" function="));
    Serial.print(fc, HEX);
    Serial.print(F(" address="));
    Serial.print(address, HEX);
    Serial.print(F(" quantity="));
    Serial.println(quantity);
  }
  if (slave < 1 || slave > 247 || (fc != 0x03 && fc != 0x04) ||
      address < 0 || address > 0xFFFF || quantity < 1 || quantity > 32) {
    Serial.println(F("ERROR: bad rawread args"));
    return;
  }
  ModbusTxnInfo info = makeTxnInfo();
  info.deviceName = "rawread";
  modbus.readRegisters(static_cast<uint8_t>(slave), static_cast<uint8_t>(fc),
                       static_cast<uint16_t>(address),
                       static_cast<uint16_t>(quantity), &info);
}

void Cli::cmdRs485Raw(char *args) {
  char *slaveStr = strtok(args, " \t");
  char *fcStr = strtok(nullptr, " \t");
  char *addrStr = strtok(nullptr, " \t");
  char *qtyStr = strtok(nullptr, " \t");
  if (!slaveStr || !fcStr || !addrStr || !qtyStr) {
    Serial.println(F("Usage: rs485raw <slave> <fc> <hex_addr> <qty>"));
    return;
  }
  const long slave = strtol(slaveStr, nullptr, 0);
  const long fc = strtol(fcStr, nullptr, 16);
  const long address = strtol(addrStr, nullptr, 16);
  const long quantity = strtol(qtyStr, nullptr, 10);
  if (slave < 1 || slave > 247 || (fc != 0x03 && fc != 0x04) ||
      address < 0 || address > 0xFFFF || quantity < 1 || quantity > 32) {
    Serial.println(F("ERROR: bad rs485raw args"));
    return;
  }
  modbus.rawCapture(static_cast<uint8_t>(slave), static_cast<uint8_t>(fc),
                    static_cast<uint16_t>(address),
                    static_cast<uint16_t>(quantity));
}

void Cli::cmdRs485RawRepeat(char *args) {
  char *nStr = strtok(args, " \t");
  char *slaveStr = strtok(nullptr, " \t");
  char *fcStr = strtok(nullptr, " \t");
  char *addrStr = strtok(nullptr, " \t");
  char *qtyStr = strtok(nullptr, " \t");
  if (!nStr || !slaveStr || !fcStr || !addrStr || !qtyStr) {
    Serial.println(
        F("Usage: rs485raw-repeat <n> <slave> <fc> <hex_addr> <qty>"));
    return;
  }
  const long n = strtol(nStr, nullptr, 10);
  const long slave = strtol(slaveStr, nullptr, 0);
  const long fc = strtol(fcStr, nullptr, 16);
  const long address = strtol(addrStr, nullptr, 16);
  const long quantity = strtol(qtyStr, nullptr, 10);
  if (n < 1 || n > 100 || slave < 1 || slave > 247 ||
      (fc != 0x03 && fc != 0x04) || address < 0 || address > 0xFFFF ||
      quantity < 1 || quantity > 32) {
    Serial.println(F("ERROR: bad rs485raw-repeat args"));
    return;
  }

  uint16_t zero = 0, five = 0, other = 0, crcOk = 0, crcBad = 0;
  for (long i = 0; i < n; i++) {
    Serial.print(F("--- rs485raw-repeat "));
    Serial.print(i + 1);
    Serial.print('/');
    Serial.println(n);
    const RawRs485Capture cap = modbus.rawCapture(
        static_cast<uint8_t>(slave), static_cast<uint8_t>(fc),
        static_cast<uint16_t>(address), static_cast<uint16_t>(quantity));
    if (cap.rxLen == 0) {
      zero++;
    } else if (cap.rxLen == 5) {
      five++;
      if (modbusCrcOk(cap.rx, 5)) {
        crcOk++;
      } else {
        crcBad++;
      }
    } else {
      other++;
      if (cap.rxLen >= 4 && modbusCrcOk(cap.rx, cap.rxLen)) {
        crcOk++;
      } else {
        crcBad++;
      }
    }
    delay(50);
  }

  Serial.println();
  Serial.println(F("## RS485 RAW REPEAT SUMMARY"));
  Serial.print(F("Attempts: "));
  Serial.println(n);
  Serial.print(F("0-byte timeout: "));
  Serial.println(zero);
  Serial.print(F("5-byte response: "));
  Serial.println(five);
  Serial.print(F("other-length response: "));
  Serial.println(other);
  Serial.print(F("CRC valid: "));
  Serial.println(crcOk);
  Serial.print(F("CRC invalid: "));
  Serial.println(crcBad);
}

void Cli::cmdRepeat(char *args) {
  char *nStr = strtok(args, " \t");
  char *slaveStr = strtok(nullptr, " \t");
  char *fcStr = strtok(nullptr, " \t");
  char *addrStr = strtok(nullptr, " \t");
  char *qtyStr = strtok(nullptr, " \t");
  if (!nStr || !slaveStr || !fcStr || !addrStr || !qtyStr) {
    Serial.println(F("Usage: repeat <n> <slave> <fc> <hex_addr> <qty>"));
    return;
  }

  const long n = strtol(nStr, nullptr, 10);
  const long slave = strtol(slaveStr, nullptr, 0);
  const long fc = strtol(fcStr, nullptr, 16);
  const long address = strtol(addrStr, nullptr, 16);
  const long quantity = strtol(qtyStr, nullptr, 10);
  if (n < 1 || n > 100 || slave < 1 || slave > 247 ||
      (fc != 0x03 && fc != 0x04) || address < 0 || address > 0xFFFF ||
      quantity < 1 || quantity > 32) {
    Serial.println(F("ERROR: bad repeat args (n=1..100)"));
    return;
  }

  uint16_t ok = 0, exc = 0, crc = 0, partial = 0, timeout = 0, unexp = 0,
           other = 0;
  ModbusTxnInfo info = makeTxnInfo();
  info.deviceName = "repeat";

  // Temporarily quieter? Keep full diagnostic per request for Phase 2A.
  for (long i = 0; i < n; i++) {
    Serial.print(F("--- repeat "));
    Serial.print(i + 1);
    Serial.print(F("/"));
    Serial.println(n);
    const ModbusResponse r = modbus.readRegisters(
        static_cast<uint8_t>(slave), static_cast<uint8_t>(fc),
        static_cast<uint16_t>(address), static_cast<uint16_t>(quantity),
        &info);
    switch (r.result) {
      case ModbusResult::Ok:
        ok++;
        break;
      case ModbusResult::Exception:
        exc++;
        break;
      case ModbusResult::CrcError:
        crc++;
        break;
      case ModbusResult::Partial:
        partial++;
        break;
      case ModbusResult::Timeout:
        timeout++;
        break;
      case ModbusResult::UnexpectedFunction:
        unexp++;
        break;
      default:
        other++;
        break;
    }
    delay(50);
  }

  Serial.println();
  Serial.println(F("## MODBUS REPEAT TEST"));
  Serial.print(F("Requests: "));
  Serial.println(n);
  Serial.print(F("Valid responses: "));
  Serial.println(ok);
  Serial.print(F("Valid exceptions: "));
  Serial.println(exc);
  Serial.print(F("CRC errors: "));
  Serial.println(crc);
  Serial.print(F("Partial frames: "));
  Serial.println(partial);
  Serial.print(F("Timeouts: "));
  Serial.println(timeout);
  Serial.print(F("Unexpected function: "));
  Serial.println(unexp);
  Serial.print(F("Other errors: "));
  Serial.println(other);
}

namespace {

bool frameStable(const ModbusResponse &a, const ModbusResponse &b) {
  if (a.rawRxLen < 3 || a.rawRxLen != b.rawRxLen) {
    return false;
  }
  return memcmp(a.rawRx, b.rawRx, a.rawRxLen) == 0;
}

bool illegalAddressShape(const ModbusResponse &r, uint8_t slave, uint8_t fc) {
  return r.rawRxLen >= 3 && r.rawRx[0] == slave &&
         r.rawRx[1] == static_cast<uint8_t>(fc | 0x80) && r.rawRx[2] == 0x02;
}

void printRaw(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
    if (i + 1 < len) {
      Serial.print(' ');
    }
  }
}

}  // namespace

void Cli::cmdDcem(char *args) {
  (void)args;
  const uint8_t slave = _slaveId;
#if defined(ARDUINO_ARCH_ESP32)
  WiFi.mode(WIFI_OFF);
  Serial.println(F("WiFi radio OFF for this scan (reset the board to bring the AP back)"));
#endif
  Serial.println(F("## ET1641 FORMAT SEARCH"));
  Serial.print(F("Slave "));
  Serial.println(slave);

  struct Trial {
    uint32_t baud;
    uint8_t parity;
    const char *name;
  };
  const Trial trials[] = {
      {9600, 0, "9600 8N1"},
      {9600, 2, "9600 8E1"},
      {9600, 1, "9600 8O1"},
      {19200, 0, "19200 8N1"},
      {19200, 2, "19200 8E1"},
      {4800, 0, "4800 8N1"},
  };

  bool formatWon = false;
  for (size_t t = 0; t < sizeof(trials) / sizeof(trials[0]); t++) {
    _baudRate = trials[t].baud;
    rs485.applyConfig(trials[t].baud, 8, trials[t].parity, 1);
    delay(20);
    uint8_t pass = 0;
    ModbusTxnInfo info = makeTxnInfo();
    info.quiet = true;
    info.deviceName = "dcem";
    for (uint8_t n = 0; n < 3; n++) {
      const ModbusResponse r =
          modbus.readRegisters(slave, 0x04, 0, 1, &info);
      if (r.result == ModbusResult::Ok || r.result == ModbusResult::Exception) {
        pass++;
      }
    }
    Serial.print(trials[t].name);
    Serial.print(F(": CRC-valid "));
    Serial.print(pass);
    Serial.println(F("/3"));
    if (pass > 0) {
      formatWon = true;
      Serial.println(F("FORMAT LOCKED"));
      break;
    }
  }
  if (!formatWon) {
    _baudRate = 9600;
    rs485.applyConfig(9600, 8, 0, 1);
    Serial.println(F("No CRC-valid header yet. Scanning addresses at 9600 8N1."));
  }

  static const uint16_t kExtra[] = {48, 50, 64, 100, 128, 200, 256, 512,
                                     1000, 1024, 4000, 4096, 4100, 4101};
  uint16_t pass = 0;
  uint16_t stableData = 0;
  uint16_t illegal = 0;
  uint16_t unstable = 0;
  uint16_t none = 0;

  Serial.println(F("## ET1641 ADDRESS SCAN (qty 2, two reads)"));
  for (uint8_t fc = 0x03; fc <= 0x04; fc++) {
    for (uint16_t addr = 0; addr < 48; addr++) {
      ModbusTxnInfo info = makeTxnInfo();
      info.quiet = true;
      info.deviceName = "dcem";
      const ModbusResponse a =
          modbus.readRegisters(slave, fc, addr, 2, &info);
      const ModbusResponse b =
          modbus.readRegisters(slave, fc, addr, 2, &info);
      const bool ok = a.result == ModbusResult::Ok ||
                      a.result == ModbusResult::Exception ||
                      b.result == ModbusResult::Ok ||
                      b.result == ModbusResult::Exception;
      const bool stable = frameStable(a, b);
      if (ok) {
        pass++;
        Serial.print(F("CRC PASS fc "));
        if (fc < 0x10) {
          Serial.print('0');
        }
        Serial.print(fc, HEX);
        Serial.print(F(" addr "));
        Serial.print(addr);
        Serial.print(F(" rx "));
        printRaw(a.rawRx, a.rawRxLen);
        Serial.println();
        if (a.result == ModbusResult::Ok) {
          Serial.print(F("  value0=0x"));
          Serial.println(a.values[0], HEX);
        }
      } else if (stable && illegalAddressShape(a, slave, fc)) {
        illegal++;
      } else if (stable && a.rawRxLen >= 3) {
        stableData++;
        Serial.print(F("STABLE fc "));
        if (fc < 0x10) {
          Serial.print('0');
        }
        Serial.print(fc, HEX);
        Serial.print(F(" addr "));
        Serial.print(addr);
        Serial.print(F(" len "));
        Serial.print(a.rawRxLen);
        Serial.print(F(" rx "));
        printRaw(a.rawRx, a.rawRxLen);
        Serial.println();
      } else if (a.rawRxLen == 0 && b.rawRxLen == 0) {
        none++;
      } else {
        unstable++;
      }
      if ((addr & 15) == 0) {
        yield();
      }
    }
  }

  for (size_t i = 0; i < sizeof(kExtra) / sizeof(kExtra[0]); i++) {
    for (uint8_t fc = 0x03; fc <= 0x04; fc++) {
      const uint16_t addr = kExtra[i];
      ModbusTxnInfo info = makeTxnInfo();
      info.quiet = true;
      info.deviceName = "dcem";
      const ModbusResponse a =
          modbus.readRegisters(slave, fc, addr, 2, &info);
      const ModbusResponse b =
          modbus.readRegisters(slave, fc, addr, 2, &info);
      const bool ok = a.result == ModbusResult::Ok ||
                      a.result == ModbusResult::Exception ||
                      b.result == ModbusResult::Ok ||
                      b.result == ModbusResult::Exception;
      if (ok) {
        pass++;
        Serial.print(F("CRC PASS fc "));
        Serial.print(fc, HEX);
        Serial.print(F(" addr "));
        Serial.print(addr);
        Serial.print(F(" rx "));
        printRaw(a.rawRx, a.rawRxLen);
        Serial.println();
      } else if (frameStable(a, b) && a.rawRxLen >= 3 &&
                 !illegalAddressShape(a, slave, fc)) {
        stableData++;
        Serial.print(F("STABLE fc "));
        Serial.print(fc, HEX);
        Serial.print(F(" addr "));
        Serial.print(addr);
        Serial.print(F(" rx "));
        printRaw(a.rawRx, a.rawRxLen);
        Serial.println();
      } else if (frameStable(a, b) && illegalAddressShape(a, slave, fc)) {
        illegal++;
      } else if (a.rawRxLen == 0 && b.rawRxLen == 0) {
        none++;
      } else {
        unstable++;
      }
    }
  }

  Serial.println(F("## ET1641 SCAN SUMMARY"));
  Serial.print(F("CRC-valid frames: "));
  Serial.println(pass);
  Serial.print(F("Stable non-exception payloads: "));
  Serial.println(stableData);
  Serial.print(F("Stable illegal-address exceptions: "));
  Serial.println(illegal);
  Serial.print(F("Unstable frames: "));
  Serial.println(unstable);
  Serial.print(F("No reply: "));
  Serial.println(none);
  Serial.print(F("UART now: "));
  Serial.print(rs485.baud());
  Serial.print(rs485.parity() == 1 ? F(" 8O1")
                                   : (rs485.parity() == 2 ? F(" 8E1") : F(" 8N1")));
  Serial.println();
}

void Cli::cmdEnvolt(char *args) {
  uint8_t slave = 63;
  if (args && args[0]) {
    const long id = strtol(args, nullptr, 0);
    if (id >= 1 && id <= 247) {
      slave = static_cast<uint8_t>(id);
    }
  }
  _slaveId = slave;
#if defined(ARDUINO_ARCH_ESP32)
  WiFi.mode(WIFI_OFF);
  Serial.println(F("WiFi radio OFF for this scan"));
#endif
  Serial.println(F("## EN-VOLT SCAN"));
  Serial.print(F("Slave "));
  Serial.println(slave);

  // This meter answers in ~15 ms. A short miss-timeout keeps a full
  // address sweep from stalling a second on every empty address.
  const uint32_t tmo = 200;
  uint8_t rx[64];

  auto printHex = [](const uint8_t *f, size_t n) {
    for (size_t i = 0; i < n; i++) {
      if (f[i] < 0x10) {
        Serial.print('0');
      }
      Serial.print(f[i], HEX);
      if (i + 1 < n) {
        Serial.print(' ');
      }
    }
    Serial.println();
  };
  auto printWords = [](const uint8_t *f, size_t n) {
    if (n < 7) {
      return;
    }
    uint16_t index = 0;
    for (size_t i = 3; i + 1 < n - 2; i += 2) {
      const uint16_t word =
          static_cast<uint16_t>((f[i] << 8) | f[i + 1]);
      Serial.print(F("  ["));
      Serial.print(index++);
      Serial.print(F("]="));
      Serial.print(word);
      Serial.print(F(" 0x"));
      Serial.println(word, HEX);
    }
  };
  auto wordDiffs = [](const uint8_t *a, const uint8_t *b, size_t n) -> uint8_t {
    uint8_t diffs = 0;
    if (n < 7) {
      return 255;
    }
    for (size_t i = 3; i + 1 < n - 2; i += 2) {
      if (a[i] != b[i] || a[i + 1] != b[i + 1]) {
        diffs++;
      }
    }
    return diffs;
  };

  Serial.println(F("-- FC04 addr 0 qty 1"));
  size_t n = modbus.exchange(slave, 0x04, 0, 1, rx, sizeof(rx), tmo);
  Serial.print(F("len "));
  Serial.print(n);
  Serial.print((n >= 5 && modbusCrcOk(rx, n)) ? F(" CRC PASS ") : F(" CRC FAIL "));
  printHex(rx, n);

  static uint8_t images[6][64];
  static uint8_t imageLen[6];
  static uint16_t imageAddr[6];
  uint8_t imageCount = 0;
  // 0 silent, 1..6 data image, 0x80|code exception
  static uint8_t status[256];
  memset(status, 0, sizeof(status));
  const uint16_t span = 256;

  Serial.println(F("-- FC03 addr 0..255 qty 1"));
  for (uint16_t addr = 0; addr < span; addr++) {
    n = modbus.exchange(slave, 0x03, addr, 1, rx, sizeof(rx), tmo);
    delay(40);
    if (n < 5 || !modbusCrcOk(rx, n)) {
      continue;
    }
    if (n == 5 && (rx[1] & 0x80)) {
      status[addr] = static_cast<uint8_t>(0x80 | rx[2]);
      continue;
    }
    int slot = -1;
    for (uint8_t s = 0; s < imageCount; s++) {
      if (imageLen[s] == n && wordDiffs(images[s], rx, n) <= 2) {
        slot = s;
        break;
      }
    }
    if (slot < 0 && imageCount < 6) {
      slot = imageCount++;
      memcpy(images[slot], rx, n);
      imageLen[slot] = static_cast<uint8_t>(n);
      imageAddr[slot] = addr;
      Serial.print(F("DATA addr "));
      Serial.print(addr);
      Serial.print(F(" len "));
      Serial.print(n);
      Serial.print(F(" count-field "));
      Serial.println(rx[2]);
      printHex(rx, n);
      printWords(rx, n);
    } else if (slot < 0) {
      Serial.print(F("DATA addr "));
      Serial.print(addr);
      Serial.print(F(" len "));
      Serial.print(n);
      Serial.print(' ');
      printHex(rx, n);
    }
    if (slot >= 0) {
      status[addr] = static_cast<uint8_t>(slot + 1);
    }
  }

  auto dumpCode = [&](uint8_t code, const __FlashStringHelper *label) {
    bool in = false;
    uint16_t start = 0;
    uint16_t count = 0;
    for (uint16_t addr = 0; addr <= span; addr++) {
      const bool on = addr < span && status[addr] == code;
      if (on && !in) {
        start = addr;
        in = true;
      }
      if (!on && in) {
        Serial.print(label);
        Serial.print(' ');
        Serial.print(start);
        if (addr - 1 != start) {
          Serial.print('-');
          Serial.print(addr - 1);
        }
        Serial.print(F(" ("));
        Serial.print(addr - start);
        Serial.println(')');
        count += static_cast<uint16_t>(addr - start);
        in = false;
      }
    }
    if (count) {
      Serial.print(label);
      Serial.print(F(" total "));
      Serial.println(count);
    }
  };
  Serial.println(F("-- ranges"));
  dumpCode(0, F("silent"));
  dumpCode(0x82, F("exc02 illegal-address"));
  dumpCode(0x83, F("exc03 illegal-value"));
  dumpCode(0x81, F("exc01 illegal-function"));
  for (uint8_t s = 0; s < imageCount; s++) {
    Serial.print(F("image "));
    Serial.print(s);
    dumpCode(static_cast<uint8_t>(s + 1), F(""));
  }

  Serial.println(F("-- quantity at each new address"));
  const uint16_t qtys[] = {1, 2, 8, 16};
  for (uint8_t s = 0; s < imageCount; s++) {
    for (size_t q = 0; q < sizeof(qtys) / sizeof(qtys[0]); q++) {
      n = modbus.exchange(slave, 0x03, imageAddr[s], qtys[q], rx, sizeof(rx),
                          tmo);
      delay(40);
      const bool crc = n >= 5 && modbusCrcOk(rx, n);
      Serial.print(F("image "));
      Serial.print(s);
      Serial.print(F(" addr "));
      Serial.print(imageAddr[s]);
      Serial.print(F(" qty "));
      Serial.print(qtys[q]);
      Serial.print(F(" len "));
      Serial.print(n);
      Serial.print(crc ? F(" CRC PASS") : F(" CRC FAIL"));
      if (crc) {
        Serial.print(F(" count-field "));
        Serial.print(rx[2]);
        if (n == imageLen[s]) {
          Serial.print(F(" word-diffs "));
          Serial.print(wordDiffs(images[s], rx, n));
        }
      }
      Serial.println();
    }
  }

  Serial.println(F("-- reread data"));
  for (uint8_t s = 0; s < imageCount; s++) {
    if (imageLen[s] < 7) {
      continue;
    }
    n = modbus.exchange(slave, 0x03, imageAddr[s], 1, rx, sizeof(rx), tmo);
    delay(40);
    if (n != imageLen[s] || !modbusCrcOk(rx, n)) {
      Serial.print(F("addr "));
      Serial.print(imageAddr[s]);
      Serial.println(F(" miss"));
      continue;
    }
    const uint8_t diffs = wordDiffs(images[s], rx, n);
    if (diffs == 0) {
      Serial.print(F("addr "));
      Serial.print(imageAddr[s]);
      Serial.println(F(" steady"));
      continue;
    }
    Serial.print(F("addr "));
    Serial.print(imageAddr[s]);
    Serial.print(F(" live"));
    for (size_t i = 3; i + 1 < n - 2; i += 2) {
      if (rx[i] == images[s][i] && rx[i + 1] == images[s][i + 1]) {
        continue;
      }
      const uint16_t word =
          static_cast<uint16_t>((rx[i] << 8) | rx[i + 1]);
      Serial.print(F(" ["));
      Serial.print((i - 3) / 2);
      Serial.print(F("]="));
      Serial.print(word);
    }
    Serial.println();
  }
  Serial.println(F("-- FC04 again"));
  n = modbus.exchange(slave, 0x04, 0, 1, rx, sizeof(rx), tmo);
  Serial.print(F("len "));
  Serial.print(n);
  Serial.print((n >= 5 && modbusCrcOk(rx, n)) ? F(" CRC PASS ") : F(" CRC FAIL "));
  printHex(rx, n);
  Serial.println(F("EN-VOLT DONE"));
}

void Cli::handleLine(char *line) {
  while (*line == ' ' || *line == '\t') {
    line++;
  }
  if (*line == '\0') {
    return;
  }

  char *cmd = strtok(line, " \t");
  char *rest = strtok(nullptr, "");
  if (rest) {
    while (*rest == ' ' || *rest == '\t') {
      rest++;
    }
  }
  char empty[] = "";
  if (!rest) {
    rest = empty;
  }

  if (modbus.diagnostic()) {
    Serial.print(F("RAW COMMAND: "));
    Serial.println(line);
    Serial.print(F("TOKEN CMD: "));
    Serial.println(cmd);
    if (rest[0]) {
      Serial.print(F("TOKEN REST: "));
      Serial.println(rest);
    }
  }

  if (dispatchShell(cmd, rest)) {
    return;
  }

  if (strcasecmp(cmd, "help") == 0 || strcasecmp(cmd, "?") == 0) {
    printHelp();
  } else if (strcasecmp(cmd, "status") == 0) {
    printStatus();
  } else if (strcasecmp(cmd, "devices") == 0) {
    devices.printList();
  } else if (strcasecmp(cmd, "device") == 0) {
    cmdDevice(rest);
  } else if (strcasecmp(cmd, "regs") == 0 ||
             strcasecmp(cmd, "registers") == 0) {
    cmdRegs(rest);
  } else if (strcasecmp(cmd, "get") == 0) {
    cmdGet(rest);
  } else if (strcasecmp(cmd, "poll") == 0) {
    cmdPoll(rest);
  } else if (strcasecmp(cmd, "iterate") == 0) {
    cmdIterate(rest);
  } else if (strcasecmp(cmd, "watch") == 0 || strcasecmp(cmd, "loop") == 0) {
    cmdWatch(rest);
  } else if (strcasecmp(cmd, "alarms") == 0) {
    cmdAlarms(rest);
  } else if (strcasecmp(cmd, "scf") == 0) {
    cmdScf(rest);
  } else if (strcasecmp(cmd, "write16") == 0) {
    cmdWrite(rest, false);
  } else if (strcasecmp(cmd, "write06") == 0) {
    cmdWrite(rest, true);
  } else if (strcasecmp(cmd, "read") == 0) {
    cmdRead(rest, false);
  } else if (strcasecmp(cmd, "readhex") == 0) {
    cmdRead(rest, true);
  } else if (strcasecmp(cmd, "rawread") == 0) {
    cmdRawRead(rest);
  } else if (strcasecmp(cmd, "rs485raw") == 0) {
    cmdRs485Raw(rest);
  } else if (strcasecmp(cmd, "rs485raw-repeat") == 0) {
    cmdRs485RawRepeat(rest);
  } else if (strcasecmp(cmd, "set") == 0) {
    cmdSet(rest);
  } else if (strcasecmp(cmd, "crc-test") == 0 ||
             strcasecmp(cmd, "crctest") == 0) {
    modbus.runCrcSelfTest();
  } else if (strcasecmp(cmd, "diagnostic") == 0) {
    cmdDiagnostic(rest);
  } else if (strcasecmp(cmd, "repeat") == 0) {
    cmdRepeat(rest);
  } else if (strcasecmp(cmd, "dcem") == 0) {
    cmdDcem(rest);
  } else if (strcasecmp(cmd, "envolt") == 0) {
    cmdEnvolt(rest);
#if defined(ARDUINO_ARCH_ESP32)
  } else if (strcasecmp(cmd, "uplink") == 0) {
    if (strcasecmp(rest, "now") == 0) {
      Uplink::kick();
      Serial.println(F("Uplink will send the current batch on the next quiet loop."));
      Uplink::printStatus();
    } else if (strcasecmp(rest, "heartbeat") == 0 || strcasecmp(rest, "ping") == 0) {
      Uplink::heartbeat();
    } else {
      Uplink::printStatus();
    }
#endif
  } else {
    Serial.print(F("Unknown command: "));
    Serial.println(cmd);
  }
}

namespace {

char *trimEdge(char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  char *e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t')) {
    *--e = '\0';
  }
  return s;
}

void wipe(char *s) {
  if (!s) {
    return;
  }
  memset(s, 0, strlen(s));
}

}  // namespace

void Cli::adoptSettings() {
#if defined(ARDUINO_ARCH_ESP32)
  const GatewayConfig &c = Settings::get();
  _slaveId = c.slave;
  _baudRate = c.baud;
  _functionCode = c.fc;
#else
#endif
}

bool Cli::persistSetting(const char *key, const char *value) {
#if !defined(ARDUINO_ARCH_ESP32)
  (void)key;
  (void)value;
  return false;
#else
  char err[80];
  err[0] = '\0';
  if (!Settings::put(key, value, err, sizeof(err))) {
    if (err[0]) {
      Serial.print(F("WARNING: "));
      Serial.println(err);
    }
    return false;
  }
  adoptSettings();
  return true;
#endif
}

bool Cli::secretMode() const {
  return _mode == ConsoleMode::LoginPass || _mode == ConsoleMode::AskCur ||
         _mode == ConsoleMode::AskNew || _mode == ConsoleMode::AskConfirm;
}

bool Cli::sessionOpen() const {
  return _mode != ConsoleMode::LoginUser && _mode != ConsoleMode::LoginPass;
}

void Cli::buildPrompt(char *out, size_t n) const {
  if (!out || n == 0) {
    return;
  }
#if defined(ARDUINO_ARCH_ESP32)
  switch (_mode) {
    case ConsoleMode::LoginUser:
      strncpy(out, "login: ", n - 1);
      break;
    case ConsoleMode::LoginPass:
      strncpy(out, "Password: ", n - 1);
      break;
    case ConsoleMode::AskCur:
      strncpy(out, "Current password: ", n - 1);
      break;
    case ConsoleMode::AskNew:
      strncpy(out, "New password: ", n - 1);
      break;
    case ConsoleMode::AskConfirm:
      strncpy(out, "Retype new password: ", n - 1);
      break;
    case ConsoleMode::Editor:
      strncpy(out, "edit> ", n - 1);
      break;
    case ConsoleMode::Shell:
      snprintf(out, n, "%s@%s:%s$ ", Auth::user(), Settings::hostname(),
               Vfs::cwd());
      break;
  }
#else
  strncpy(out, "> ", n - 1);
  (void)_mode;
#endif
  out[n - 1] = '\0';
}

void Cli::showPrompt() {
  char prompt[96];
  buildPrompt(prompt, sizeof(prompt));
  tty.setPrompt(prompt);
  tty.setSecret(secretMode());
  tty.setEofLogsOut(_mode == ConsoleMode::Shell);
  tty.printPrompt();
}

void Cli::cancelEntry() {
  memset(_pwNew, 0, sizeof(_pwNew));
  if (_mode == ConsoleMode::LoginPass) {
    memset(_loginUser, 0, sizeof(_loginUser));
    _mode = ConsoleMode::LoginUser;
    return;
  }
  if (_mode == ConsoleMode::AskCur || _mode == ConsoleMode::AskNew ||
      _mode == ConsoleMode::AskConfirm) {
    _addingUser = false;
    memset(_loginUser, 0, sizeof(_loginUser));
    _mode = ConsoleMode::Shell;
    Serial.println(F("Cancelled."));
  }
}

void Cli::dropSession(bool tell) {
#if defined(ARDUINO_ARCH_ESP32)
  if (tell && Editor::dirty()) {
    Serial.println(F("Editor closed without saving."));
  }
  Editor::close();
  Auth::logout();
  _addingUser = false;
  memset(_pwNew, 0, sizeof(_pwNew));
  memset(_loginUser, 0, sizeof(_loginUser));
  _mode = ConsoleMode::LoginUser;
  if (tell) {
    Serial.println(F("Logged out."));
  }
#else
  (void)tell;
#endif
}

void Cli::onSubmitted(char *line) {
#if !defined(ARDUINO_ARCH_ESP32)
  if (line && line[0]) {
    tty.remember(line);
  }
  handleLine(line);
  return;
#else
  switch (_mode) {
    case ConsoleMode::LoginUser: {
      char *user = trimEdge(line);
      if (!user[0]) {
        return;
      }
      if (strlen(user) > 15) {
        delay(400);
        Serial.println(F("Login incorrect."));
        return;
      }
      strncpy(_loginUser, user, sizeof(_loginUser) - 1);
      _loginUser[sizeof(_loginUser) - 1] = '\0';
      _mode = ConsoleMode::LoginPass;
      return;
    }
    case ConsoleMode::LoginPass: {
      const bool ok = Auth::login(_loginUser, line);
      wipe(line);
      if (!ok) {
        memset(_loginUser, 0, sizeof(_loginUser));
        _mode = ConsoleMode::LoginUser;
        delay(400);
        Serial.println(F("Login incorrect."));
        return;
      }
      _mode = ConsoleMode::Shell;
      _lastInputMs = millis();
      printBrandIntro();
      Serial.print(F("Logged in as "));
      Serial.println(Auth::user());
      Serial.print(F("Hostname: "));
      Serial.println(Settings::hostname());
      WifiPortal::printBrief();
      if (Auth::factoryPassword()) {
        Serial.println(F("Factory password is still set. Run: passwd"));
      }
      Serial.println();
      Vfs::showMotd();
      Serial.println(F("Type help for commands."));
      Serial.println(F("View: cat motd     cat conf"));
      Serial.println(F("Edit: edit motd    edit conf     then wq to save"));
      Serial.println(F("Reboot: reboot"));
      return;
    }
    case ConsoleMode::AskCur: {
      const bool ok = Auth::check(Auth::user(), line);
      wipe(line);
      if (!ok) {
        Serial.println(F("Authentication failure."));
        _mode = ConsoleMode::Shell;
        return;
      }
      _mode = ConsoleMode::AskNew;
      return;
    }
    case ConsoleMode::AskNew: {
      const size_t n = strlen(line);
      if (n < 4 || n > 32) {
        wipe(line);
        Serial.println(F("Password must be 4..32 characters."));
        return;
      }
      memcpy(_pwNew, line, n);
      _pwNew[n] = '\0';
      wipe(line);
      _mode = ConsoleMode::AskConfirm;
      return;
    }
    case ConsoleMode::AskConfirm: {
      const bool match = strcmp(line, _pwNew) == 0;
      wipe(line);
      if (!match) {
        memset(_pwNew, 0, sizeof(_pwNew));
        Serial.println(F("Passwords do not match."));
        _mode = ConsoleMode::AskNew;
        return;
      }
      char err[72];
      err[0] = '\0';
      const bool ok = _addingUser
                          ? Auth::addUser(_loginUser, _pwNew, err, sizeof(err))
                          : Auth::setPassword(Auth::user(), _pwNew, err,
                                              sizeof(err));
      memset(_pwNew, 0, sizeof(_pwNew));
      _addingUser = false;
      memset(_loginUser, 0, sizeof(_loginUser));
      _mode = ConsoleMode::Shell;
      if (!ok) {
        Serial.println(err[0] ? err : "Could not update password.");
      } else {
        Serial.println(F("Password updated."));
      }
      return;
    }
    case ConsoleMode::Editor:
      if (!Editor::onLine(line)) {
        _mode = ConsoleMode::Shell;
      }
      return;
    case ConsoleMode::Shell:
      if (line[0] && !strContainsIgnoreCase(line, "pass")) {
        tty.remember(line);
      }
      handleLine(line);
      return;
  }
#endif
}

void Cli::onHostDown() { dropSession(false); }

void Cli::poll() {
  // Nested calls come from Telemetry's idle hook during Modbus refresh.
  // Echo keystrokes immediately, but defer Enter / logout until the outer
  // poll so a command cannot start a second bus transaction mid-refresh.
  if (_inPoll) {
    char line[Tty::kCap];
    const Tty::Event ev = tty.poll(line, sizeof(line));
    if (ev == Tty::Event::None) {
      return;
    }
    _lastInputMs = millis();
    if (ev == Tty::Event::Activity) {
      return;
    }
    if (ev == Tty::Event::Interrupt) {
      _deferredInterrupt = true;
      return;
    }
    if (ev == Tty::Event::Logout) {
      _deferredLogout = true;
      return;
    }
    if (!_hasDeferredLine) {
      strncpy(_deferredLine, line, sizeof(_deferredLine) - 1);
      _deferredLine[sizeof(_deferredLine) - 1] = '\0';
      _hasDeferredLine = true;
    }
    return;
  }

  _inPoll = true;
#if defined(ARDUINO_ARCH_ESP32)
  if (sessionOpen()) {
    const uint16_t idleMin = Settings::get().idleMin;
    if (idleMin > 0 &&
        (millis() - _lastInputMs) > static_cast<uint32_t>(idleMin) * 60000UL) {
      Serial.println();
      Serial.println(F("Session idle, logged out."));
      dropSession(false);
      showPrompt();
      _lastInputMs = millis();
    }
  }
#endif
  char line[Tty::kCap];
  Tty::Event ev = tty.poll(line, sizeof(line));
  if (ev == Tty::Event::None && _hasDeferredLine) {
    strncpy(line, _deferredLine, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';
    _hasDeferredLine = false;
    _deferredLine[0] = '\0';
    ev = Tty::Event::Line;
  } else if (ev == Tty::Event::None && _deferredInterrupt) {
    _deferredInterrupt = false;
    ev = Tty::Event::Interrupt;
  } else if (ev == Tty::Event::None && _deferredLogout) {
    _deferredLogout = false;
    ev = Tty::Event::Logout;
  }
  if (ev == Tty::Event::None) {
    _inPoll = false;
    return;
  }
  _lastInputMs = millis();
  if (ev == Tty::Event::Activity) {
    _inPoll = false;
    return;
  }
  if (ev == Tty::Event::Interrupt) {
    cancelEntry();
    showPrompt();
    _inPoll = false;
    return;
  }
  if (ev == Tty::Event::Logout) {
    dropSession(true);
    showPrompt();
    _inPoll = false;
    return;
  }
  onSubmitted(line);
  showPrompt();
  _inPoll = false;
}
