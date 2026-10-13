#include <algorithm>

#include "bitcoin.h"

namespace btc {
namespace {
// Parse stripped Bitcoin transaction serialization only. Stratum coinbase hashing
// requires the txid serialization, not the witness-inclusive transaction.
class Reader {
 public:
  explicit Reader(const std::vector<uint8_t>& bytes) : data(bytes) {}
  bool number(unsigned bytes, uint64_t& value) {
    if (bytes > 8 || bytes > data.size() - offset)
      return false;
    value = 0;
    for (unsigned i = 0; i < bytes; ++i) value |= uint64_t(data[offset++]) << (8 * i);
    return true;
  }
  bool compact(uint64_t& value) {
    uint64_t marker;
    if (!number(1, marker))
      return false;
    if (marker < 253) {
      value = marker;
      return true;
    }
    unsigned size = marker == 253 ? 2 : marker == 254 ? 4 : 8;
    if (!number(size, value))
      return false;
    return value >= (size == 2 ? 253ULL : size == 4 ? 65536ULL : 4294967296ULL);
  }
  bool skip(uint64_t count) {
    if (count > data.size() - offset)
      return false;
    offset += size_t(count);
    return true;
  }
  const std::vector<uint8_t>& data;
  size_t offset = 0;
};
}  // namespace

PayoutCheck checkCoinbasePayout(const std::vector<uint8_t>& tx,
                                const std::vector<uint8_t>& expected) {
  PayoutCheck result;
  if (tx.size() > 4096 || expected.empty() || expected.size() > 42)
    return result;
  Reader r(tx);
  uint64_t count, value, length;
  if (!r.skip(4) || !r.compact(count) || count != 1)
    return result;
  size_t prev = r.offset;
  if (!r.skip(32) ||
      !std::all_of(tx.begin() + prev, tx.begin() + prev + 32, [](uint8_t b) { return b == 0; }))
    return result;
  if (!r.number(4, value) || value != 0xffffffffULL || !r.compact(length) || length < 2 ||
      length > 100 || !r.skip(length) || !r.skip(4) || !r.compact(count) || count == 0 ||
      count > 128)
    return result;
  constexpr uint64_t maxMoney = 21000000ULL * 100000000ULL;
  for (uint64_t i = 0; i < count; ++i) {
    if (!r.number(8, value) || value > maxMoney || result.total > maxMoney - value ||
        !r.compact(length))
      return PayoutCheck{};
    size_t start = r.offset;
    if (!r.skip(length))
      return PayoutCheck{};
    result.total += value;
    if (length == expected.size() &&
        std::equal(expected.begin(), expected.end(), tx.begin() + start))
      result.paidToWallet += value;
  }
  if (!r.skip(4) || r.offset != tx.size())
    return PayoutCheck{};
  result.valid = true;
  // Strict policy: every satoshi in positive-valued coinbase outputs pays the
  // saved wallet. No hidden or automatically permitted pool/developer fee.
  result.verified = result.total > 0 && result.paidToWallet == result.total;
  return result;
}
}  // namespace btc
