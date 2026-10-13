#include "stratum.h"

#include <WiFi.h>
#include <esp_task_wdt.h>

#include <algorithm>
#include <array>

#include "mining.h"
#include "protocol.h"
#include "settings.h"
#include "stats.h"
namespace {
WiFiClient client;
btc::Backoff backoff;
btc::Job job;
btc::Hash target;
std::string extra1;
std::vector<uint8_t> extra2;
bool transportConnected = false, subscribed = false, authorized = false, haveJob = false,
     haveDifficulty = false;
double nextDifficulty = 1;
uint32_t connectedAt = 0, lastRx = 0, lastTx = 0, lastJob = 0, retryAt = 0;
unsigned nextId = 4;
std::string line;
DynamicJsonDocument doc(32768);
struct Pending {
  unsigned id = 0;
  uint32_t sent = 0;
};
std::array<Pending, 16> pending;
bool send(const std::string& s) {
  if (client.write(reinterpret_cast<const uint8_t*>(s.data()), s.size()) != s.size())
    return false;
  lastTx = millis();
  return true;
}
void disconnect(const char* why) {
  client.stop();
  transportConnected = false;
  stopMining();
  line.clear();
  subscribed = authorized = haveJob = haveDifficulty = false;
  extra1.clear();
  extra2.clear();
  {
    StatsLock lock;
    stats.pool = false;
    stats.payoutVerified = false;
    for (auto& p : pending)
      if (p.id) {
        stats.stale++;
        p.id = 0;
      }
  }
  retryAt = millis() + backoff.next() * 1000;
  status(why);
  Serial.println(why);
}
bool publish() {
  if (!subscribed || !authorized || !haveJob || !haveDifficulty)
    return true;
  // Increment full-width extranonce2 for every new template; never repeat a nonce range.
  bool carry = true;
  for (int i = int(extra2.size()) - 1; i >= 0 && carry; i--) {
    extra2[i]++;
    carry = extra2[i] == 0;
  }
  if (carry)
    return false;
  Work w;
  std::string ex = btc::hex(extra2.data(), extra2.size());
  std::vector<uint8_t> coinbase, walletScript;
  if (!btc::unhex(job.coinb1 + extra1 + ex + job.coinb2, coinbase, 4096) ||
      !btc::addressScript(settings.address.c_str(), walletScript))
    return false;
  const auto payout = btc::checkCoinbasePayout(coinbase, walletScript);
  if (!payout.verified) {
    stopMining();
    haveJob = false;
    {
      StatsLock lock;
      stats.payoutVerified = false;
      stats.blockedJobs++;
    }
    status("BLOCKED: payout mismatch");
    Serial.println(payout.valid ? "SECURITY: job blocked; reward does not fully pay saved wallet"
                                : "SECURITY: job blocked; malformed coinbase transaction");
    return true;
  }
  if (!btc::buildHeader(job, extra1, ex, w.header))
    return false;
  w.target = target;
  uint32_t bits = uint32_t(w.header[72]) | (uint32_t(w.header[73]) << 8) |
                  (uint32_t(w.header[74]) << 16) | (uint32_t(w.header[75]) << 24);
  if (!btc::compactTarget(bits, w.networkTarget))
    return false;
  {
    StatsLock lock;
    stats.payoutVerified = true;
  }
  w.active = true;
  strlcpy(w.id, job.id.c_str(), sizeof(w.id));
  strlcpy(w.time, job.time.c_str(), 9);
  strlcpy(w.ex2, ex.c_str(), 33);
  publishWork(w);
  status("Mining");
  return true;
}
bool handle() {
  doc.clear();
  if (deserializeJson(doc, line, DeserializationOption::NestingLimit(8)) || !doc.is<JsonObject>()) {
    Serial.println("Malformed Stratum JSON");
    return false;
  }
  auto method = doc["method"];
  if (method.is<const char*>()) {
    std::string m = method.as<const char*>();
    auto params = doc["params"].as<JsonVariantConst>();
    if (m == "mining.set_difficulty") {
      if (!params.is<JsonArrayConst>() || params.size() != 1 || !params[0].is<double>())
        return false;
      double d;
      btc::Hash t;
      if (!btc::parseDifficulty(line, d) || !btc::difficultyTarget(d, t))
        return false;
      nextDifficulty = d;
      haveDifficulty = true;  // Applies to NEXT job; existing jobs retain their target.
      return true;
    }
    if (m == "mining.notify") {
      btc::Job n;
      if (!btc::parseNotify(params, n))
        return false;
      stopMining();
      {
        StatsLock lock;
        stats.payoutVerified = false;
      }
      job = std::move(n);
      haveJob = true;
      lastJob = millis();
      if (!btc::difficultyTarget(nextDifficulty, target))
        return false;
      {
        StatsLock lock;
        stats.lastJob = lastJob;
        stats.poolDifficulty = nextDifficulty;
      }
      return publish();
    }
    if (m == "mining.set_extranonce")
      return false;  // Reconnect and resubscribe; discard old work safely.
    if (m == "client.reconnect")
      return false;  // Reconnect configured host; never follow an arbitrary server URL.
    return true;
  }
  if (!doc["id"].is<unsigned>())
    return true;
  unsigned id = doc["id"].as<unsigned>();
  bool error = !doc["error"].isNull();
  if (id == 1) {
    unsigned size;
    if (error || !btc::parseSubscribe(doc["result"], extra1, size))
      return false;
    extra2.assign(size, 0);
    subscribed = true;
    return publish();
  }
  if (id == 2) {
    if (!btc::successfulResponse(doc.as<JsonVariantConst>())) {
      Serial.println("Pool authorization refused; check receiving address and pool support");
      return false;
    }
    authorized = true;
    {
      StatsLock lock;
      stats.pool = true;
    }
    backoff.reset();
    return publish();
  }
  for (auto& p : pending)
    if (p.id == id) {
      StatsLock lock;
      if (btc::successfulResponse(doc.as<JsonVariantConst>())) {
        stats.accepted++;
        Serial.println("Pool accepted share (not a block discovery)");
      } else {
        stats.rejected++;
        Serial.println("Pool rejected share");
      }
      p.id = 0;
      break;
    }
  return true;
}
}  // namespace
void networkWorker(void*) {
  esp_task_wdt_add(nullptr);
  target.fill(0);
  line.reserve(btc::MAX_LINE);
  for (;;) {
    esp_task_wdt_reset();
    uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (transportConnected)
        disconnect("Wi-Fi lost");
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    if (!client.connected()) {
      if (transportConnected) {
        disconnect("Pool disconnected");
        continue;
      }
      if (int32_t(now - retryAt) < 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      status("Connecting pool");
      Serial.println("Connecting configured Stratum pool");
      client.setTimeout(2000);
      if (!client.connect(settings.pool.c_str(), settings.port, 3000)) {
        disconnect("Pool connect failed");
        continue;
      }
      transportConnected = true;
      client.setNoDelay(true);
      connectedAt = lastRx = lastTx = lastJob = millis();
      nextId = 4;
      nextDifficulty = 1;
      haveDifficulty = true;
      if (!send(btc::request(1, "mining.subscribe", {"ESP32LotteryMiner/1.0"})) ||
          !send(btc::request(2, "mining.authorize",
                             {settings.user().c_str(), settings.poolPassword.c_str()})) ||
          !send("{\"id\":3,\"method\":\"mining.suggest_difficulty\",\"params\":[0.0001]}\n")) {
        disconnect("Pool send failed");
        continue;
      }
    }
    bool failed = false;
    unsigned processed = 0;
    while (client.available() && processed++ < 2048) {
      int c = client.read();
      if (c < 0)
        break;
      if (c == '\n') {
        if (line.empty())
          continue;
        if (!handle()) {
          failed = true;
          break;
        }
        line.clear();
        lastRx = millis();
        {
          StatsLock lock;
          stats.lastCommunication = lastRx;
        }
      } else if (c != '\r') {
        if (line.size() >= btc::MAX_LINE) {
          failed = true;
          break;
        }
        line += char(c);
      }
    }
    if (failed) {
      disconnect("Invalid pool message");
      continue;
    }
    now = millis();
    if (btc::connectionTimedOut(now, connectedAt, lastRx, lastJob,
                                authorized && subscribed && haveJob && haveDifficulty)) {
      disconnect("Pool session timeout");
      continue;
    }
    if (now - lastTx > 30000) {
      char keepalive[128];
      snprintf(keepalive, sizeof(keepalive),
               "{\"id\":%u,\"method\":\"mining.suggest_difficulty\",\"params\":[0.0001]}\n",
               nextId++);
      if (!send(keepalive)) {
        disconnect("Pool keepalive failed");
        continue;
      }
    }
    bool expired = false;
    for (auto& p : pending)
      if (p.id && now - p.sent > 30000)
        expired = true;
    if (expired) {
      disconnect("Share response timeout");
      continue;
    }
    Share s;
    for (int i = 0; i < 16 && xQueueReceive(shareQueue, &s, 0) == pdTRUE; i++) {
      if (!authorized || s.generation != workGeneration()) {
        StatsLock lock;
        stats.stale++;
        continue;
      }
      auto slot =
          std::find_if(pending.begin(), pending.end(), [](const Pending& p) { return p.id == 0; });
      if (slot == pending.end()) {
        StatsLock lock;
        stats.dropped++;
        continue;
      }
      btc::Job j;
      j.id = s.id;
      j.time = s.time;
      unsigned id = nextId++;
      if (nextId < 4)
        nextId = 4;
      if (!send(btc::submission(id, settings.user().c_str(), j, s.ex2, s.nonce))) {
        disconnect("Share send failed");
        break;
      }
      slot->id = id;
      slot->sent = now;
    }
    if (workExhausted() && !publish()) {
      disconnect("Extranonce exhausted");
      continue;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
