#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>

#include "mining.h"
#include "oled.h"
#include "settings.h"
#include "stats.h"
#include "stratum.h"
#include "wifi_manager.h"
static bool configured = false;
void setup() {
  Serial.begin(115200);
  delay(100);
  statsMutex = xSemaphoreCreateMutex();
  configASSERT(statsMutex);
  initMining();
  Serial.printf("ESP32 Lottery Miner 1.1 payout-guard; %u cores; flash %u bytes; heap %u\n",
                ESP.getChipCores(), ESP.getFlashChipSize(), ESP.getFreeHeap());
  esp_task_wdt_init(15, true);
  esp_task_wdt_add(nullptr);
  configASSERT(xTaskCreatePinnedToCore(oledWorker, "OLED", 4096, nullptr, 1, nullptr, 0) == pdPASS);
  if (ESP.getFlashChipSize() < 4 * 1024 * 1024) {
    status("Flash below 4MB");
    Serial.println("Unsupported flash size for this partition table");
    while (true) {
      esp_task_wdt_reset();
      delay(1000);
    }
  }
  if (!selfTest()) {
    status("SHA self-test FAIL");
    Serial.println("FATAL SHA self-test failed; mining disabled");
    while (true) {
      esp_task_wdt_reset();
      delay(1000);
    }
  }
  Serial.println("SHA reference, optimized and mbedTLS self-tests PASS");
  benchmarkHashing();
  configured = loadSettings();
  if (configured)
    startWifi();
  else {
    WiFi.mode(WIFI_AP);
    startPortal();
  }
  configASSERT(xTaskCreatePinnedToCore(networkWorker, "Stratum", 16384, nullptr, 2, nullptr, 0) ==
               pdPASS);
  for (unsigned i = 0; i < MINER_WORKERS; i++)
    configASSERT(xTaskCreatePinnedToCore(miningWorker, "SHA256d", 4096,
                                         reinterpret_cast<void*>(uintptr_t(i)), 1, nullptr,
                                         i) == pdPASS);
  Serial.println("Serial commands: setup | setup-key | reset-config | stats (Enter terminates)");
}
void loop() {
  esp_task_wdt_reset();
  if (configured)
    serviceWifi();
  servicePortal();
  static String command;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      command.trim();
      if (command == "setup")
        startPortal();
      else if (command == "setup-key")
        showSetupKey();
      else if (command == "reset-config") {
        if (resetSettings()) {
          Serial.println("Configuration cleared; restarting");
          delay(100);
          ESP.restart();
        } else
          Serial.println("NVS reset failed");
      } else if (command == "stats") {
        auto s = snapshot();
        Serial.printf(
            "hashes=%llu rate=%.3f KH/s avg=%.3f accepted=%u rejected=%u stale=%u dropped=%u "
            "best=%.8g uptime=%llu wifi=%d pool=%d jobAgeMs=%u rxAgeMs=%u heap=%u minHeap=%u\n",
            s.hashes, s.khps, s.average, s.accepted, s.rejected, s.stale, s.dropped, s.best,
            s.uptime, s.wifi, s.pool, uint32_t(millis() - s.lastJob),
            uint32_t(millis() - s.lastCommunication), s.heap, s.minHeap);
        Serial.printf(
            "payout_verified=%d blocked_jobs=%u pool_difficulty=%.12g network_candidates=%u\n",
            s.payoutVerified, s.blockedJobs, s.poolDifficulty, s.blockCandidates);
      }
      command = "";
    } else if (command.length() < 32)
      command += c;
    else
      command = "";
  }
  delay(10);
}
