#pragma once
#include <cstddef>
#include <cstdint>
namespace recoverable {
constexpr uint32_t MaxPayload = 2U * 1024U * 1024U;
enum class SlotState { absent, invalid, valid, uncertain };
struct Record { SlotState state = SlotState::invalid; uint64_t generation = 0; uint32_t length = 0; uint32_t checksum = 0; };
// Fixed numeric slots only. Implementations must confine them to the app root.
struct Files {
  virtual ~Files() = default;
  // absent is distinct from an I/O failure; size is authoritative only for valid.
  virtual SlotState size(unsigned slot, uint64_t& bytes) = 0;
  virtual bool read(unsigned slot, uint64_t offset, uint8_t* out, size_t bytes) = 0;
};
enum class RecoveryState { empty, selected, uncertain, conflict };
struct Recovery { RecoveryState state = RecoveryState::empty; unsigned slot = 0; Record record; };
class RecoverableDownloadStore {
 public:
  explicit RecoverableDownloadStore(Files& files) : files_(files) {}
  Record validate(unsigned slot) {
    if (slot > 1) return {};
    uint64_t size = 0;
    SlotState state = files_.size(slot, size);
    if (state == SlotState::absent || state == SlotState::uncertain) return {state};
    if (state != SlotState::valid || size < 48 || size > uint64_t(MaxPayload) + 48) return {};
    uint8_t header[24], footer[24], buffer[1024];
    if (!files_.read(slot, 0, header, sizeof(header))) return {SlotState::uncertain};
    uint64_t generation = number(header + 8, 8);
    uint32_t length = uint32_t(number(header + 16, 4));
    uint32_t expected = uint32_t(number(header + 20, 4));
    if (number(header, 4) != 0x31534452U || number(header + 4, 4) != 1 ||
        !generation || length > MaxPayload || size != uint64_t(length) + 48) return {};
    uint32_t hash = 2166136261U;
    uint32_t offset = 0;
    while (offset < length) {
      size_t bytes = length - offset;
      if (bytes > sizeof(buffer)) bytes = sizeof(buffer);
      if (!files_.read(slot, uint64_t(offset) + 24, buffer, bytes)) return {SlotState::uncertain};
      for (size_t i = 0; i < bytes; ++i) hash = (hash ^ buffer[i]) * 16777619U;
      offset += uint32_t(bytes);
    }
    if (!files_.read(slot, uint64_t(length) + 24, footer, sizeof(footer))) return {SlotState::uncertain};
    if (hash != expected || number(footer, 4) != 0x4c414553U || number(footer + 4, 4) != 1 ||
        number(footer + 8, 8) != generation || number(footer + 16, 4) != length ||
        number(footer + 20, 4) != expected) return {};
    return {SlotState::valid, generation, length, hash};
  }
  Recovery recover() {
    const Record a = validate(0), b = validate(1);
    if (a.state == SlotState::uncertain || b.state == SlotState::uncertain)
      return {RecoveryState::uncertain, 0, {}};
    if (a.state != SlotState::valid && b.state != SlotState::valid) return {};
    if (a.state != SlotState::valid) return {RecoveryState::selected, 1, b};
    if (b.state != SlotState::valid) return {RecoveryState::selected, 0, a};
    if (a.generation != b.generation)
      return a.generation > b.generation ? Recovery{RecoveryState::selected, 0, a}
                                         : Recovery{RecoveryState::selected, 1, b};
    if (a.length != b.length || a.checksum != b.checksum) return {RecoveryState::conflict, 0, {}};
    // Checksums detect corruption, not identity: compare equal-generation bytes.
    uint8_t left[512], right[512];
    for (uint32_t offset = 0; offset < a.length;) {
      size_t bytes = a.length - offset;
      if (bytes > sizeof(left)) bytes = sizeof(left);
      if (!files_.read(0, uint64_t(offset) + 24, left, bytes) ||
          !files_.read(1, uint64_t(offset) + 24, right, bytes)) return {RecoveryState::uncertain, 0, {}};
      for (size_t i = 0; i < bytes; ++i)
        if (left[i] != right[i]) return {RecoveryState::conflict, 0, {}};
      offset += uint32_t(bytes);
    }
    return {RecoveryState::selected, 0, a};
  }
 private:
  static uint64_t number(const uint8_t* bytes, unsigned count) {
    uint64_t result = 0;
    for (unsigned i = 0; i < count; ++i) result |= uint64_t(bytes[i]) << (8 * i);
    return result;
  }
  Files& files_;
};
}
