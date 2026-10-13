#include "stats.h"
Statistics stats;
SemaphoreHandle_t statsMutex;
void status(const char* s) {
  StatsLock lock;
  strlcpy(stats.status, s, sizeof(stats.status));
}
Statistics snapshot() {
  StatsLock lock;
  return stats;
}
