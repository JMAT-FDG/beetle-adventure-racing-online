#pragma once
#include <cstdint>
// Installed before the render threads start; null in a standalone renderer.
extern "C" {
extern bool (*RT64_BarStreamWanted)();
extern void (*RT64_BarStreamFrame)(const uint8_t*, uint32_t, uint32_t, uint32_t);
extern float (*RT64_BarStreamAspect)();
}
