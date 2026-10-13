#include "protocol.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
namespace btc {
static bool text(JsonVariantConst v, std::string& s, size_t max) {
  if (!v.is<const char*>())
    return false;
  s = v.as<const char*>();
  return s.size() <= max;
}
static bool hexText(JsonVariantConst v, std::string& s, size_t max, size_t exact = 0) {
  std::vector<uint8_t> b;
  return text(v, s, max * 2) && (!exact || s.size() == exact * 2) && unhex(s, b, max);
}
bool parseNotify(JsonVariantConst v, Job& j) {
  if (!v.is<JsonArrayConst>() || v.size() != 9)
    return false;
  Job n;
  if (!text(v[0], n.id, 64) || n.id.empty() || !hexText(v[1], n.prevhash, 32, 32) ||
      !hexText(v[2], n.coinb1, 2048) || !hexText(v[3], n.coinb2, 2048) ||
      !hexText(v[5], n.version, 4, 4) || !hexText(v[6], n.bits, 4, 4) ||
      !hexText(v[7], n.time, 4, 4) || !v[8].is<bool>() || !v[4].is<JsonArrayConst>() ||
      v[4].size() > 32)
    return false;
  for (JsonVariantConst b : v[4].as<JsonArrayConst>()) {
    std::string s;
    if (!hexText(b, s, 32, 32))
      return false;
    n.branches.push_back(s);
  }
  n.clean = v[8].as<bool>();
  j = std::move(n);
  return true;
}
bool parseSubscribe(JsonVariantConst v, std::string& ex1, unsigned& size) {
  if (!v.is<JsonArrayConst>() || v.size() != 3 || !v[0].is<JsonArrayConst>() ||
      !hexText(v[1], ex1, 32) || !v[2].is<unsigned>())
    return false;
  size = v[2].as<unsigned>();
  return size >= 1 && size <= 16;
}
bool parseDifficulty(const std::string& line, double& value) {
  // Caller validates the full JSON first. Locate the top-level params key and use
  // libc strtod on its numeric token, avoiding ArduinoJson's exponent rounding.
  int depth = 0, matches = 0;
  bool found = false;
  for (size_t i = 0; i < line.size(); i++) {
    char c = line[i];
    if (c == '{' || c == '[') {
      depth++;
      continue;
    }
    if (c == '}' || c == ']') {
      depth--;
      continue;
    }
    if (c != '"')
      continue;
    size_t start = i++;
    while (i < line.size()) {
      if (line[i] == '\\') {
        i += 2;
        continue;
      }
      if (line[i] == '"')
        break;
      i++;
    }
    if (i >= line.size())
      return false;
    if (depth != 1 || line.substr(start, i - start + 1) != "\"params\"")
      continue;
    size_t p = i + 1;
    auto space = [&]() {
      while (p < line.size() && isspace(static_cast<unsigned char>(line[p]))) p++;
    };
    space();
    if (p >= line.size() || line[p++] != ':')
      continue;
    matches++;
    space();
    if (p >= line.size() || line[p++] != '[')
      return false;
    space();
    char* end = nullptr;
    value = strtod(line.c_str() + p, &end);
    if (end == line.c_str() + p || !std::isfinite(value))
      return false;
    p = end - line.c_str();
    space();
    if (p >= line.size() || line[p] != ']')
      return false;
    found = true;
  }
  return found && matches == 1;
}
bool successfulResponse(JsonVariantConst doc) {
  return doc["error"].isNull() && doc["result"].is<bool>() && doc["result"].as<bool>();
}
bool connectionTimedOut(uint32_t now, uint32_t connectedAt, uint32_t lastRx, uint32_t lastJob,
                        bool ready) {
  return (!ready && uint32_t(now - connectedAt) > 20000) || uint32_t(now - lastRx) > 90000 ||
         uint32_t(now - lastJob) > 180000;
}
std::string request(unsigned id, const char* method, const std::vector<std::string>& params) {
  DynamicJsonDocument d(2048);
  d["id"] = id;
  d["method"] = method;
  auto a = d.createNestedArray("params");
  for (auto& s : params) a.add(s);
  std::string out;
  serializeJson(d, out);
  out += '\n';
  return out;
}
std::string submission(unsigned id, const std::string& worker, const Job& j, const std::string& ex2,
                       uint32_t nonce) {
  char n[9];
  snprintf(n, sizeof(n), "%08x", unsigned(nonce));
  return request(id, "mining.submit", {worker, j.id, ex2, j.time, n});
}
}  // namespace btc
