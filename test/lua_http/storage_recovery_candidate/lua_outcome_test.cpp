#include "LuaRecoveryContract.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace recoverable;
int main() {
  const auto unknown = luaPublicationOutcome(CommitState::commit_unknown);
  assert(unknown.apiVersion == 5);
  assert(!unknown.ok);
  assert(unknown.recoveryRequired);
  assert(!unknown.retryAllowed);
  assert(std::strcmp(unknown.state, "commit_unknown") == 0);
  std::puts("PASS unknown_never_success_or_retry");
  const auto committed = luaPublicationOutcome(CommitState::committed);
  assert(committed.apiVersion == 5 && committed.ok && !committed.recoveryRequired);
  assert(!committed.retryAllowed && std::strcmp(committed.state, "committed") == 0);
  std::puts("PASS committed_is_only_success");
  const auto unchanged = luaPublicationOutcome(CommitState::not_committed);
  assert(!unchanged.ok && !unchanged.recoveryRequired && !unchanged.retryAllowed);
  assert(std::strcmp(unchanged.state, "not_committed") == 0);
  std::puts("PASS not_committed_is_not_auto_retry_authority");
  const auto invalid = luaPublicationOutcome(static_cast<CommitState>(255));
  assert(!invalid.ok && invalid.recoveryRequired && !invalid.retryAllowed);
  assert(std::strcmp(invalid.state, "commit_unknown") == 0);
  std::puts("PASS unknown_enum_fails_closed");
}
