#include <algorithm>
#include <cstring>

#include "bitcoin.h"
namespace btc {
bool addressScript(const std::string& address, std::vector<uint8_t>& script) {
  script.clear();
  if (address.size() < 14 || address.size() > 90 || address.find('\0') != std::string::npos)
    return false;
  if (address.rfind("bc1", 0) == 0 || address.rfind("BC1", 0) == 0) {
    bool lower = false, upper = false;
    std::string s = address;
    for (char& c : s) {
      if (c >= 'a' && c <= 'z')
        lower = true;
      if (c >= 'A' && c <= 'Z') {
        upper = true;
        c += 32;
      }
    }
    if (lower && upper)
      return false;
    const char* alphabet = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";
    std::vector<uint8_t> data;
    for (size_t i = 3; i < s.size(); i++) {
      const char* p = strchr(alphabet, s[i]);
      if (!p)
        return false;
      data.push_back(p - alphabet);
    }
    uint32_t chk = 1;
    auto step = [&](int v) {
      uint32_t top = chk >> 25;
      chk = ((chk & 0x1ffffff) << 5) ^ v;
      const uint32_t gen[] = {0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3};
      for (int i = 0; i < 5; i++)
        if ((top >> i) & 1)
          chk ^= gen[i];
    };
    step('b' >> 5);
    step('c' >> 5);
    step(0);
    step('b' & 31);
    step('c' & 31);
    for (auto v : data) step(v);
    if (data.size() < 7 || data[0] > 16)
      return false;
    unsigned version = data[0];
    if (chk != (version == 0 ? 1u : 0x2bc830a3u))
      return false;
    unsigned acc = 0, bits = 0;
    std::vector<uint8_t> program;
    for (size_t i = 1; i < data.size() - 6; i++) {
      acc = ((acc << 5) | data[i]) & 4095;
      bits += 5;
      if (bits >= 8) {
        bits -= 8;
        program.push_back((acc >> bits) & 255);
      }
    }
    if (bits >= 5 || ((acc << (8 - bits)) & 255))
      return false;
    if (program.size() < 2 || program.size() > 40)
      return false;
    // Present-day standard Bitcoin receiving outputs supported here: v0 or Taproot v1.
    if (!(version == 0 ? (program.size() == 20 || program.size() == 32)
                       : (version == 1 && program.size() == 32)))
      return false;
    script.push_back(version == 0 ? 0 : 0x50 + version);
    script.push_back(program.size());
    script.insert(script.end(), program.begin(), program.end());
    return true;
  }
  if (address[0] != '1' && address[0] != '3')
    return false;
  const char* alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
  std::array<uint8_t, 25> n{};
  for (char c : address) {
    const char* p = strchr(alphabet, c);
    if (!p)
      return false;
    unsigned carry = p - alphabet;
    for (int i = 24; i >= 0; i--) {
      carry += n[i] * 58;
      n[i] = carry & 255;
      carry >>= 8;
    }
    if (carry)
      return false;
  }
  size_t leading = address.find_first_not_of('1');
  if (leading == std::string::npos)
    return false;
  size_t zero = 0;
  while (zero < 25 && !n[zero]) zero++;
  if (leading != zero)
    return false;
  if (n[0] != 0 && n[0] != 5)
    return false;
  Hash checksum = sha256d(n.data(), 21);
  if (!std::equal(n.begin() + 21, n.end(), checksum.begin()))
    return false;
  if (n[0] == 0)
    script = {0x76, 0xa9, 0x14};
  else
    script = {0xa9, 0x14};
  script.insert(script.end(), n.begin() + 1, n.begin() + 21);
  script.push_back(n[0] == 0 ? 0x88 : 0x87);
  if (n[0] == 0)
    script.push_back(0xac);
  return true;
}
bool addressValid(const std::string& address) {
  std::vector<uint8_t> script;
  return addressScript(address, script);
}
}  // namespace btc
