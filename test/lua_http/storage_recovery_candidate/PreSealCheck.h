#pragma once
#include "RecoverableDownloadStore.h"
namespace recoverable {
enum class PreSealState { verified, invalid, uncertain };
// Read-only candidate check, NOT publication. Caller holds the exclusive app
// lock and supplies the checksum accumulated from the accepted input body.
// Zero metadata proves staging is still unsealed; sealing remains separate.
inline PreSealState checkStaged(Files& files, unsigned slot, uint32_t length, uint32_t expected) {
  if (slot > 1 || length > MaxPayload) return PreSealState::invalid;
  uint64_t size = 0;
  const SlotState state = files.size(slot, size);
  if (state == SlotState::uncertain) return PreSealState::uncertain;
  if (state != SlotState::valid || size != uint64_t(length) + 48) return PreSealState::invalid;
  uint8_t metadata[24], buffer[1024];
  for (unsigned end = 0; end < 2; ++end) {
    const uint64_t offset = end ? uint64_t(length) + 24 : 0;
    if (!files.read(slot, offset, metadata, sizeof(metadata))) return PreSealState::uncertain;
    for (uint8_t byte : metadata) if (byte) return PreSealState::invalid;
  }
  uint32_t hash = 2166136261U;
  for (uint32_t offset = 0; offset < length;) {
    size_t bytes = length - offset;
    if (bytes > sizeof(buffer)) bytes = sizeof(buffer);
    if (!files.read(slot, uint64_t(offset) + 24, buffer, bytes)) return PreSealState::uncertain;
    for (size_t i = 0; i < bytes; ++i) hash = (hash ^ buffer[i]) * 16777619U;
    offset += uint32_t(bytes);
  }
  return hash == expected ? PreSealState::verified : PreSealState::invalid;
}
}
