#pragma once
#include <Arduino.h>
struct Settings {
  String ssid, password, pool = "public-pool.io", poolPassword = "x", address, worker = "esp32";
  uint16_t port = 21496;
  String user() const {
    return address + "." + worker;
  }
};
extern Settings settings;
bool validateSettings(const Settings&);
bool loadSettings();
bool saveSettings(const Settings&);
bool resetSettings();
