#pragma once
#include "BodyStage.h"
#include "PreSealCheck.h"
namespace recoverable {
// Version 1 fixed-slot record publication, separate from the Lua API4 contract.
// Caller holds the exclusive app lock throughout staging, sealing and recovery.
// openSeal MUST open the existing file for in-place metadata writes, without
// truncation, creation, rename, removal or modification on failure. read/size
// after close MUST reopen/read storage rather than a stale writer buffer.
struct SealFiles : StageFiles {
  virtual bool openSeal(unsigned slot) = 0;
};
enum class CommitState { committed, not_committed, commit_unknown };
inline CommitState sealStaged(SealFiles& files, BodyInput& input, const StageReceipt& receipt) {
  if (!receipt.valid || receipt.slot > 1 || !receipt.generation || receipt.length > MaxPayload)
    return CommitState::not_committed;
  const Recovery recovered = RecoverableDownloadStore(files).recover();
  const WritePlan plan = admitWrite(recovered, receipt.length, input.cancelled());
  if (plan.state != AdmissionState::ready || plan.slot != receipt.slot ||
      plan.generation != receipt.generation ||
      checkStaged(files, receipt.slot, receipt.length, receipt.checksum) != PreSealState::verified)
    return CommitState::not_committed;
  if (input.cancelled()) return CommitState::not_committed;
  if (!files.openSeal(receipt.slot)) { files.close(receipt.slot); return CommitState::not_committed; }
  if (input.cancelled()) { files.close(receipt.slot); return CommitState::not_committed; }
  uint8_t header[24] = {}, footer[24] = {};
  const auto put = [](uint8_t* p, uint64_t n, unsigned count) {
    for (unsigned i = 0; i < count; ++i) p[i] = uint8_t(n >> (i * 8));
  };
  put(header, 0x31534452U, 4); put(footer, 0x4c414553U, 4);
  uint8_t* metadata[2] = {header, footer};
  for (uint8_t* p : metadata) {
    put(p + 4, 1, 4); put(p + 8, receipt.generation, 8);
    put(p + 16, receipt.length, 4); put(p + 20, receipt.checksum, 4);
  }
  // From the first metadata write onward a failed acknowledgement may conceal
  // mutation. Never retry, delete, roll back or report cancellation/no-change.
  bool ok = files.write(receipt.slot, uint64_t(receipt.length) + 24, footer, sizeof(footer));
  if (ok) ok = files.write(receipt.slot, 0, header, sizeof(header));
  if (ok) ok = files.sync(receipt.slot);
  const bool closed = files.close(receipt.slot);
  if (!ok || !closed) return CommitState::commit_unknown;
  const Recovery after = RecoverableDownloadStore(files).recover();
  if (after.state != RecoveryState::selected || after.slot != receipt.slot ||
      after.record.generation != receipt.generation || after.record.length != receipt.length ||
      after.record.checksum != receipt.checksum) return CommitState::commit_unknown;
  return CommitState::committed;
}
}
