#pragma once

#include "LibraryActivity.h"

// Preserve the current launcher/ActivityManager constructor contract. There is
// one selected Library implementation and one derived index, not two shelves.
class LibraryListActivity final : public LibraryActivity {
 public:
  LibraryListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
};
