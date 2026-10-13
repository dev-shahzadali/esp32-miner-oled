#include "oled.h"

#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>

#include "stats.h"
void oledWorker(void*) {
  esp_task_wdt_add(nullptr);
  Wire.begin(21, 22);
  Wire.setClock(400000);
  Wire.setTimeOut(50);
  Wire.beginTransmission(0x3c);
  Adafruit_SSD1306 oled(128, 64, &Wire, -1, 400000, 400000);
  bool present =
      Wire.endTransmission() == 0 && oled.begin(SSD1306_SWITCHCAPVCC, 0x3c, false, false);
  if (!present)
    Serial.println("OLED absent at 0x3C; mining continues");
  uint64_t previousHashes = 0;
  int64_t previousTime = esp_timer_get_time();
  double samples[30] = {};
  unsigned index = 0, used = 0;
  for (;;) {
    esp_task_wdt_reset();
    int64_t now = esp_timer_get_time();
    auto s = snapshot();
    double rate = double(s.hashes - previousHashes) * 1000.0 / double(now - previousTime);
    previousHashes = s.hashes;
    previousTime = now;
    samples[index++ % 30] = rate;
    if (used < 30)
      used++;
    double sum = 0;
    for (unsigned i = 0; i < used; i++) sum += samples[i];
    {
      StatsLock lock;
      stats.khps = rate;
      stats.average = sum / used;
      stats.uptime = now / 1000000;
      stats.heap = ESP.getFreeHeap();
      stats.minHeap = ESP.getMinFreeHeap();
    }
    if (present) {
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE);
      oled.setTextWrap(false);
      oled.setCursor(0, 0);
      oled.println("NERDMINER ESP32");
      oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);
      oled.setCursor(0, 12);
      if (s.portal) {
        oled.println("ESP32-LotteryMiner");
        oled.println("192.168.4.1");
        oled.println("Key (Wi-Fi + admin):");
        oled.println(s.setupKey);
        oled.println("Save to restart");
      } else {
        uint64_t seconds = now / 1000000;
        if ((seconds / 8) % 2 == 0) {
          oled.printf("HASH %7.2f KH/s\n", rate);
          oled.printf("AVG  %7.2f KH/s\n", sum / used);
          oled.printf("WIFI:%s POOL:%s\n", s.wifi ? "ON" : "OFF", s.pool ? "ON" : "OFF");
          oled.printf("SHARES %u / %u\n", s.accepted, s.rejected);
          if (s.mining)
            oled.printf("BEST %.3g\n", s.best);
          else
            oled.printf("%.21s\n", s.status);
        } else {
          oled.printf("UP %llu:%02u:%02u\n", seconds / 3600, unsigned(seconds / 60 % 60),
                      unsigned(seconds % 60));
          oled.printf("STALE %u DROP %u\n", s.stale, s.dropped);
          oled.printf("RSSI %d PAY:%s\n", s.rssi, s.payoutVerified ? "OK" : "WAIT");
          oled.printf("HEAP %u KB\n", ESP.getFreeHeap() / 1024);
          oled.printf("%.21s\n", s.status);
        }
      }
      oled.display();  // Buffered refresh; no clear command sent to the panel.
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
