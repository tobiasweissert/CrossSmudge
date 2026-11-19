#include <HalStorage.h>
#include <MemoryBudget.h>
#include <SdCardFontRegistry.h>

#include <cassert>
#include <fstream>
static void put(const char* path) {
  std::filesystem::create_directories(std::filesystem::path(testRoot + path).parent_path());
  std::ofstream(testRoot + path) << "font";
}
int main(int argc, char** argv) {
  assert(argc == 2);
  testRoot = argv[1];
  put("/.fonts/Bitmap/Bitmap_12.cpfont");
  put("/.fonts/Bitmap/Bitmap_16.cpfont");
  put("/fonts/Bitmap/Bitmap_20.cpfont");
  put("/.fonts/Vector/Regular.otf");
  put("/.fonts/Vector/Bold.ttf");
  put("/fonts/Loose.ttc");
  SdCardFontRegistry r;
  uint8_t parsedSize = 0, parsedStyle = 0;
  assert(SdCardFontRegistry::parseFilename("Upper_14.CPFONT", parsedSize, parsedStyle) && parsedSize == 14);
  assert(r.loadNames());
  assert(!r.lastDiscoveryFailed());
  auto* v = r.findFamily("Vector");
  assert(v && v->vector);
  assert(v->files.size() == 2);
  char path[160];
  assert(v->filePath(v->files[0], path, sizeof(path)));
  v->releaseDetails();
  assert(v->files.empty());
  assert(v->ensureDetails());
  assert(v->files.size() == 2);
  auto* loose = r.findFamily("Loose");
  assert(loose && loose->vector && loose->loose);
  auto* b = r.findFamily("Bitmap");
  assert(b && !b->vector && b->findClosestFile(14)->pointSize == 12);
  assert(!b->hasSize(20));
  b->releaseDetails();
  assert(b->ensureDetails());
  auto steps = directorySteps;
  r.clear();
  assert(r.loadNames());
  assert(steps == directorySteps);
  auto revision = r.revision();
  SdCardFontRegistry::invalidateIndex();
  assert(r.needsRefresh());
  assert(r.loadNames());
  assert(r.revision() > revision);
  MemoryBudget::available = 100;
  r.clear();
  assert(!r.loadNames());
  assert(r.lastDiscoveryFailed());
  MemoryBudget::available = 1024 * 1024;
  assert(r.loadNames());
  failDirectoryScan = true;
  assert(!r.loadNames(true));
  assert(r.lastDiscoveryFailed());
  failDirectoryScan = false;
  assert(r.loadNames(true));
}
