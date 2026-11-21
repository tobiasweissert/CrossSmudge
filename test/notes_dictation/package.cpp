#include <fstream>
#include <iterator>

#include "AppPackage.h"
#include "NotesLaunch.h"
int main(int argc, char** argv) {
  assert(argc == 2);
  std::ifstream f(argv[1]);
  std::string manifest{std::istreambuf_iterator<char>(f), {}};
  assert(!manifest.empty());
  for (std::string dir : {"/apps/cp-notes", "/.crosssmudge/applications/cp-notes"}) {
    appfs::reset();
    appfs::directory(dir);
    appfs::files[dir + "/manifest.json"] = {manifest, false};
    auto apps = AppPackage::scanApplications();
    assert(apps.size() == 1);
    auto& app = apps.front();
    assert(app.version == "0.1.5");
    assert(dictation::firmwareRoute(app.path, app.path + "/" + app.entryScript) ==
           (dir == "/.crosssmudge/applications/cp-notes"));
  }
  puts("PASS actual installed package scan and versioned entry provenance in two roots");
}
