#include <cassert>
#include <cstdio>

#include "CrossPointSettings.h"
uint8_t CrossPointSettings::defaultUiScale() { return UI_SCALE_LARGE; }  // hardware preference double
int main() {
  auto& s = SETTINGS;
  JsonDocument owner;
  // Data from the owner's durable namespace is not a CrossPoint document.
  s.fontFamily = CrossPointSettings::BITTER;
  s.readerFontPointSize = 16;
  s.lineHeightPercent = 137;
  s.hideClock = CrossPointSettings::HIDE_CLOCK_NEVER;
  s.statusBarTimeLeft = CrossPointSettings::TIME_LEFT_BOOK;
  s.statusBarBookPercentageFormat = 2;
  s.clockUtcOffsetQ = 71;  // Nepal: +5:45, independent of UTC certificate clock.
  s.sideButtonLayout = CrossPointSettings::NEXT_NEXT;
  s.sideButtonLongPress = CrossPointSettings::SIDE_LONG_FONT_SIZE;
  s.readCompatibilitySettings(owner.as<JsonVariantConst>(), false);
  assert(s.fontFamily == CrossPointSettings::BITTER && s.fontPointSize == 16 && s.lineHeightPercent == 137);
  assert(s.clockUtcOffsetQ == 71 && s.clockTimezone == 255);
  assert(s.sideButtonUpShort == CrossPointSettings::PAGE_TURN &&
         s.sideButtonDownShort == CrossPointSettings::PAGE_TURN);
  assert(s.sideButtonUpLong == CrossPointSettings::SIDE_INCREASE_FONT &&
         s.sideButtonDownLong == CrossPointSettings::SIDE_DECREASE_FONT);
  assert(s.topReaderStatusBar.contains(ReaderStatusBarItem::Clock));
  assert(s.bottomReaderStatusBar.contains(ReaderStatusBarItem::TimeLeftBook));
  assert(s.bottomReaderStatusBar.percentageFormat == 2);
  JsonDocument saved;
  s.trackReadingStats = 0;
  s.clockTimezone = 24;
  s.clockDst = 2;
  s.librarySortMethod = 6;
  s.leftEdgeUp = 1;
  s.topReaderStatusBar.slots[0] = ReaderStatusBarItem::Date;
  s.writeCompatibilitySettings(saved);
  s.trackReadingStats = 1;
  s.librarySortMethod = 0;
  s.clockTimezone = 255;
  s.leftEdgeUp = 0;
  s.readCompatibilitySettings(saved.as<JsonVariantConst>(), false);
  assert(!s.shouldTrackReadingStats() && s.librarySortMethod == 6 && s.clockTimezone == 24 && s.clockDst == 2 &&
         s.leftEdgeUp == 1);
  assert(s.topReaderStatusBar.slots[0] == ReaderStatusBarItem::Date);
  assert(s.fontFamily == CrossPointSettings::BITTER && s.lineHeightPercent == 137);
  // Partial modern shortcut settings preserve explicit values and derive only missing keys.
  JsonDocument partial;
  partial["sideButtonUpShort"] = CrossPointSettings::QUICK_LOCK;
  s.readCompatibilitySettings(partial.as<JsonVariantConst>(), false);
  assert(s.sideButtonUpShort == CrossPointSettings::QUICK_LOCK &&
         s.sideButtonDownShort == CrossPointSettings::PAGE_TURN);
  // Upstream raw IDs do not mean the owner's enum values.
  JsonDocument upstream;
  upstream["fontFamily"] = 1;
  upstream["fontSize"] = 18;
  upstream["sleepScreen"] = 4;
  upstream["shortPwrBtn"] = 4;
  upstream["lineHeight"] = 4;
  s.readCompatibilitySettings(upstream.as<JsonVariantConst>(), true);
  assert(s.fontFamily == CrossPointSettings::NOTOSANS && s.fontPointSize == 18 &&
         s.sleepScreen == CrossPointSettings::COVER_CUSTOM);
  assert(s.shortPwrBtn == CrossPointSettings::FOOTNOTES && s.lineHeightPercent == 145);
  JsonDocument canonical;
  canonical["shortPwrBtn"] = CrossPointSettings::PWR_READER_MENU;
  canonical["imageRendering"] = 3;
  s.shortPwrBtn = CrossPointSettings::IGNORE;
  s.imageRendering = 0;
  s.readCompatibilitySettings(canonical.as<JsonVariantConst>(), false);
  assert(s.shortPwrBtn == CrossPointSettings::PWR_READER_MENU && s.imageRendering == 3);
  assert(CrossPointSettings::LEXENDDECA == 0 && CrossPointSettings::BITTER == 1 &&
         CrossPointSettings::QUICK_LOCK == 30);
  puts(
      "PASS owner settings, independent bars, partial shortcuts, library, stats, timezone and upstream enum migration");
}
