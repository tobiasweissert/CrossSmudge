#pragma once
#include <algorithm>
#include <cstring>
#include <string>
// Nested-only fault controls layered onto the qualified clipping HAL, not stores.
inline bool failBackup = false, shortWrite = false, failSync = false, failRead = false;
inline bool mutateRenameFailure = false, corruptRead = false;
inline std::string failRenameTo, readbackFaultPath;
inline int failWriteAt = 0, writeCalls = 0, shortWrites = 0, mutations = 0;
inline std::string inputFaultPath;
inline size_t inputFaultPosition = 0;
inline int inputFaultReturn = -1, inputFaultHits = 0, inputReadsAfterFault = 0;
inline bool inputFault = false;
