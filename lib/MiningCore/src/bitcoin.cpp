#include "bitcoin.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
namespace btc {
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
static uint32_t rotr(uint32_t v, int n) {
  return (v >> n) | (v << (32 - n));
}
static void compress(uint32_t* s, const uint8_t* p) {
  uint32_t w[64];
  for (int i = 0; i < 16; i++)
    w[i] = (uint32_t(p[4 * i]) << 24) | (uint32_t(p[4 * i + 1]) << 16) |
           (uint32_t(p[4 * i + 2]) << 8) | p[4 * i + 3];
  for (int i = 16; i < 64; i++)
    w[i] = w[i - 16] + (rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
           (rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10));
  uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4], f = s[5], g = s[6], h = s[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t = h + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
    uint32_t u = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g;
    g = f;
    f = e;
    e = d + t;
    d = c;
    c = b;
    b = a;
    a = t + u;
  }
  s[0] += a;
  s[1] += b;
  s[2] += c;
  s[3] += d;
  s[4] += e;
  s[5] += f;
  s[6] += g;
  s[7] += h;
}
void sha256(const uint8_t* p, size_t n, uint8_t* out) {
  uint32_t s[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  size_t whole = n / 64;
  for (size_t i = 0; i < whole; i++) compress(s, p + 64 * i);
  uint8_t tail[128] = {};
  size_t rem = n % 64;
  if (rem)
    memcpy(tail, p + whole * 64, rem);
  tail[rem] = 0x80;
  size_t len = rem < 56 ? 64 : 128;
  uint64_t bits = uint64_t(n) * 8;
  for (int i = 0; i < 8; i++) tail[len - 1 - i] = bits >> (8 * i);
  compress(s, tail);
  if (len == 128)
    compress(s, tail + 64);
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 4; j++) out[4 * i + j] = s[i] >> (24 - 8 * j);
}
Hash sha256d(const uint8_t* p, size_t n) {
  Hash a, b;
  sha256(p, n, a.data());
  sha256(a.data(), 32, b.data());
  return b;
}
static int nib(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}
bool unhex(const std::string& s, std::vector<uint8_t>& out, size_t max) {
  out.clear();
  if (s.size() % 2 || s.size() / 2 > max)
    return false;
  out.reserve(s.size() / 2);
  for (size_t i = 0; i < s.size(); i += 2) {
    int a = nib(s[i]), b = nib(s[i + 1]);
    if (a < 0 || b < 0) {
      out.clear();
      return false;
    }
    out.push_back(a * 16 + b);
  }
  return true;
}
std::string hex(const uint8_t* p, size_t n) {
  static const char* d = "0123456789abcdef";
  std::string s;
  s.reserve(2 * n);
  for (size_t i = 0; i < n; i++) {
    s += d[p[i] >> 4];
    s += d[p[i] & 15];
  }
  return s;
}
void putLE(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) p[i] = v >> (8 * i);
}
bool compactTarget(uint32_t c, Hash& t) {
  t.fill(0);
  unsigned e = c >> 24;
  uint32_t m = c & 0x7fffff;
  if (!m || (c & 0x800000) || e > 34 || (e > 33 && m > 0xff) || (e > 32 && m > 0xffff))
    return false;
  if (e <= 3) {
    m >>= 8 * (3 - e);
    for (int i = 0; i < 3; i++) t[i] = m >> (8 * i);
  } else
    for (int i = 0; i < 3; i++)
      if (e - 3 + i < 32)
        t[e - 3 + i] = m >> (8 * i);
  return std::any_of(t.begin(), t.end(), [](uint8_t x) { return x != 0; });
}
// Exact floor of diff1 / IEEE-754 binary64 difficulty, using integer long division.
// Targets are little endian. This avoids rounding a boundary upward with doubles.
bool difficultyTarget(double d, Hash& out) {
  out.fill(0);
  if (!std::isfinite(d) || d < std::ldexp(1.0, -64) || d > std::ldexp(1.0, 256))
    return false;
  uint64_t raw;
  memcpy(&raw, &d, 8);
  uint64_t mant = (raw & ((uint64_t(1) << 52) - 1));
  int exp = int((raw >> 52) & 2047);
  if (exp)
    mant |= uint64_t(1) << 52;
  else
    exp = 1;
  int shift = exp - 1023 - 52;
  std::array<uint8_t, 160> numerator{}, denominator{}, quotient{};
  numerator[26] = 0xff;
  numerator[27] = 0xff;
  for (int i = 0; i < 8; i++) denominator[i] = mant >> (8 * i);
  auto shl = [](auto& a, int bits) {
    for (int k = 0; k < bits; k++) {
      uint8_t carry = 0;
      for (auto& x : a) {
        uint8_t next = x >> 7;
        x = uint8_t((x << 1) | carry);
        carry = next;
      }
    }
  };
  if (shift < 0)
    shl(numerator, -shift);
  else
    shl(denominator, shift);
  std::array<uint8_t, 160> remainder{};
  for (int bit = 1279; bit >= 0; --bit) {
    shl(remainder, 1);
    remainder[0] |= (numerator[bit / 8] >> (bit % 8)) & 1;
    int cmp = 0;
    for (int i = 159; i >= 0; i--)
      if (remainder[i] != denominator[i]) {
        cmp = remainder[i] > denominator[i] ? 1 : -1;
        break;
      }
    if (cmp >= 0) {
      int borrow = 0;
      for (int i = 0; i < 160; i++) {
        int v = int(remainder[i]) - denominator[i] - borrow;
        borrow = v < 0;
        remainder[i] = v;
      }
      quotient[bit / 8] |= 1 << (bit % 8);
    }
  }
  for (int i = 32; i < 160; i++)
    if (quotient[i]) {
      out.fill(255);
      return true;
    }
  std::copy_n(quotient.begin(), 32, out.begin());
  return true;
}
bool meets(const uint8_t* h, const Hash& t) {
  for (int i = 31; i >= 0; i--) {
    if (h[i] < t[i])
      return true;
    if (h[i] > t[i])
      return false;
  }
  return true;
}
double difficulty(const uint8_t* h) {
  double v = 0;
  for (int i = 31; i >= 0; i--) v = v * 256 + h[i];
  return v ? std::ldexp(65535.0, 208) / v : std::numeric_limits<double>::infinity();
}
bool buildHeader(const Job& j, const std::string& ex1, const std::string& ex2,
                 std::array<uint8_t, 80>& h) {
  std::vector<uint8_t> c, b;
  if (j.id.empty() || j.id.size() > 64 || j.branches.size() > 32 || ex1.size() > 64 ||
      ex2.empty() || ex2.size() > 32)
    return false;
  if (!unhex(j.coinb1 + ex1 + ex2 + j.coinb2, c, 4096))
    return false;
  Hash root = sha256d(c.data(), c.size());
  for (auto& branch : j.branches) {
    if (branch.size() != 64 || !unhex(branch, b, 32))
      return false;
    uint8_t pair[64];
    memcpy(pair, root.data(), 32);
    memcpy(pair + 32, b.data(), 32);
    root = sha256d(pair, 64);
  }
  if (j.prevhash.size() != 64 || !unhex(j.prevhash, b, 32))
    return false;
  // Stratum prevhash is in 32-bit word-swapped order, NOT display-hash order.
  for (int i = 0; i < 32; i += 4)
    for (int k = 0; k < 4; k++) h[4 + i + k] = b[i + 3 - k];
  memcpy(h.data() + 36, root.data(), 32);
  const std::string* fields[] = {&j.version, &j.time, &j.bits};
  int offsets[] = {0, 68, 72};
  for (int i = 0; i < 3; i++) {
    if (fields[i]->size() != 8 || !unhex(*fields[i], b, 4))
      return false;
    for (int k = 0; k < 4; k++) h[offsets[i] + k] = b[3 - k];
  }
  uint32_t bits =
      uint32_t(h[72]) | (uint32_t(h[73]) << 8) | (uint32_t(h[74]) << 16) | (uint32_t(h[75]) << 24);
  Hash target;
  if (!compactTarget(bits, target))
    return false;
  putLE(h.data() + 76, 0);
  return true;
}
}  // namespace btc
