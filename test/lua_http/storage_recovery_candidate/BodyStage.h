#pragma once
#include "WriteAdmission.h"
namespace recoverable {
// Candidate-only interface: no path, rename, unlink or seal operation.
// Caller owns an exclusive app lock from fresh recovery through publication.
struct StageFiles : Files {
  virtual bool reset(unsigned slot) = 0;
  virtual bool write(unsigned slot, uint64_t offset, const uint8_t* data, size_t bytes) = 0;
  virtual bool sync(unsigned slot) = 0;
  virtual bool close(unsigned slot) = 0;
};
struct BodyInput {
  virtual ~BodyInput() = default;
  virtual bool read(uint32_t offset, uint8_t* out, size_t bytes) = 0;
  virtual bool cancelled() = 0;
};
enum class StageState { staged, rejected, cancelled, io_error };
// Valid only after successful sync AND close. This receipt is not a seal or
// authority to publish: the caller must read back and check the staged bytes.
struct StageReceipt {
  bool valid = false;
  unsigned slot = 0;
  uint64_t generation = 0;
  uint32_t length = 0;
  uint32_t checksum = 0;
};
// Does NOT publish or produce a recoverable record. Invalid header/footer remain
// zero; metadata, checksum readback and sealing are separate pending phases.
// Abort never deletes a slot. A failed reset/write can have modified the inactive
// slot; only the selected previous generation is promised unchanged.
inline StageState stageBody(StageFiles& files, BodyInput& input,
                            const WritePlan& plan, uint32_t length, StageReceipt* receipt = nullptr) {
  if (receipt) *receipt = {};
  if (plan.state != AdmissionState::ready || plan.slot > 1 || !plan.generation || length > MaxPayload)
    return StageState::rejected;
  if (input.cancelled()) return StageState::cancelled;
  const unsigned slot = plan.slot;
  if (!files.reset(slot)) { files.close(slot); return StageState::io_error; }
  uint8_t zeros[24] = {}, buffer[1024];
  bool ok = files.write(slot, 0, zeros, sizeof(zeros));
  uint32_t offset = 0;
  uint32_t checksum = 2166136261U;
  while (ok && offset < length) {
    if (input.cancelled()) { files.close(slot); return StageState::cancelled; }
    size_t bytes = length - offset;
    if (bytes > sizeof(buffer)) bytes = sizeof(buffer);
    ok = input.read(offset, buffer, bytes) && files.write(slot, uint64_t(offset) + 24, buffer, bytes);
    if (ok) {
      for (size_t i = 0; i < bytes; ++i) checksum = (checksum ^ buffer[i]) * 16777619U;
      offset += uint32_t(bytes);
    }
  }
  if (ok && input.cancelled()) { files.close(slot); return StageState::cancelled; }
  if (ok) ok = files.write(slot, uint64_t(length) + 24, zeros, sizeof(zeros));
  if (ok) ok = files.sync(slot);
  const bool closed = files.close(slot);
  if (ok && closed && receipt)
    *receipt = {true, slot, plan.generation, length, checksum};
  return ok && closed ? StageState::staged : StageState::io_error;
}
}
