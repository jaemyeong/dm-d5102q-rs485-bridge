#pragma once
#include <stdint.h>
extern uint32_t fakeNow;
inline int64_t& fakeUptimeOffset() { static int64_t value = 0; return value; }
inline int64_t esp_timer_get_time() { return int64_t(fakeNow) * 1000 + fakeUptimeOffset(); }
