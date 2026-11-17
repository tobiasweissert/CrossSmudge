#pragma once

namespace AppStoreSource {
// One root controls catalog, icons and every package dependency. Catalog metadata
// never redirects downloads to a different host or branch.
// Repository name is retained for source compatibility; this is Crosscub's
// live owner-controlled 41-app catalog, not this branch's 37-source inventory.
// Never replace the live catalog with the smaller source-baseline catalog.
#define CROSSSMUDGE_STORE_ROOT "https://raw.githubusercontent.com/tobiasweissert/CrossSmudge/integration/sticky-test-all/apps/"
static constexpr const char* baseUrl = CROSSSMUDGE_STORE_ROOT;
static constexpr const char* catalogUrl = CROSSSMUDGE_STORE_ROOT "catalog.json";
#undef CROSSSMUDGE_STORE_ROOT

inline bool safePackagePath(const char* path) {
  if (!path || !*path) return false;
  unsigned component = 0;
  unsigned length = 0;
  for (const char* p = path; *p; ++p) {
    if (++length > 192) return false;
    if (*p == '/') {
      if (!component) return false;
      component = 0;
    } else {
      const bool alpha = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z');
      const bool digit = *p >= '0' && *p <= '9';
      if (!(alpha || digit || *p == '_' || *p == '-' || *p == '.')) return false;
      // No hidden files or dot components; prevents traversal and URL escapes.
      if (!component && *p == '.') return false;
      ++component;
    }
  }
  return component != 0;
}
}  // namespace AppStoreSource
