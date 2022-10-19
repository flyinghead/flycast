#pragma once
#include <cstdint>

void set_timer_resolution();
void reset_timer_resolution();
int64_t sleep_and_busy_wait(int64_t us);

