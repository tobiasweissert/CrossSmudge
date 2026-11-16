#include "AppPackage.h"

#include <cassert>
#include <cstdio>
#include <string>

using Error = AppPackage::BrowserResolveError;
static const std::string Manifest = "{\"id\":\"browser\",\"name\":\"Browser\",\"entry\":\"main.lua\",\"browser\":true,\"requires_psram\":true,\"min_api\":6}";
[[maybe_unused]] static constexpr const char* Upper = "/.crosssmudge/applications";
static void install(const std::string& root, const std::string& json = Manifest) {
  appfs::directory(root + "/browser");
  appfs::files[root + "/browser/manifest.json"] = {json, false};
  appfs::files[root + "/browser/main.lua"] = {"browser_launch_api=1", false};
}
static void rejected(Error expected) {
  AppPackage output; output.path = "preserved"; Error error;
  assert(!AppPackage::resolveInstalledBrowser(output, error) && error == expected);
  assert(output.path == "preserved" && appfs::opened == 0);
}
[[maybe_unused]] static void noFallbackIo() {
  for (const char* failed : {"/", "/.crosssmudge", "/.crosssmudge/applications", "/.crosssmudge/applications/browser"}) {
    appfs::reset(); install("/apps"); install(Upper); appfs::files[failed].openFailure = true;
    rejected(Error::StorageIo);
  }
  for (const char* failed : {"/", "/.crosssmudge", "/.crosssmudge/applications", "/.crosssmudge/applications/browser"}) {
    for (unsigned fault = 0; fault < 3; ++fault) {
      appfs::reset(); install("/apps"); install(Upper); auto& node = appfs::files[failed];
      if (fault == 0) node.iterationFailure = true;
      if (fault == 1) node.allocationFailure = true;
      if (fault == 2) node.closeFailure = true;
      rejected(Error::StorageIo);
    }
  }
  std::puts("PASS unreadable earlier root/folder/iteration/allocation/close never falls through");
}
[[maybe_unused]] static void packageReads() {
  const std::string folder = std::string(Upper) + "/browser";
  for (unsigned fault = 0; fault < 5; ++fault) {
    appfs::reset(); install("/apps"); install(Upper);
    if (fault == 0) appfs::files[folder + "/manifest.json"].shortRead = true;
    if (fault == 1) appfs::files[folder + "/manifest.json"].closeFailureAt = 2;
    if (fault == 2) appfs::files[folder + "/main.lua"].closeFailureAt = 3;
    if (fault == 3) appfs::files[folder + "/main.lua"].openFailure = true;
    if (fault == 4) appfs::files[folder + "/manifest.json"].nameFailure = true;
    rejected(Error::StorageIo);
  }
  appfs::reset(); install("/apps"); appfs::directory(folder);
  rejected(Error::InvalidManifest); // Earlier empty/stale package is not permission to use another copy.
  appfs::reset(); install("/apps"); install(Upper); appfs::files.erase(folder + "/manifest.json");
  rejected(Error::InvalidManifest);
  assert(appfs::existsCalls == 0);
  std::puts("PASS manifest/script short reads and close failures are checked before launch");
}
[[maybe_unused]] static void precedenceAndPolicy() {
  appfs::reset(); rejected(Error::Missing); assert(appfs::existsCalls == 0);
  appfs::directory("/.crosssmudge"); // Healthy missing nested applications root.
  install("/apps");
  AppPackage package; Error error;
  assert(AppPackage::resolveInstalledBrowser(package, error) && package.path == "/apps/browser");
  install("/applications");
  assert(AppPackage::resolveInstalledBrowser(package, error) && package.path == "/applications/browser");
  install(Upper);
  assert(AppPackage::resolveInstalledBrowser(package, error) && package.path == std::string(Upper) + "/browser");
  install(Upper, "{\"id\":\"browser\",\"browser\":true,\"requires_psram\":true,\"min_api\":5}"); rejected(Error::Incompatible);
  install(Upper, "{\"id\":\"notbrowser\",\"browser\":true,\"requires_psram\":true,\"min_api\":6}"); rejected(Error::InvalidManifest);
  install(Upper, "{\"id\":\"browser\",\"entry\":\"../evil.lua\",\"browser\":true,\"requires_psram\":true,\"min_api\":6}"); rejected(Error::InvalidEntry);
  install(Upper, std::string(4097, 'x')); rejected(Error::InvalidManifest);
  install(Upper, "{\"id\":\"browser\\u0000other\",\"browser\":true,\"requires_psram\":true,\"min_api\":6}"); rejected(Error::InvalidManifest);
  install(Upper, "{\"id\":\"browser\",\"entry\":\"main.lua\\u0000other\",\"browser\":true,\"requires_psram\":true,\"min_api\":6}"); rejected(Error::InvalidEntry);
  install(Upper, "{\"id\":\"browser\",\"entry\":123,\"browser\":true,\"requires_psram\":true,\"min_api\":6}"); rejected(Error::InvalidEntry);
  assert(appfs::opened == 0);
  std::puts("PASS precedence, missing/stale/API/entry policy and real Storage macro");
}
[[maybe_unused]] static void boundedNamesAndWork() {
  appfs::reset(); install("/apps");
  std::string utf8;
  for (unsigned i = 0; i < 255; ++i) utf8 += "\xe5\xad\x97";
  appfs::files["/" + utf8] = {"irrelevant", false};
  AppPackage package; Error error;
  assert(AppPackage::resolveInstalledBrowser(package, error)); // Full FAT UTF-8 names fit the reused buffer.
  appfs::reset(); install("/apps");
  for (unsigned i = 0; i < 2047; ++i) {
    char name[48]; std::snprintf(name, sizeof name, "/apps/a%04u", i); appfs::files[name] = {"", false};
  }
  assert(AppPackage::resolveInstalledBrowser(package, error));
  appfs::files["/apps/a2048"] = {"", false}; rejected(Error::StorageIo);
  assert(appfs::opened == 0);
  std::puts("PASS complete UTF-8 FAT names and directory count limit/+1");
}
int main() {
#ifdef TEST_NO_PSRAM
  appfs::reset(); install("/apps"); rejected(Error::Incompatible); return 0;
#endif
  noFallbackIo(); packageReads(); precedenceAndPolicy(); boundedNamesAndWork();
}
