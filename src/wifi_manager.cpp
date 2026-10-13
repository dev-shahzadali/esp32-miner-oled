#include "wifi_manager.h"

#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

#include "settings.h"
#include "stats.h"
static WebServer server(IPAddress(192, 168, 4, 1), 80);
static bool active = false;
static String key, token;
static uint32_t wifiStart = 0, lastRetry = 0;
static String randomKey() {
  uint8_t b[8];
  esp_fill_random(b, 8);
  String s;
  const char* h = "0123456789abcdef";
  for (auto v : b) {
    s += h[v >> 4];
    s += h[v & 15];
  }
  return s;
}
bool portalActive() {
  return active;
}
void showSetupKey() {
  if (active)
    Serial.printf("One-time setup key (Wi-Fi and HTTP admin): %s\n", key.c_str());
  else
    Serial.println("Run setup first");
}
static bool localRequest() {
  IPAddress ip = server.client().remoteIP();
  return ip[0] == 192 && ip[1] == 168 && ip[2] == 4;
}
static bool authorized() {
  if (!localRequest()) {
    server.send(403, "text/plain", "AP access required");
    return false;
  }
  if (!server.authenticate("admin", key.c_str())) {
    server.requestAuthentication();
    return false;
  }
  return true;
}
void startPortal() {
  if (active)
    return;
  active = true;
  key = randomKey();
  token = randomKey();
  WiFi.softAP("ESP32-LotteryMiner", key.c_str(), 1, false, 2);
  {
    StatsLock lock;
    stats.portal = true;
    strlcpy(stats.setupKey, key.c_str(), sizeof(stats.setupKey));
  }
  status("Setup: 192.168.4.1");
  Serial.println(
      "Setup AP ESP32-LotteryMiner at 192.168.4.1; HTTP user admin. Key on OLED; enter setup-key "
      "over USB if needed.");
  server.on("/", HTTP_GET, [] {
    if (!authorized())
      return;
    String page =
        F("<!doctype html><meta name='viewport' content='width=device-width'><title>ESP32 Lottery "
          "Miner</title><h1>Miner setup</h1><p>Use a public Bitcoin mainnet receiving address "
          "only. Never enter a seed phrase or private key.</p><form method='post' "
          "action='/save'><input type='hidden' name='token' value='");
    page += token;
    page += F(
        "'><p>Wi-Fi SSID <input name='ssid' maxlength='32' required></p><p>Wi-Fi password <input "
        "name='password' type='password' maxlength='63'></p><p>Pool host <input name='pool' "
        "value='public-pool.io' maxlength='253' required></p><p>Pool port <input name='port' "
        "type='number' value='21496' min='1' max='65535' required></p><p>Pool password <input "
        "name='poolPassword' value='x' maxlength='64' type='password'></p><p>Bitcoin receiving "
        "address <input name='address' maxlength='90' required></p><p>Worker <input name='worker' "
        "value='esp32' maxlength='24' required></p><button>Save and restart</button></form><p>Pool "
        "authorization confirms address support. No credentials are displayed by this page.</p>");
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Content-Security-Policy",
                      "default-src 'none'; form-action 'self'; base-uri 'none'");
    server.send(200, "text/html", page);
  });
  server.on("/save", HTTP_POST, [] {
    if (!authorized())
      return;
    if (server.arg("token") != token) {
      server.send(403, "text/plain", "Invalid setup token");
      return;
    }
    Settings s;
    s.ssid = server.arg("ssid");
    s.password = server.arg("password");
    s.pool = server.arg("pool");
    s.poolPassword = server.arg("poolPassword");
    s.address = server.arg("address");
    s.worker = server.arg("worker");
    String port = server.arg("port");
    bool digits = !port.isEmpty() && port.length() <= 5;
    for (char c : port)
      if (c < '0' || c > '9')
        digits = false;
    long n = port.toInt();
    if (!digits || n < 1 || n > 65535) {
      server.send(400, "text/plain", "Invalid port");
      return;
    }
    s.port = n;
    if (!validateSettings(s)) {
      server.send(400, "text/plain",
                  "Invalid settings: check Wi-Fi lengths, mainnet address checksum, host, and "
                  "worker characters.");
      return;
    }
    if (!saveSettings(s)) {
      server.send(500, "text/plain", "NVS write failed");
      return;
    }
    server.send(200, "text/plain", "Saved. Restarting; reconnect to your normal Wi-Fi.");
    delay(300);
    ESP.restart();
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}
void servicePortal() {
  if (active)
    server.handleClient();
}
void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
  wifiStart = lastRetry = millis();
  status("Connecting Wi-Fi");
}
void serviceWifi() {
  bool up = WiFi.status() == WL_CONNECTED;
  {
    StatsLock lock;
    stats.wifi = up;
    stats.rssi = up ? WiFi.RSSI() : 0;
  }
  if (up) {
    wifiStart = millis();
    return;
  }
  uint32_t now = millis();
  if (now - lastRetry >= 15000) {
    lastRetry = now;
    WiFi.disconnect();
    WiFi.begin(settings.ssid.c_str(), settings.password.c_str());
    Serial.println("Wi-Fi retry");
  }
  if (now - wifiStart >= 60000)
    startPortal();
}
