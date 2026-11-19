#include <CrossPointState.h>
#include <HalStorage.h>

#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <new>
// Observe the actual production string allocation; never attempt a giant allocation.
static bool watchAllocations = false;
static size_t oversizedAllocations = 0;
void* operator new(std::size_t n) {
  if (watchAllocations && n > 50000) {
    ++oversizedAllocations;
    if (n > 1000000) throw std::bad_alloc();
  }
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
__attribute__((noinline)) void operator delete(void* p) noexcept { std::free(p); }
__attribute__((noinline)) void operator delete[](void* p) noexcept { std::free(p); }
__attribute__((noinline)) void operator delete(void* p, std::size_t) noexcept { std::free(p); }
__attribute__((noinline)) void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
const std::string primary = "/.crosspoint/state.json";
const std::string temp = primary + ".checked.tmp", backup = primary + ".checked.bak",
                  pending = primary + ".checked.pending";
const std::string old =
    R"({"openEpubPath":"/old.epub","favoriteBootImagePath":"/keep.png","readerActivityLoadCount":27,"preferredSleepFolderPath":"/wallpapers","pendingBookmarkSpine":7,"quickLockRestoreFrontlight":true,"lastSleepImage":3})";
void reset() {
  io = {};
  io.files[primary] = old;
  assert(APP_STATE.loadFromFile());
  APP_STATE.openEpubPath.clear();
}
bool oldRetained() {
  return (io.files.count(primary) && io.files[primary] == old) || (io.files.count(backup) && io.files[backup] == old);
}
void snapshot(const std::string& path) {
  std::ofstream out(path);
  for (const auto& [key, value] : io.files) out << std::quoted(key) << ' ' << std::quoted(value) << '\n';
  assert(out.good());
}
int main(int argc, char** argv) {
  std::cout << std::unitbuf;
  auto& state = APP_STATE;
  if (argc == 3) {
    std::ifstream in(argv[2]);
    std::string key, value;
    while (in >> std::quoted(key) >> std::quoted(value)) io.files[key] = value;
    assert(!io.files.empty());
    const std::string mode = argv[1];
    if (mode == "--reopen-missing") io.files.erase(primary);
    if (mode == "--reopen-corrupt") io.files[primary] = "{broken";
    assert(state.loadFromFile());
    assert(state.openEpubPath.empty() && state.favoriteBootImagePath == "/keep.png");
    assert(state.readerActivityLoadCount == 27 && state.preferredSleepFolderPath == "/wallpapers");
    assert(state.pendingBookmarkSpine == 7 && state.quickLockRestoreFrontlight);
    const auto files = io.files;
    io.mutations.clear();
    assert(!state.saveToFile());  // recovery does not silently repair/truncate any root
    assert(io.mutations.empty() && io.files == files);
    std::cout << "PASS state fresh process " << mode << " checked read-only recovery\n";
    return 0;
  }
  assert(argc == 2);
  // Each oversized root must be rejected before both allocation and body IO.
  for (const auto& root : {primary, temp, pending})
    for (uint64_t reported : {uint64_t(50001), UINT64_MAX}) {
      io = {};
      io.files[root] = "{}";
      io.reportedSizes[root] = reported;
      if (root != pending) io.files[root == primary ? temp : pending] = old;
      const auto files = io.files;
      oversizedAllocations = 0;
      watchAllocations = true;
      bool loaded = false, threw = false;
      try {
        loaded = state.loadFromFile();
      } catch (const std::exception&) {
        threw = true;
      }
      watchAllocations = false;
      assert(!threw && oversizedAllocations == 0 && io.bodyReads[root] == 0);
      assert(loaded == (root != pending));
      if (loaded) assert(state.openEpubPath == "/old.epub" && state.readerActivityLoadCount == 27);
      assert(io.files == files && io.mutations.empty());
      std::cout << "PASS state size admission " << root << " reported " << reported << " no allocation or read\n";
    }
  for (const auto& root : {primary, temp, pending}) {
    for (const std::string& invalid : {std::string(), std::string("[]"), std::string("null"), std::string("42")}) {
      io = {};
      io.files[root] = invalid;
      const auto files = io.files;
      assert(!state.loadFromFile());
      assert(io.files == files && io.mutations.empty());
      if (invalid.empty()) assert(io.bodyReads[root] == 0);
    }
    std::cout << "PASS state zero and nonobject rejection retains " << root << '\n';
    io = {};
    io.files[root] = old + std::string(50000 - old.size(), ' ');
    const auto files = io.files;
    assert(state.loadFromFile());
    assert(state.openEpubPath == "/old.epub" && state.readerActivityLoadCount == 27 && state.isRecentSleep(3, 16));
    assert(io.files == files && io.mutations.empty() && io.bodyReads[root] > 0);
    std::cout << "PASS state exactly 50000 byte legacy object accepted " << root << '\n';
  }
  reset();
  JsonDocument measured;
  state.toJson(measured);
  state.openEpubPath = std::string(50000 - measureJson(measured), 'x');
  measured.clear();
  state.toJson(measured);
  assert(measureJson(measured) == 50000);
  assert(state.saveToFile() && io.files[primary].size() == 50000);
  state.openEpubPath.clear();
  assert(state.loadFromFile() && !state.openEpubPath.empty());
  std::cout << "PASS state exactly 50000 byte save reopens through real reader\n";
  for (bool existing : {false, true}) {
    reset();
    if (existing) {
      io.files[temp] = old;
      io.files[backup] = old;
    } else
      io = {};
    JsonDocument baseline;
    state.toJson(baseline);
    state.openEpubPath = std::string(50001 - measureJson(baseline), 'x');
    baseline.clear();
    state.toJson(baseline);
    assert(measureJson(baseline) == 50001);
    const auto files = io.files;
    io.mutations.clear();
    oversizedAllocations = 0;
    watchAllocations = true;
    const bool saved = state.saveToFile();
    watchAllocations = false;
    assert(!saved && io.mutations.empty() && io.files == files && oversizedAllocations == 0);
    std::cout << "PASS state exactly 50001 byte save zero mutations existing " << existing << '\n';
  }
  reset();
  io.files[temp] = old;
  io.files[backup] = old;
  state.openEpubPath = std::string(50000, 'x');
  const auto cappedFiles = io.files;
  io.mutations.clear();
  oversizedAllocations = 0;
  watchAllocations = true;
  const bool savedOversized = state.saveToFile();
  watchAllocations = false;
  assert(!savedOversized && io.mutations.empty() && io.files == cappedFiles && oversizedAllocations == 0);
  std::cout << "PASS state oversized save rejects before serialization allocation or candidate cleanup\n";
  reset();
  io.failAnyWriteOpen = true;
  assert(!state.saveToFile());
  assert(oldRetained());
  std::cout << "PASS state old document survives replacement failure\n";
  reset();
  assert(state.saveToFile());
  const auto committed = io.files[primary];
  io.files[primary] = "{broken";
  state.openEpubPath = "/sentinel.epub";
  assert(state.loadFromFile());
  assert(state.openEpubPath.empty() && state.favoriteBootImagePath == "/keep.png" &&
         state.readerActivityLoadCount == 27);
  assert(io.files[temp] == committed);
  const auto retained = io.files;
  io.mutations.clear();
  assert(!state.saveToFile());
  assert(io.mutations.empty() && io.files == retained);
  std::cout << "PASS state corrupt primary recovers latest tombstone without rotating backup\n";
  reset();
  io.failSyncNumber = 4;
  assert(!state.saveToFile());
  assert(oldRetained());
  std::cout << "PASS state late sync failure retains old independent document\n";
  for (const auto& root : {temp, pending, backup, primary}) {
    for (int fault = 0; fault < 5; ++fault) {
      reset();
      if (fault == 0) io.failWriteOpen = root;
      if (fault == 1) io.shortWrite = root;
      if (fault == 2) io.failSync = root;
      if (fault == 3) io.failClose = root;
      if (fault == 4) io.failRead = root;
      assert(!state.saveToFile());
      assert(oldRetained());
      std::cout << "PASS state checked boundary " << root << " fault " << fault << " retains old document\n";
    }
  }
  for (const auto& root : {primary, pending}) {
    reset();
    io.failRemove = root;
    assert(!state.saveToFile());
    assert(oldRetained());
    const auto files = io.files;
    io.failRemove.clear();
    io.mutations.clear();
    assert(state.loadFromFile());
    assert(!state.saveToFile());
    assert(io.mutations.empty() && io.files == files);
    std::cout << "PASS state failed removal retains uncertainty across load " << root << '\n';
  }
  reset();
  io.files[primary + ".bak"] = "legacy alias sentinel";
  io.files[primary + ".tmp"] = "legacy temp sentinel";
  io.failRename = temp;
  io.renameAliases = true;  // Neither success nor failure semantics are relied upon.
  assert(state.saveToFile());
  assert(state.saveToFile());
  assert(io.files[primary + ".bak"] == "legacy alias sentinel" && io.files[primary + ".tmp"] == "legacy temp sentinel");
  for (const auto& event : io.mutations) assert(event.find("rename:") != 0);
  assert(state.loadFromFile() && state.openEpubPath.empty() && state.isRecentSleep(3, 16));
  assert(state.readerActivityLoadCount == 27 && state.pendingBookmarkSpine == 7 && state.quickLockRestoreFrontlight);
  snapshot(argv[1]);
  std::cout << "PASS state repeated saves preserve legacy fields and never invoke FAT rename\n";
  // An acknowledged cleanup's latest copy is authoritative, never the stale old backup.
  io.files.erase(primary);
  io.files[temp] = "{broken";
  assert(!state.loadFromFile());
  std::cout << "PASS state stale backup cannot resurrect acknowledged cleared references\n";
  reset();
  io.shortWrite = primary;
  assert(!state.saveToFile());
  assert(oldRetained());
  snapshot(std::string(argv[1]) + ".uncertain");
  std::cout << "PASS state uncertain snapshot retains old and complete replacement\n";
  io = {};
  assert(!state.loadFromFile());
  std::string binary(1, static_cast<char>(1));
  const std::string legacyPath = "/legacy.epub";
  const uint32_t length = legacyPath.size();
  binary.append(reinterpret_cast<const char*>(&length), sizeof(length));
  binary += legacyPath;
  io.files["/.crosspoint/state.bin"] = binary;
  assert(state.loadFromFile());
  assert(state.openEpubPath == legacyPath);
  assert(io.files.count(primary) && io.files.count(temp));
  std::cout << "PASS state legacy binary migration still uses real codec and checked save\n";
  io.files[primary] = "{broken";
  io.files[temp] = "{broken";
  io.files["/.crosspoint/state.bin"] = binary;
  assert(!state.loadFromFile());
  std::cout << "PASS state damaged JSON roots never resurrect legacy binary references\n";
  reset();
  assert(state.saveToFile());
  io.failRemove = backup;
  state.readerActivityLoadCount = 28;
  assert(!state.saveToFile());  // retained stale old backup from the acknowledged cleanup
  io.files[primary] = "{broken";
  io.files[temp] = "{broken";
  assert(state.loadFromFile());
  assert(state.openEpubPath.empty());
  assert(state.readerActivityLoadCount == 27);
  std::cout << "PASS state pending recovery cannot revive a prior acknowledged deletion\n";
}
