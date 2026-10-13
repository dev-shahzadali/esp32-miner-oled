#pragma once
#include <ArduinoJson.h>

#include "bitcoin.h"
namespace btc {
constexpr size_t MAX_LINE = 16384;
bool parseNotify(JsonVariantConst, Job&);
bool parseSubscribe(JsonVariantConst, std::string&, unsigned&);
bool parseDifficulty(const std::string&, double&);
bool successfulResponse(JsonVariantConst);
bool connectionTimedOut(uint32_t now, uint32_t connectedAt, uint32_t lastRx, uint32_t lastJob,
                        bool ready);
std::string request(unsigned id, const char* method, const std::vector<std::string>& params);
std::string submission(unsigned id, const std::string& worker, const Job&, const std::string& ex2,
                       uint32_t nonce);
struct Backoff {
  unsigned seconds = 1;
  unsigned next() {
    unsigned n = seconds;
    seconds = seconds < 32 ? seconds * 2 : 60;
    return n;
  }
  void reset() {
    seconds = 1;
  }
};
}  // namespace btc
