#pragma once
#include <cstdint>
#include <chrono>

inline uint32_t current_ms() {
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
}
