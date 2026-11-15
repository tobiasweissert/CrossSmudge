#pragma once
#include "RecoverableDownloadStore.h"
#include <limits>
namespace recoverable {
// Read-only planning, NOT a storage lease. One exclusive owner must retain the
// app store lock across recover(), admission and the entire eventual write.
enum class AdmissionState { ready, cancelled, invalid, recovery_required, generation_exhausted };
struct WritePlan {
  AdmissionState state = AdmissionState::invalid;
  unsigned slot = 0;
  uint64_t generation = 0;
};
inline WritePlan admitWrite(const Recovery& recovery, uint32_t length, bool cancelled) {
  if (cancelled) return {AdmissionState::cancelled};
  if (length > MaxPayload) return {AdmissionState::invalid};
  if (recovery.state == RecoveryState::uncertain || recovery.state == RecoveryState::conflict)
    return {AdmissionState::recovery_required};
  if (recovery.state == RecoveryState::empty) return {AdmissionState::ready, 0, 1};
  if (recovery.state != RecoveryState::selected || recovery.slot > 1 ||
      recovery.record.state != SlotState::valid || !recovery.record.generation)
    return {AdmissionState::invalid};
  if (recovery.record.generation == std::numeric_limits<uint64_t>::max())
    return {AdmissionState::generation_exhausted};
  return {AdmissionState::ready, 1U - recovery.slot, recovery.record.generation + 1};
}
}
