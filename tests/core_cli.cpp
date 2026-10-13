#include <cstring>
#include <iostream>

#include "bitcoin.h"
#include "nerdSHA256plus.h"
#include "protocol.h"
int main() {
  std::string line;
  while (std::getline(std::cin, line)) {
    DynamicJsonDocument input(32768), output(32768);
    if (line.size() > btc::MAX_LINE || deserializeJson(input, line)) {
      output["ok"] = false;
    } else {
      std::string op = input["op"].as<std::string>();
      bool ok = true;
      if (op == "sha" || op == "sha2" || op == "fast") {
        std::vector<uint8_t> bytes;
        ok = btc::unhex(input["hex"].as<std::string>(), bytes);
        alignas(4) uint8_t hash[32] = {};
        if (ok) {
          if (op == "sha")
            btc::sha256(bytes.data(), bytes.size(), hash);
          else if (op == "sha2") {
            auto h = btc::sha256d(bytes.data(), bytes.size());
            memcpy(hash, h.data(), 32);
          } else if (bytes.size() == 80) {
            alignas(4) uint8_t header[80];
            memcpy(header, bytes.data(), 80);
            uint32_t mid[8], bake[16];
            nerd_mids(mid, header);
            nerd_sha256_bake(mid, header + 64, bake);
            nerd_sha256d_baked(mid, header + 64, bake, hash);
          } else
            ok = false;
          output["hash"] = btc::hex(hash, 32);
        }
      } else if (op == "target") {
        btc::Hash t;
        double d = strtod(input["decimal"].as<const char*>(), nullptr);
        ok = btc::difficultyTarget(d, t);
        output["target"] = btc::hex(t.data(), 32);
      } else if (op == "compact") {
        btc::Hash t;
        ok = btc::compactTarget(input["bits"].as<uint32_t>(), t);
        output["target"] = btc::hex(t.data(), 32);
      } else if (op == "addressScript") {
        std::vector<uint8_t> script;
        ok = btc::addressScript(input["address"].as<std::string>(), script);
        output["script"] = btc::hex(script.data(), script.size());
      } else if (op == "payout") {
        std::vector<uint8_t> tx, script;
        ok = btc::unhex(input["tx"].as<std::string>(), tx, 4096) &&
             btc::unhex(input["script"].as<std::string>(), script, 42);
        if (ok) {
          auto check = btc::checkCoinbasePayout(tx, script);
          output["valid"] = check.valid;
          output["verified"] = check.verified;
          output["total"] = check.total;
          output["paid"] = check.paidToWallet;
        }
      } else if (op == "address")
        ok = btc::addressValid(input["address"].as<std::string>());
      else if (op == "header" || op == "notify") {
        btc::Job j;
        ok = btc::parseNotify(input["params"], j);
        if (ok && op == "header") {
          std::array<uint8_t, 80> h;
          ok = btc::buildHeader(j, input["ex1"].as<std::string>(), input["ex2"].as<std::string>(),
                                h);
          if (ok)
            output["header"] = btc::hex(h.data(), 80);
        }
      } else if (op == "subscribe") {
        std::string ex;
        unsigned size = 0;
        ok = btc::parseSubscribe(input["result"], ex, size);
        output["ex1"] = ex;
        output["size"] = size;
      } else if (op == "submit") {
        btc::Job j;
        j.id = input["job"].as<std::string>();
        j.time = input["time"].as<std::string>();
        output["payload"] = btc::submission(input["id"], input["worker"].as<std::string>(), j,
                                            input["ex2"].as<std::string>(), input["nonce"]);
      } else if (op == "compare") {
        std::vector<uint8_t> h, t;
        ok = btc::unhex(input["hash"].as<std::string>(), h, 32) && h.size() == 32 &&
             btc::unhex(input["target"].as<std::string>(), t, 32) && t.size() == 32;
        if (ok) {
          btc::Hash target;
          memcpy(target.data(), t.data(), 32);
          output["meets"] = btc::meets(h.data(), target);
          char d[64];
          snprintf(d, sizeof(d), "%.17g", btc::difficulty(h.data()));
          output["difficulty"] = d;
        }
      } else if (op == "difficultyMessage") {
        double d;
        std::string raw = input["raw"].as<std::string>();
        DynamicJsonDocument msg(2048);
        ok = !deserializeJson(msg, raw) && btc::parseDifficulty(raw, d);
        if (ok) {
          btc::Hash t;
          ok = btc::difficultyTarget(d, t);
          output["target"] = btc::hex(t.data(), 32);
        }
      } else if (op == "response") {
        output["success"] = btc::successfulResponse(input["message"]);
      } else if (op == "timeout") {
        output["expired"] = btc::connectionTimedOut(
            input["now"], input["connectedAt"], input["lastRx"], input["lastJob"], input["ready"]);
      } else if (op == "backoff") {
        btc::Backoff b;
        auto a = output.createNestedArray("delays");
        for (int i = 0; i < 10; i++) a.add(b.next());
        b.reset();
        output["reset"] = b.next();
      } else
        ok = false;
      output["ok"] = ok;
    }
    std::string response;
    serializeJson(output, response);
    std::cout << response << std::endl;
  }
}
