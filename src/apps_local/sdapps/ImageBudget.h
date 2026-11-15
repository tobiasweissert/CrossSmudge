#pragma once
#include <cstddef>
#include <cstdint>
namespace sdapps {
// Resource admission only. Espressif's validator must still validate structure,
// symbol names, relocation types and targets before any module code executes.
bool imageWithinBudget(const uint8_t* bytes, size_t size);
}
