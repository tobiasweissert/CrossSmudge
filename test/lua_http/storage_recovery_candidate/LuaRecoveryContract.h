#pragma once
#include "SealStage.h"
namespace recoverable {
// Proposed Lua storage API5 envelope. Pure mapping only; NOT a persistent
// uncertainty latch, Lua registration, retry engine or storage adapter.
struct LuaPublicationOutcome {
  unsigned apiVersion;
  const char* state;
  bool ok;
  bool recoveryRequired;
  bool retryAllowed;
};
inline LuaPublicationOutcome luaPublicationOutcome(CommitState state) {
  switch (state) {
    case CommitState::committed: return {5, "committed", true, false, false};
    case CommitState::not_committed: return {5, "not_committed", false, false, false};
    case CommitState::commit_unknown: break;
  }
  return {5, "commit_unknown", false, true, false};
}
}
