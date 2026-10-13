#include "settings.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "bitcoin.h"
Settings settings;
bool validateSettings(const Settings& s) {
  if (s.ssid.isEmpty() || s.ssid.length() > 32 || s.password.length() > 63 ||
      (!s.password.isEmpty() && s.password.length() < 8) || s.pool.isEmpty() ||
      s.pool.length() > 253 || !s.port || s.poolPassword.length() > 64 || s.worker.isEmpty() ||
      s.worker.length() > 24 || !btc::addressValid(s.address.c_str()))
    return false;
  for (char c : s.pool)
    if (!isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-')
      return false;
  for (char c : s.worker)
    if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
      return false;
  return true;
}
bool saveSettings(const Settings& s) {
  if (!validateSettings(s))
    return false;
  DynamicJsonDocument d(2048);
  d["v"] = 1;
  d["ssid"] = s.ssid;
  d["password"] = s.password;
  d["pool"] = s.pool;
  d["port"] = s.port;
  d["poolPassword"] = s.poolPassword;
  d["address"] = s.address;
  d["worker"] = s.worker;
  String blob;
  serializeJson(d, blob);
  Preferences p;
  if (!p.begin("lottery", false))
    return false;
  bool ok = p.putString("config", blob) == blob.length();
  p.end();
  return ok;
}
bool loadSettings() {
  Preferences p;
  if (!p.begin("lottery", true))
    return false;
  String blob = p.getString("config", "");
  p.end();
  if (blob.length() > 2048)
    return false;
  DynamicJsonDocument d(2048);
  if (deserializeJson(d, blob) || d["v"] != 1 || !d["port"].is<unsigned>() ||
      d["port"].as<unsigned>() > 65535)
    return false;
  const char* fields[] = {"ssid", "password", "pool", "poolPassword", "address", "worker"};
  for (auto f : fields)
    if (!d[f].is<const char*>())
      return false;
  Settings s;
  s.ssid = d["ssid"].as<String>();
  s.password = d["password"].as<String>();
  s.pool = d["pool"].as<String>();
  s.poolPassword = d["poolPassword"].as<String>();
  s.address = d["address"].as<String>();
  s.worker = d["worker"].as<String>();
  s.port = d["port"].as<unsigned>();
  if (!validateSettings(s))
    return false;
  settings = s;
  return true;
}
bool resetSettings() {
  Preferences p;
  if (!p.begin("lottery", false))
    return false;
  bool ok = p.clear();
  p.end();
  return ok;
}
