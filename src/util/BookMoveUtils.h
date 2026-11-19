#pragma once

#include <string>

namespace BookMoveUtils {

enum class RenameMigrationResult {
  Success,
  RolledBack,
  KeepRenamed,
  Uncertain,
};

// File-only entry point used by web Rename/Move. Caller must close reader/store
// sessions first. Refuses collisions, dual TXT cache roots and retained journals;
// moves the entire cache (including statsOff and temporary recovery records).
RenameMigrationResult moveFile(const std::string& oldPath, const std::string& newPath);

std::string buildReadFolderDestination(const std::string& srcPath);
// Prepares reader metadata, renames the physical book, then commits the state
// migration so one canonical metadata path is always available across resets.
// Files manual Move opts into preserveOnRenameFailure: unacknowledged cache,
// file or reference writes retain candidates and report Uncertain, not rollback.
// Conservative mode writes a checked encompassing CPBOOKMOVE1 intent before
// store preparation. Any subsequent failure retains journal/candidates and
// returns Uncertain; public callers must not clear it on retry or reboot.
// Store-specific retained-root rules remain BookmarkStore/ClippingStore-owned.
RenameMigrationResult migrateRenamedBookState(const std::string& oldPath, const std::string& newPath,
                                              const std::string& oldCachePath, const std::string& title,
                                              const std::string& author, const char* bookType,
                                              bool preserveOnRenameFailure = false);
bool migrateMovedEpubState(const std::string& oldPath, const std::string& newPath, const std::string& oldCachePath,
                           const std::string& title, const std::string& author, bool keepInRecents);

}  // namespace BookMoveUtils
