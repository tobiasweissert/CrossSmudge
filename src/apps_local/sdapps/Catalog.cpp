#include "Catalog.h"
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <cstdio>
#include <cstring>

namespace sdapps {
namespace {
bool readManifest(const char* root, const char* id, char* scratch, Package& out) {
  char path[96];
  const int n = snprintf(path, sizeof(path), "%s/%s/app.cfg", root, id);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("SDAPP", path, file) || file.isDirectory()) return false;
  const auto length = file.fileSize64();
  if (!length || length > MAX_MANIFEST_BYTES || file.read(scratch, length) != static_cast<int>(length)) return false;
  return parsePackage(scratch, length, id, out);
}
}
void discoverPackages(const char* root, Catalog& catalog) {
  catalog.count = 0;
  if (!root || (strcmp(root, "/apps") && strcmp(root, "/games"))) return;
  if (!Storage.exists(root)) return;
  auto directory = Storage.open(root);
  if (!directory || !directory.isDirectory()) return;
  // One 320-byte scratch allocation per scan, reused for every manifest;
  // larger than the stack budget and unnecessary between shelf visits.
  auto scratch = makeUniqueNoThrow<char[]>(MAX_MANIFEST_BYTES);
  if (!scratch) {
    LOG_ERR("SDAPP", "OOM scanning package manifests: %u bytes", unsigned(MAX_MANIFEST_BYTES));
    return;
  }
  // Bound SD work even on a directory filled with invalid entries.
  for (unsigned visited = 0; visited < 256 && catalog.count < MAX_PACKAGES_PER_FOLDER; ++visited) {
    auto entry = directory.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) continue;
    char id[33] = {};
    const size_t length = entry.getName(id, sizeof(id));
    if (!length || length >= 32 || !validPackageId(id)) continue;
    auto& package = catalog.entries[catalog.count];
    if (!readManifest(root, id, scratch.get(), package)) continue;
    bool duplicate = false;
    for (size_t i = 0; i < catalog.count; ++i) {
      if (!strcmp(catalog.entries[i].id, package.id)) duplicate = true;
    }
    if (!duplicate) ++catalog.count;
  }
  for (size_t i = 1; i < catalog.count; ++i) {
    const Package item = catalog.entries[i];
    size_t j = i;
    while (j && strcmp(catalog.entries[j - 1].id, item.id) > 0) {
      catalog.entries[j] = catalog.entries[j - 1];
      --j;
    }
    catalog.entries[j] = item;
  }
}
}
