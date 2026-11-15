#pragma once
#include "Package.h"
namespace sdapps {
struct Catalog {
  Package entries[MAX_PACKAGES_PER_FOLDER];
  size_t count = 0;
};
// Caller owns the catalog and must not mutate it while a shelf render uses it.
void discoverPackages(const char* root, Catalog& catalog);
}
