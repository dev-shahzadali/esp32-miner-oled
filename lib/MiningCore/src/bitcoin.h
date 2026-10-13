#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace btc {
using Hash = std::array<uint8_t, 32>;
void sha256(const uint8_t*, size_t, uint8_t*);
Hash sha256d(const uint8_t*, size_t);
bool unhex(const std::string&, std::vector<uint8_t>&, size_t maxBytes = 4096);
std::string hex(const uint8_t*, size_t);
void putLE(uint8_t*, uint32_t);
bool compactTarget(uint32_t, Hash&);
bool difficultyTarget(double, Hash&);
bool meets(const uint8_t*, const Hash&);
double difficulty(const uint8_t*);
bool addressValid(const std::string&);
bool addressScript(const std::string&, std::vector<uint8_t>&);
struct PayoutCheck {
  bool valid = false, verified = false;
  uint64_t total = 0, paidToWallet = 0;
};
PayoutCheck checkCoinbasePayout(const std::vector<uint8_t>&, const std::vector<uint8_t>&);
struct Job {
  std::string id, prevhash, coinb1, coinb2, version, bits, time;
  std::vector<std::string> branches;
  bool clean = true;
};
bool buildHeader(const Job&, const std::string&, const std::string&, std::array<uint8_t, 80>&);
}  // namespace btc
