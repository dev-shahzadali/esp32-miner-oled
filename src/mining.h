#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "bitcoin.h"
#ifndef MINER_WORKERS
#define MINER_WORKERS 2
#endif
struct Work {
  alignas(4) std::array<uint8_t, 80> header{};
  btc::Hash target{};
  btc::Hash networkTarget{};
  uint32_t generation = 0;
  char id[65] = {}, time[9] = {}, ex2[33] = {};
  bool active = false;
};
struct Share {
  uint32_t generation, nonce;
  char id[65], time[9], ex2[33];
};
extern QueueHandle_t shareQueue;
void initMining();
void stopMining();
void publishWork(const Work&);
bool workExhausted();
uint32_t workGeneration();
void miningWorker(void*);
bool selfTest();
void benchmarkHashing();
