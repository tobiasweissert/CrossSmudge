#pragma once

#include <string>

namespace BookMoveUtils {

enum class RenameMigrationResult {
  Success,
  RolledBack,
  KeepRenamed,
  Uncertain,
};

std::string buildReadFolderDestination(const std::string& srcPath);
// Prepares reader metadata, renames the physical book, then commits the state
// migration so one canonical metadata path is always available across resets.
// Files manual Move opts into preserveOnRenameFailure: unacknowledged cache,
// file or reference writes retain candidates and report Uncertain, not rollback.
// This is not a durable recovery journal; store-specific preparation IO still
// belongs to BookmarkStore/ClippingStore's existing migration contracts.
RenameMigrationResult migrateRenamedBookState(const std::string& oldPath, const std::string& newPath,
                                              const std::string& oldCachePath, const std::string& title,
                                              const std::string& author, const char* bookType,
                                              bool preserveOnRenameFailure = false);
bool migrateMovedEpubState(const std::string& oldPath, const std::string& newPath, const std::string& oldCachePath,
                           const std::string& title, const std::string& author, bool keepInRecents);

}  // namespace BookMoveUtils
