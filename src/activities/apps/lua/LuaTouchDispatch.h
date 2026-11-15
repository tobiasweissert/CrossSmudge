#pragma once

// Caller holds the Lua mutex. Query before consuming: legacy apps retain their
// tap/swipe behavior, including long contacts. MappedInput owns suppression of
// the contact after a hold, so its release cannot also become a dig.
template <typename Input, typename Runner>
bool dispatchLuaTouch(Input& input, Runner& runner) {
  if (runner.hasActiveTouchHold() && !input.isScreenContactActive()) {
    runner.onTouchRelease();
    return true;
  }
  int x = 0, y = 0;
  const bool acceptsHold = runner.hasLongPressHandler();
  if (acceptsHold && input.wasScreenLongPress(x, y)) {
    runner.onLongPress(x, y);
    return true;
  }
  unsigned long heldMs = 0;
  const bool tapped = acceptsHold ? input.wasScreenTapped(x, y, heldMs) : input.wasScreenTapped(x, y);
  if (tapped) {
    // A long contact cancelled by movement must not fall back to a dig on lift.
    // Matches MappedInput's existing 500 ms hold threshold; legacy apps bypass it.
    if (acceptsHold && heldMs >= 500UL) return true;
    runner.onTap(x, y);
    runner.onTouch(x, y, "tap");
    return true;
  }
  return false;
}
