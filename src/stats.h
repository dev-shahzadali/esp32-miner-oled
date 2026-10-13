#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
struct Statistics {
  uint64_t hashes = 0, uptime = 0;
  uint32_t accepted = 0, rejected = 0, stale = 0, dropped = 0, lastJob = 0, lastCommunication = 0;
  double best = 0, khps = 0, average = 0;
  double poolDifficulty = 0;
  uint32_t blockedJobs = 0, blockCandidates = 0;
  bool payoutVerified = false;
  bool wifi = false, pool = false, mining = false, portal = false;
  uint32_t heap = 0, minHeap = 0;
  int rssi = 0;
  char status[32] = "Starting";
  char setupKey[17] = {};
};
extern Statistics stats;
extern SemaphoreHandle_t statsMutex;
struct StatsLock {
  StatsLock() {
    xSemaphoreTake(statsMutex, portMAX_DELAY);
  }
  ~StatsLock() {
    xSemaphoreGive(statsMutex);
  }
};
void status(const char*);
Statistics snapshot();
