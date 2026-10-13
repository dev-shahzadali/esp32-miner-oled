#include "mining.h"

#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <mbedtls/sha256.h>

#include <atomic>
#include <cstring>

#include "nerdSHA256plus.h"
#include "stats.h"
QueueHandle_t shareQueue;
static SemaphoreHandle_t workMutex;
static Work work;
static std::atomic<uint32_t> generation{0};
static bool exhausted[MINER_WORKERS] = {};
void initMining() {
  workMutex = xSemaphoreCreateMutex();
  shareQueue = xQueueCreate(16, sizeof(Share));
  configASSERT(workMutex && shareQueue);
}
void stopMining() {
  xSemaphoreTake(workMutex, portMAX_DELAY);
  work.active = false;
  work.generation = ++generation;
  xSemaphoreGive(workMutex);
  StatsLock lock;
  stats.mining = false;
}
void publishWork(const Work& w) {
  xSemaphoreTake(workMutex, portMAX_DELAY);
  work = w;
  work.generation = ++generation;
  for (auto& x : exhausted) x = false;
  xSemaphoreGive(workMutex);
  StatsLock lock;
  stats.mining = true;
}
uint32_t workGeneration() {
  return generation.load();
}
bool workExhausted() {
  xSemaphoreTake(workMutex, portMAX_DELAY);
  bool done = work.active;
  for (auto x : exhausted) done &= x;
  xSemaphoreGive(workMutex);
  return done;
}
void miningWorker(void* arg) {
  unsigned core = unsigned(reinterpret_cast<uintptr_t>(arg));
  esp_task_wdt_add(nullptr);
  Work local;
  uint64_t nonce = core;
  uint32_t mid[8], bake[16];
  alignas(4) uint8_t hash[32];
  for (;;) {
    if (local.generation != generation.load() || !local.active) {
      xSemaphoreTake(workMutex, portMAX_DELAY);
      local = work;
      xSemaphoreGive(workMutex);
      nonce = core;
      if (local.active) {
        nerd_mids(mid, local.header.data());
        nerd_sha256_bake(mid, local.header.data() + 64, bake);
      }
    }
    if (!local.active || nonce > UINT32_MAX) {
      esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    unsigned count = 0;
    double best = 0;
    for (unsigned i = 0; i < 256 && nonce <= UINT32_MAX; i++, nonce += MINER_WORKERS) {
      if ((i & 31) == 0 && local.generation != generation.load())
        break;
      btc::putLE(local.header.data() + 76, uint32_t(nonce));
      nerd_sha256d_baked(mid, local.header.data() + 64, bake, hash);
      count++;
      double d = btc::difficulty(hash);
      if (d > best)
        best = d;
      const bool blockCandidate = btc::meets(hash, local.networkTarget);
      if (btc::meets(hash, local.target) || blockCandidate) {
        // Independently verify every candidate with the general implementation before submission.
        auto reference = btc::sha256d(local.header.data(), 80);
        if (memcmp(hash, reference.data(), 32)) {
          Serial.println("FATAL: SHA disagreement; restarting");
          ESP.restart();
        }
        if (blockCandidate) {
          StatsLock lock;
          stats.blockCandidates++;
          Serial.println(
              "Network-target candidate verified locally; awaiting pool/network validation");
        }
        Share share{};
        share.generation = local.generation;
        share.nonce = nonce;
        memcpy(share.id, local.id, sizeof(share.id));
        memcpy(share.time, local.time, 9);
        memcpy(share.ex2, local.ex2, 33);
        if (xQueueSend(shareQueue, &share, 0) != pdTRUE) {
          StatsLock lock;
          stats.dropped++;
        }
      }
    }
    {
      StatsLock lock;
      stats.hashes += count;
      if (best > stats.best)
        stats.best = best;
    }
    if (nonce > UINT32_MAX) {
      xSemaphoreTake(workMutex, portMAX_DELAY);
      if (work.generation == local.generation)
        exhausted[core] = true;
      xSemaphoreGive(workMutex);
    }
    esp_task_wdt_reset();
    vTaskDelay(1);  // Allow IDLE watchdog, Wi-Fi, TCP, portal and display to run.
  }
}
bool selfTest() {
  uint8_t hash[32];
  btc::sha256(reinterpret_cast<const uint8_t*>("abc"), 3, hash);
  if (btc::hex(hash, 32) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    return false;
  std::vector<uint8_t> bytes;
  btc::unhex(
      "0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac7"
      "2c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c",
      bytes);
  auto reference = btc::sha256d(bytes.data(), 80);
  if (btc::hex(reference.data(), 32) !=
      "6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000")
    return false;
  alignas(4) uint8_t h[80];
  memcpy(h, bytes.data(), 80);
  uint32_t mid[8], bake[16];
  nerd_mids(mid, h);
  nerd_sha256_bake(mid, h + 64, bake);
  for (unsigned n = 0; n < 32; n++) {
    btc::putLE(h + 76, n);
    nerd_sha256d_baked(mid, h + 64, bake, hash);
    reference = btc::sha256d(h, 80);
    if (memcmp(hash, reference.data(), 32))
      return false;
    uint8_t accelerated[32];
    if (mbedtls_sha256_ret(h, 80, accelerated, 0))
      return false;
    btc::sha256(h, 80, hash);
    if (memcmp(hash, accelerated, 32))
      return false;
  }
  return true;
}
void benchmarkHashing() {
  alignas(4) uint8_t header[80] = {}, hash[32], intermediate[32];
  uint32_t mid[8], bake[16];
  nerd_mids(mid, header);
  nerd_sha256_bake(mid, header + 64, bake);
  constexpr unsigned count = 1024;
  int64_t start = esp_timer_get_time();
  for (unsigned n = 0; n < count; n++) {
    btc::putLE(header + 76, n);
    nerd_sha256d_baked(mid, header + 64, bake, hash);
  }
  int64_t sw = esp_timer_get_time() - start;
  start = esp_timer_get_time();
  for (unsigned n = 0; n < count; n++) {
    btc::putLE(header + 76, n);
    mbedtls_sha256_ret(header, 80, intermediate, 0);
    mbedtls_sha256_ret(intermediate, 32, hash, 0);
  }
  int64_t hw = esp_timer_get_time() - start;
  Serial.printf(
      "Startup SHA256d benchmark: optimized=%.2f KH/s; mbedTLS=%.2f KH/s (%u real hashes per "
      "backend, one core, no network/UI overhead)\n",
      count * 1000.0 / sw, count * 1000.0 / hw, count);
  Serial.println(
      "Mining uses optimized software on independent cores; compare esp32-single and esp32-oled "
      "rolling rates for sustained throughput.");
}
