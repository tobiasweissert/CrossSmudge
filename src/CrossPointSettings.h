#pragma once
#include <ArduinoJson.h>
#include <BlePageTurner.h>
#include <BlePageTurnerJson.h>
#include <Epub/ReaderRenderSpec.h>
#include <HalStorage.h>
#include <PersistableStore.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <mutex>

#include "ReaderFontSizeStep.h"

enum class ReaderStatusBarPosition : uint8_t { Top, Bottom };

constexpr ReaderStatusBarPosition xtcStatusBarConfigPosition(const ReaderStatusBarPosition displayed,
                                                             const bool legacyTopUsesBottom) {
  return displayed == ReaderStatusBarPosition::Top && legacyTopUsesBottom ? ReaderStatusBarPosition::Bottom : displayed;
}

// The stored values are deliberately independent of the order shown in the picker.
enum class ReaderStatusBarItem : uint8_t {
  Empty = 0,
  Clock,
  Battery,
  TimeLeftBook,
  TimeLeftChapter,
  ChapterPageCount,
  StablePageNumber,
  BookProgressPercentage,
  TitleBook,
  TitleChapter,
  Date,
  Count,
};

constexpr bool validReaderStatusBarItemValue(const int value, const bool clockAvailable) {
  return value >= 0 && value < static_cast<int>(ReaderStatusBarItem::Count) &&
         ((value != static_cast<int>(ReaderStatusBarItem::Clock) &&
           value != static_cast<int>(ReaderStatusBarItem::Date)) ||
          clockAvailable);
}

constexpr bool validReaderStatusBarChoice(const int value, const int optionCount) {
  return value >= 0 && value < optionCount;
}

struct ReaderStatusBarConfig {
  static constexpr int TOP_TEXT_INSET = 2;
  static constexpr int BOOKMARK_WIDTH = 9;
  static constexpr int BOOKMARK_HEIGHT = 14;
  static constexpr unsigned SLOT_COUNT = 7;
  static constexpr unsigned LEFT_FIRST = 0;
  static constexpr unsigned LEFT_SECOND = 1;
  static constexpr unsigned LEFT_THIRD = 2;
  static constexpr unsigned CENTER = 3;
  static constexpr unsigned RIGHT_FIRST = 4;
  static constexpr unsigned RIGHT_SECOND = 5;
  static constexpr unsigned RIGHT_THIRD = 6;

  std::array<ReaderStatusBarItem, SLOT_COUNT> slots{};
  uint8_t percentageFormat = 0;
  uint8_t progressBar = 2;
  uint8_t progressBarThickness = 1;

  constexpr bool contains(ReaderStatusBarItem item) const {
    return std::any_of(slots.begin(), slots.end(), [item](const auto slot) { return slot == item; });
  }

  constexpr bool hasTextItems(bool clockAvailable) const {
    return std::any_of(slots.begin(), slots.end(), [clockAvailable](const auto slot) {
      return slot != ReaderStatusBarItem::Empty &&
             ((slot != ReaderStatusBarItem::Clock && slot != ReaderStatusBarItem::Date) || clockAvailable);
    });
  }
};

constexpr bool validDisplayStatusBarItemValue(const int value, const bool clockAvailable) {
  return value == static_cast<int>(ReaderStatusBarItem::Empty) ||
         (clockAvailable && (value == static_cast<int>(ReaderStatusBarItem::Clock) ||
                             value == static_cast<int>(ReaderStatusBarItem::Date))) ||
         value == static_cast<int>(ReaderStatusBarItem::Battery);
}

struct DisplayStatusBarConfig {
  std::array<ReaderStatusBarItem, 3> slots{ReaderStatusBarItem::Empty, ReaderStatusBarItem::Empty,
                                           ReaderStatusBarItem::Battery};

  ReaderStatusBarConfig asReaderConfig() const {
    ReaderStatusBarConfig config;
    config.progressBar = 2;
    config.slots[ReaderStatusBarConfig::LEFT_FIRST] = slots[0];
    config.slots[ReaderStatusBarConfig::CENTER] = slots[1];
    config.slots[ReaderStatusBarConfig::RIGHT_FIRST] = slots[2];
    return config;
  }
};

struct ReaderStatusBarsPayload {
  ReaderStatusBarConfig top;
  ReaderStatusBarConfig bottom;
  uint8_t xtcMode = 0;
};

constexpr int readerStatusBarTotalHeight(const ReaderStatusBarPosition position, const bool hasText,
                                         const int progressSpace, const int textLaneHeight) {
  if (!hasText && progressSpace > 0 && position == ReaderStatusBarPosition::Bottom) {
    return std::max(progressSpace, ReaderStatusBarConfig::BOOKMARK_HEIGHT);
  }
  return (hasText
              ? textLaneHeight + (position == ReaderStatusBarPosition::Top ? ReaderStatusBarConfig::TOP_TEXT_INSET : 0)
              : 0) +
         progressSpace;
}

struct LegacyReaderStatusBarSettings {
  bool chapterPageCount = false;
  bool stablePageNumbers = false;
  bool bookProgressPercentage = false;
  uint8_t titleMode = 2;
  uint8_t timeLeftMode = 0;
  bool battery = false;
  uint8_t percentageFormat = 0;
  uint8_t progressBar = 2;
  uint8_t progressBarThickness = 1;
};

constexpr ReaderStatusBarConfig migrateBottomStatusBar(const LegacyReaderStatusBarSettings& old) {
  ReaderStatusBarConfig result;
  result.percentageFormat = old.percentageFormat;
  result.progressBar = old.progressBar;
  result.progressBarThickness = old.progressBarThickness;
  unsigned left = ReaderStatusBarConfig::LEFT_FIRST;
  unsigned right = ReaderStatusBarConfig::RIGHT_FIRST;
  if (old.battery) result.slots[left++] = ReaderStatusBarItem::Battery;
  if (old.timeLeftMode == 1) result.slots[left++] = ReaderStatusBarItem::TimeLeftChapter;
  if (old.timeLeftMode == 2) result.slots[left++] = ReaderStatusBarItem::TimeLeftBook;
  if (old.titleMode == 0) result.slots[ReaderStatusBarConfig::CENTER] = ReaderStatusBarItem::TitleBook;
  if (old.titleMode == 1) result.slots[ReaderStatusBarConfig::CENTER] = ReaderStatusBarItem::TitleChapter;
  if (old.chapterPageCount) result.slots[right++] = ReaderStatusBarItem::ChapterPageCount;
  if (old.stablePageNumbers) result.slots[right++] = ReaderStatusBarItem::StablePageNumber;
  if (old.bookProgressPercentage) result.slots[right++] = ReaderStatusBarItem::BookProgressPercentage;
  return result;
}

#include <ArduinoJson.h>

inline bool readDisplayStatusBarJson(const JsonVariantConst source, DisplayStatusBarConfig& config,
                                     const bool clockAvailable) {
  const auto slots = source.as<JsonArrayConst>();
  if (slots.size() != config.slots.size()) return false;
  DisplayStatusBarConfig parsed;
  for (unsigned i = 0; i < parsed.slots.size(); ++i) {
    if (!slots[i].is<int>() || !validDisplayStatusBarItemValue(slots[i].as<int>(), clockAvailable)) return false;
    parsed.slots[i] = static_cast<ReaderStatusBarItem>(slots[i].as<int>());
  }
  config = parsed;
  return true;
}

inline void writeReaderStatusBarJson(JsonObject target, const ReaderStatusBarConfig& config) {
  JsonArray slots = target["slots"].to<JsonArray>();
  for (const auto item : config.slots) slots.add(static_cast<uint8_t>(item));
  target["percentageFormat"] = config.percentageFormat;
  target["progressBar"] = config.progressBar;
  target["thickness"] = config.progressBarThickness;
}

inline bool readReaderStatusBarJson(const JsonVariantConst source, ReaderStatusBarConfig& config,
                                    const bool clockAvailable, const int percentageFormatCount,
                                    const int progressModeCount, const int thicknessCount) {
  const JsonArrayConst slots = source["slots"].as<JsonArrayConst>();
  if (slots.size() != ReaderStatusBarConfig::SLOT_COUNT) return false;
  ReaderStatusBarConfig parsed;
  for (unsigned i = 0; i < ReaderStatusBarConfig::SLOT_COUNT; ++i) {
    const int item = slots[i].as<int>();
    if (!slots[i].is<int>() || !validReaderStatusBarItemValue(item, clockAvailable)) return false;
    parsed.slots[i] = static_cast<ReaderStatusBarItem>(item);
  }
  const int percentageFormat = source["percentageFormat"].as<int>();
  const int progressMode = source["progressBar"].as<int>();
  const int thickness = source["thickness"].as<int>();
  if (!source["percentageFormat"].is<int>() || !validReaderStatusBarChoice(percentageFormat, percentageFormatCount) ||
      !source["progressBar"].is<int>() || !validReaderStatusBarChoice(progressMode, progressModeCount) ||
      !source["thickness"].is<int>() || !validReaderStatusBarChoice(thickness, thicknessCount)) {
    return false;
  }
  parsed.percentageFormat = percentageFormat;
  parsed.progressBar = progressMode;
  parsed.progressBarThickness = thickness;
  config = parsed;
  return true;
}

inline bool readReaderStatusBarsPayload(const JsonVariantConst source, ReaderStatusBarsPayload& payload,
                                        const bool clockAvailable, const int percentageFormatCount,
                                        const int progressModeCount, const int thicknessCount, const int xtcModeCount) {
  ReaderStatusBarsPayload parsed;
  const int xtcMode = source["xtcMode"].as<int>();
  if (!readReaderStatusBarJson(source["top"], parsed.top, clockAvailable, percentageFormatCount, progressModeCount,
                               thicknessCount) ||
      !readReaderStatusBarJson(source["bottom"], parsed.bottom, clockAvailable, percentageFormatCount,
                               progressModeCount, thicknessCount) ||
      !source["xtcMode"].is<int>() || !validReaderStatusBarChoice(xtcMode, xtcModeCount)) {
    return false;
  }
  parsed.xtcMode = static_cast<uint8_t>(xtcMode);
  payload = parsed;
  return true;
}

// Repairs a damaged saved bar one field at a time, retaining valid choices.
// Returns true when the caller must resave the corrected document.
inline bool repairReaderStatusBarJson(const JsonVariantConst source, ReaderStatusBarConfig& config,
                                      const bool clockAvailable, const int percentageFormatCount,
                                      const int progressModeCount, const int thicknessCount) {
  if (readReaderStatusBarJson(source, config, clockAvailable, percentageFormatCount, progressModeCount,
                              thicknessCount)) {
    return false;
  }
  const JsonArrayConst slots = source["slots"].as<JsonArrayConst>();
  // The previous layout had two left slots, one center slot, and three right slots.
  constexpr unsigned LEGACY_SLOT_COUNT = ReaderStatusBarConfig::SLOT_COUNT - 1;
  if (slots.size() != LEGACY_SLOT_COUNT && slots.size() != ReaderStatusBarConfig::SLOT_COUNT) return true;
  ReaderStatusBarConfig repaired;
  for (unsigned i = 0; i < slots.size(); ++i) {
    const int item = slots[i].as<int>();
    if (slots[i].is<int>() && validReaderStatusBarItemValue(item, clockAvailable)) {
      const unsigned target = slots.size() == LEGACY_SLOT_COUNT && i >= 2 ? i + 1 : i;
      repaired.slots[target] = static_cast<ReaderStatusBarItem>(item);
    }
  }
  const auto choice = [](const JsonVariantConst value, const int count, const uint8_t fallback) {
    const int raw = value.as<int>();
    return value.is<int>() && validReaderStatusBarChoice(raw, count) ? static_cast<uint8_t>(raw) : fallback;
  };
  repaired.percentageFormat = choice(source["percentageFormat"], percentageFormatCount, repaired.percentageFormat);
  repaired.progressBar = choice(source["progressBar"], progressModeCount, repaired.progressBar);
  repaired.progressBarThickness = choice(source["thickness"], thicknessCount, repaired.progressBarThickness);
  config = repaired;
  return true;
}

class CrossPointSettings : public PersistableStore<CrossPointSettings> {
 private:
  mutable std::mutex _mutex;

  CrossPointSettings() = default;
  friend class PersistableStore<CrossPointSettings>;

 public:
  // Access the settings mutex for protecting multi-field reads/writes from other cores.
  // Callers must not re-enter SETTINGS methods that lock _mutex while holding it.
  std::mutex& getMutex() const { return _mutex; }

  enum SLEEP_SCREEN_MODE {
    DARK = 0,
    LIGHT = 1,
    CUSTOM = 2,
    COVER = 3,
    BLANK = 4,
    COVER_CUSTOM = 5,
    OVERLAY = 6,
    READING_STATS_SLEEP = 7,
    MINIMAL_SLEEP = 8,
    QUICK_RESUME = 9,
    MINIMAL_STATS_SLEEP = 10,
    DASHBOARD_SLEEP = 11,
    TRANSPARENT_CUSTOM = 12,
    COVER_TRANSPARENT = 13,
    SLEEP_SCREEN_MODE_COUNT
  };
  enum SLEEP_SCREEN_COVER_MODE { FIT = 0, CROP = 1, SLEEP_SCREEN_COVER_MODE_COUNT };
  enum SLEEP_SCREEN_COVER_FILTER {
    NO_FILTER = 0,
    BLACK_AND_WHITE = 1,
    INVERTED_BLACK_AND_WHITE = 2,
    SLEEP_SCREEN_COVER_FILTER_COUNT
  };

  // Status bar enum - legacy
  enum STATUS_BAR_MODE {
    NONE = 0,
    NO_PROGRESS = 1,
    FULL = 2,
    BOOK_PROGRESS_BAR = 3,
    ONLY_BOOK_PROGRESS_BAR = 4,
    CHAPTER_PROGRESS_BAR = 5,
    STATUS_BAR_MODE_COUNT
  };
  enum STATUS_BAR_PROGRESS_BAR {
    BOOK_PROGRESS = 0,
    CHAPTER_PROGRESS = 1,
    HIDE_PROGRESS = 2,
    STATUS_BAR_PROGRESS_BAR_COUNT
  };
  enum STATUS_BAR_PROGRESS_BAR_THICKNESS {
    PROGRESS_BAR_THIN = 0,
    PROGRESS_BAR_NORMAL = 1,
    PROGRESS_BAR_THICK = 2,
    STATUS_BAR_PROGRESS_BAR_THICKNESS_COUNT
  };
  enum BOOK_PERCENTAGE_FORMAT {
    BOOK_PERCENTAGE_WHOLE = 0,
    BOOK_PERCENTAGE_ONE_DECIMAL = 1,
    BOOK_PERCENTAGE_TWO_DECIMALS = 2,
    BOOK_PERCENTAGE_FORMAT_COUNT
  };
  enum STATUS_BAR_TITLE { BOOK_TITLE = 0, CHAPTER_TITLE = 1, HIDE_TITLE = 2, STATUS_BAR_TITLE_COUNT };
  enum STATUS_BAR_TIME_LEFT {
    TIME_LEFT_HIDE = 0,
    TIME_LEFT_CHAPTER = 1,
    TIME_LEFT_BOOK = 2,
    STATUS_BAR_TIME_LEFT_COUNT
  };
  enum XTC_STATUS_BAR_MODE {
    XTC_STATUS_BAR_HIDE = 0,
    XTC_STATUS_BAR_BOTTOM = 1,
    XTC_STATUS_BAR_TOP = 2,
    XTC_STATUS_BAR_MODE_COUNT
  };
  enum HIDE_CLOCK_MODE { HIDE_CLOCK_NEVER = 0, HIDE_CLOCK_IN_READER = 1, HIDE_CLOCK_ALWAYS = 2, HIDE_CLOCK_MODE_COUNT };
  // Persisted date-format values mirror HalClock::DateFormat.
  enum DATE_FORMAT {
    DATE_FORMAT_MONTH_DAY_YEAR_LONG = 0,
    DATE_FORMAT_DAY_MONTH_YEAR_LONG = 1,
    DATE_FORMAT_MONTH_DAY_YEAR_NUMERIC = 2,
    DATE_FORMAT_DAY_MONTH_YEAR_NUMERIC = 3,
    DATE_FORMAT_YEAR_MONTH_DAY_NUMERIC = 4,
    DATE_FORMAT_MONTH_DAY_NUMERIC = 5,
    DATE_FORMAT_DAY_MONTH_NUMERIC = 6,
    DATE_FORMAT_MONTH_DAY_LONG = 7,
    DATE_FORMAT_DAY_MONTH_LONG = 8,
    DATE_FORMAT_COUNT
  };
  enum DATE_SEPARATOR {
    DATE_SEPARATOR_PERIOD = 0,
    DATE_SEPARATOR_HYPHEN = 1,
    DATE_SEPARATOR_SLASH = 2,
    DATE_SEPARATOR_COUNT
  };

  enum ORIENTATION {
    PORTRAIT = 0,       // 480x800 logical coordinates (current default)
    LANDSCAPE_CW = 1,   // 800x480 logical coordinates, rotated 180° (swap top/bottom)
    INVERTED = 2,       // 480x800 logical coordinates, inverted
    LANDSCAPE_CCW = 3,  // 800x480 logical coordinates, native panel orientation
    ORIENTATION_COUNT
  };

  // Front button layout options (legacy)
  // Default: Back, Confirm, Left, Right
  // Swapped: Left, Right, Back, Confirm
  enum FRONT_BUTTON_LAYOUT {
    BACK_CONFIRM_LEFT_RIGHT = 0,
    LEFT_RIGHT_BACK_CONFIRM = 1,
    LEFT_BACK_CONFIRM_RIGHT = 2,
    BACK_CONFIRM_RIGHT_LEFT = 3,
    FRONT_BUTTON_LAYOUT_COUNT
  };

  // Front button hardware identifiers (for remapping)
  enum FRONT_BUTTON_HARDWARE {
    FRONT_HW_BACK = 0,
    FRONT_HW_CONFIRM = 1,
    FRONT_HW_LEFT = 2,
    FRONT_HW_RIGHT = 3,
    FRONT_BUTTON_HARDWARE_COUNT
  };

  // Side button layout options
  // Default: Up = Previous, Down = Next
  enum SIDE_BUTTON_LAYOUT {
    PREV_NEXT = 0,
    NEXT_PREV = 1,
    SIDE_BUTTONS_DISABLED = 2,
    NEXT_NEXT = 3,
    PREV_PREV = 4,
    SIDE_BUTTON_LAYOUT_COUNT
  };

  enum SIDE_BUTTON_ACTION : uint8_t {
    SIDE_PREVIOUS_CHAPTER = 64,
    SIDE_NEXT_CHAPTER,
    SIDE_INCREASE_FONT,
    SIDE_DECREASE_FONT,
    SIDE_ROTATE_COUNTERCLOCKWISE,
    SIDE_ROTATE_CLOCKWISE,
    SIDE_ROTATE_FLIP,
  };
  enum FRONT_BUTTON_ORIENTATION_AWARE {
    FRONT_ORIENTATION_AWARE_OFF = 0,
    FRONT_ORIENTATION_AWARE_NAV_BUTTONS = 1,
    FRONT_ORIENTATION_AWARE_ALL_BUTTONS = 2,
    FRONT_ORIENTATION_AWARE_COUNT
  };

  enum TWO_FINGER_SWIPE_ACTION {
    TWO_FINGER_SWIPE_NOT_SET = 0,
    TWO_FINGER_SWIPE_INCREASE_BRIGHTNESS,
    TWO_FINGER_SWIPE_DECREASE_BRIGHTNESS,
    TWO_FINGER_SWIPE_INCREASE_WARMTH,
    TWO_FINGER_SWIPE_DECREASE_WARMTH,
    TWO_FINGER_SWIPE_NEXT_CHAPTER,
    TWO_FINGER_SWIPE_PREVIOUS_CHAPTER,
    TWO_FINGER_SWIPE_INCREASE_FONT_SIZE,
    TWO_FINGER_SWIPE_DECREASE_FONT_SIZE,
    TWO_FINGER_SWIPE_ACTION_COUNT,
  };

  // Side button long-press action options
  enum SIDE_LONG_PRESS {
    SIDE_LONG_CHAPTER_SKIP = 0,
    SIDE_LONG_FONT_SIZE = 1,
    SIDE_LONG_OFF = 2,
    SIDE_LONG_ORIENTATION_CHANGE = 3,
    SIDE_LONG_PRESS_COUNT
  };

  // Font family options (built-in fonts only; SD card fonts use sdFontFamilyName)
  enum FONT_FAMILY { LEXENDDECA = 0, BITTER = 1, NOTOSANS = 2, FONT_FAMILY_COUNT };
  static constexpr uint8_t BUILTIN_FONT_COUNT = FONT_FAMILY_COUNT;
  // Font size options
  enum FONT_SIZE { TINY = 0, SMALL = 1, MEDIUM = 2, LARGE = 3, FONT_SIZE_COUNT };
  enum SD_FONT_SIZE_RANGE {
    SD_FONT_RANGE_TEENSY = 0,
    SD_FONT_RANGE_TINY = 1,
    SD_FONT_RANGE_XLARGE = 2,
    SD_FONT_RANGE_NO_EMOJI_LEGACY = 3,
    SD_FONT_RANGE_ALL = 4,
    SD_FONT_SIZE_RANGE_COUNT
  };
  // Legacy persisted values for the old Tight / Normal / Wide line-spacing setting.
  enum LINE_COMPRESSION { TIGHT = 0, NORMAL = 1, WIDE = 2, LINE_COMPRESSION_COUNT };
  enum PARAGRAPH_ALIGNMENT {
    JUSTIFIED = 0,
    LEFT_ALIGN = 1,
    CENTER_ALIGN = 2,
    RIGHT_ALIGN = 3,
    BOOK_STYLE = 4,
    PARAGRAPH_ALIGNMENT_COUNT
  };

  // Auto-sleep timeout options (in minutes)
  enum SLEEP_TIMEOUT {
    SLEEP_1_MIN = 0,
    SLEEP_5_MIN = 1,
    SLEEP_10_MIN = 2,
    SLEEP_15_MIN = 3,
    SLEEP_30_MIN = 4,
    SLEEP_TIMEOUT_COUNT
  };

  // E-ink refresh frequency (pages between full refreshes)
  enum REFRESH_FREQUENCY {
    REFRESH_1 = 0,
    REFRESH_5 = 1,
    REFRESH_10 = 2,
    REFRESH_15 = 3,
    REFRESH_30 = 4,
    REFRESH_NEVER = 5,
    REFRESH_FREQUENCY_COUNT
  };

  enum FILE_BROWSER_DISPLAY {
    FILE_BROWSER_DISPLAY_1_LINE = 0,
    FILE_BROWSER_DISPLAY_2_LINES = 1,
    FILE_BROWSER_DISPLAY_COUNT
  };

  // Short power button press actions
  enum SHORT_PWRBTN {
    IGNORE = 0,
    SLEEP = 1,
    PAGE_TURN = 2,
    FORCE_REFRESH = 3,
    TOGGLE_FONT = 4,
    TOGGLE_GUIDE_DOTS = 5,
    TOGGLE_FOCUS_READING = 6,
    TOGGLE_BOOKMARK = 7,
    SYNC_PROGRESS = 8,
    MARK_FINISHED = 9,
    READING_STATS = 10,
    SCREENSHOT = 11,
    CYCLE_PAGE_TURN = 12,
    FILE_TRANSFER = 13,
    TOGGLE_TILT_PAGE_TURN = 14,
    TOGGLE_DARK_MODE = 15,
    FOOTNOTES = 16,
    FILE_BROWSER = 17,
    CALIBRE_WIRELESS = 18,
    JOIN_NETWORK = 19,
    CREATE_HOTSPOT = 20,
    CREATE_CLIPPING = 21,
    LOOKUP_WORD = 22,
    // Values 23-26 are already persisted by the X4 Pro Home-key feature.
    // Keep Quick Actions separate so existing Home-key mappings retain their meaning.
    TOGGLE_HOME_BUTTON_IN_READER = 26,
    QUICK_ACTIONS = 27,
    TOGGLE_FRONTLIGHT = 28,
    TOGGLE_TOUCHSCREEN = 29,
    // Appended after the X4 Pro and Quick Actions values so existing settings
    // files continue to mean exactly the same thing.
    QUICK_LOCK = 30,
    // Shortcut values are persisted. Append new actions; never reuse removed
    // raw values or they can silently change an existing binding's behavior.
    PREVIOUS_PAGE = 31,
    NEARBY_POSITION_SYNC = 32,
    LIBRARY = 33,
    SLEEP_ONLY = 34,
    WAKE_ONLY = 35,
    HOME_READER = 36,
    PWR_CONFIRM = 37,
    PWR_READER_MENU = 38,
    SHORT_PWRBTN_COUNT
  };

  static constexpr uint8_t SIDE_FLIP_ORIENTATION = SIDE_ROTATE_FLIP;
  static constexpr uint8_t SIDE_OPEN_LIBRARY = LIBRARY;
  static constexpr uint8_t PWR_BOOKMARK = TOGGLE_BOOKMARK, PWR_SYNC = SYNC_PROGRESS, PWR_FRONTLIGHT = TOGGLE_FRONTLIGHT;

  // Power + Up side-button chord actions. Keep this order aligned with
  // ButtonShortcutController::ChordAction because the runtime casts the
  // persisted value to that enum.
  enum POWER_CHORD_ACTION {
    CHORD_SCREENSHOT = 0,
    CHORD_QUICK_LOCK = 1,
    // Values 2 and 3 were removed Next Page and Previous Page actions. Keep
    // them unused so an interim settings file cannot remap them to another action.
    CHORD_DISABLED = 4,
    CHORD_SLEEP = 5,
    CHORD_PAGE_TURN = 6,
    CHORD_TOGGLE_BOOKMARK = 7,
    CHORD_READING_STATS = 8,
    CHORD_MARK_FINISHED = 9,
    CHORD_FORCE_REFRESH = 10,
    CHORD_TOGGLE_FONT = 11,
    CHORD_TOGGLE_GUIDE_DOTS = 12,
    CHORD_TOGGLE_FOCUS_READING = 13,
    CHORD_CYCLE_PAGE_TURN = 14,
    CHORD_SYNC_PROGRESS = 15,
    CHORD_FILE_TRANSFER = 16,
    CHORD_CALIBRE_WIRELESS = 17,
    CHORD_JOIN_NETWORK = 18,
    CHORD_CREATE_HOTSPOT = 19,
    CHORD_TOGGLE_DARK_MODE = 20,
    CHORD_FOOTNOTES = 21,
    CHORD_FILE_BROWSER = 22,
    CHORD_CREATE_CLIPPING = 23,
    CHORD_LOOKUP_WORD = 24,
    CHORD_TOGGLE_HOME_BUTTON = 25,
    CHORD_QUICK_ACTIONS = 26,
    CHORD_TOGGLE_FRONTLIGHT = 27,
    CHORD_TOGGLE_TOUCHSCREEN = 28,
    CHORD_PREVIOUS_PAGE = 29,
    CHORD_NEARBY_POSITION_SYNC = 30,
    POWER_CHORD_ACTION_COUNT
  };

  // Home-key shortcuts reuse power-button actions where possible. Keep the
  // dedicated values stable because they are persisted in settings.bin.
  enum HOME_BUTTON_ACTION {
    HOME_BUTTON_BACK_HOME = 23,
    HOME_BUTTON_TOGGLE_FRONTLIGHT = 24,
    HOME_BUTTON_READER_MENU = 25,
    HOME_BUTTON_ACTION_COUNT = 26
  };

  static constexpr uint8_t QUICK_ACTION_SLOT_ACTION_COUNT = 23;

  // Hide battery percentage
  enum HIDE_BATTERY_PERCENTAGE { HIDE_NEVER = 0, HIDE_READER = 1, HIDE_ALWAYS = 2, HIDE_BATTERY_PERCENTAGE_COUNT };

  // Page turn button long press behavior
  enum LONG_PRESS_BUTTON_BEHAVIOR {
    OFF = 0,
    CHAPTER_SKIP = 1,
    ORIENTATION_CHANGE = 2,
    FONT_SIZE_CHANGE = 3,
    LONG_PRESS_BUTTON_BEHAVIOR_COUNT
  };

  // UI Theme. Raw values are persisted in settings; keep existing values stable.
  enum UI_THEME {
    CLASSIC = 0,
    LYRA = 1,
    LYRA_3_COVERS = 2,
    ROUNDEDRAFF = 3,
    LYRA_CAROUSEL = 4,
    MINIMAL = 5,
    DASHBOARD = 6,
    COVER_GRID = 7,
    UI_THEME_COUNT = 8
  };
  enum RECENT_BOOKS_VIEW { RECENT_BOOKS_LIST = 0, RECENT_BOOKS_GRID = 1, RECENT_BOOKS_VIEW_COUNT };

  // Image rendering in EPUB reader
  enum IMAGE_RENDERING {
    IMAGES_DISPLAY = 0,
    IMAGES_PLACEHOLDER = 1,
    IMAGES_SUPPRESS = 2,
    IMAGES_DISPLAY_BW = 3,
    IMAGE_RENDERING_COUNT
  };
  enum TOUCH_READER_CONTROLS { TOUCH_READER_OFF = 0, TOUCH_READER_ON = 1, TOUCH_READER_CONTROLS_COUNT };
  enum PAGE_TURN_GESTURE {
    TAP_AND_SWIPE = 0,
    TAP_ONLY = 1,
    SWIPE_ONLY = 2,
    INVERTED_TAP = 3,
    PAGE_TURN_GESTURE_DISABLED = 4,
    PAGE_TURN_GESTURE_COUNT
  };

  enum INDEXING_METHOD { INDEXING_INCREMENTAL = 0, INDEXING_FULL_SECTION = 1, INDEXING_METHOD_COUNT };

  enum TILT_PAGE_TURN { TILT_OFF = 0, TILT_ON = 1, TILT_NORMAL = 1, TILT_NVERTED = 2, TILT_PAGE_TURN_COUNT };
  enum TILT_PAGE_TURN_DIRECTION {
    TILT_LEFT_RIGHT = 0,
    TILT_LEFT_RIGHT_INVERTED = 1,
    TILT_FORWARD_BACK = 2,
    TILT_FORWARD_BACK_INVERTED = 3,
    TILT_PAGE_TURN_DIRECTION_COUNT
  };

  // Long-press Confirm (menu button) quick action in reader
  enum LONG_PRESS_MENU_ACTION {
    LONG_MENU_OFF = 0,
    LONG_MENU_SLEEP = 1,
    LONG_MENU_CHANGE_FONT = 2,
    LONG_MENU_TOGGLE_GUIDE_DOTS = 3,
    LONG_MENU_TOGGLE_FOCUS = 4,
    LONG_MENU_TOGGLE_BOOKMARK = 5,
    LONG_MENU_REFRESH_SCREEN = 6,
    LONG_MENU_SYNC_PROGRESS = 7,
    LONG_MENU_MARK_FINISHED = 8,
    LONG_MENU_READING_STATS = 9,
    LONG_MENU_SCREENSHOT = 10,
    LONG_MENU_CYCLE_PAGE_TURN = 11,
    LONG_MENU_FILE_TRANSFER = 12,
    LONG_MENU_TOGGLE_TILT_PAGE_TURN = 13,
    LONG_MENU_TOGGLE_DARK_MODE = 14,
    LONG_MENU_FOOTNOTES = 15,
    LONG_MENU_FILE_BROWSER = 16,
    LONG_MENU_CALIBRE_WIRELESS = 17,
    LONG_MENU_JOIN_NETWORK = 18,
    LONG_MENU_CREATE_HOTSPOT = 19,
    LONG_MENU_CREATE_CLIPPING = 20,
    LONG_MENU_LOOKUP_WORD = 21,
    // Appended: values are persisted in settings.bin.
    LONG_MENU_QUICK_ACTIONS = 22,
    LONG_MENU_QUICK_LOCK = 23,
    LONG_PRESS_MENU_ACTION_COUNT
  };

  // Clipping storage mode
  enum CLIPPING_STORAGE : uint8_t { SINGLE_FILE = 0, PER_BOOK = 1, CLIPPING_STORAGE_COUNT };
  // Clip selector navigation scheme
  enum CLIP_NAV_MODE : uint8_t { LINE_AWARE = 0, WORD_BY_WORD = 1, CLIP_NAV_MODE_COUNT };
  // Annotation underline visibility
  enum ANNOTATION_VISIBILITY : uint8_t { ANNOT_VISIBLE = 0, ANNOT_HIDDEN = 1, ANNOTATION_VISIBILITY_COUNT };

  enum QUICK_RESUME_SLEEP_SCREEN {
    QUICK_RESUME_NEVER = 0,
    QUICK_RESUME_AFTER_TIMEOUT = 1,
    QUICK_RESUME_SLEEP_SCREEN_COUNT
  };

  // UI scale for list-style screens: sizes list fonts and row heights
  // together so touch targets grow uniformly.
  enum UI_SCALE { UI_SCALE_SMALL = 0, UI_SCALE_LARGE = 1, UI_SCALE_COUNT };
  static uint8_t defaultUiScale();

  // Sleep screen settings
  uint8_t sleepScreen = DARK;
  // Night mode: inverted output polarity, applied to every activity per render
  // by ActivityManager. Quick Resume preserves it; other sleep screens remain normal.
  uint8_t screenInverted = 0;
  // Sleep screen cover mode settings
  uint8_t sleepScreenCoverMode = FIT;
  // Sleep screen cover filter
  uint8_t sleepScreenCoverFilter = NO_FILTER;
  // Status bar settings (statusBar retained for migration only)
  uint8_t statusBar = FULL;
  uint8_t statusBarChapterPageCount = 1;
  uint8_t statusBarBookProgressPercentage = 1;
  uint8_t statusBarBookPercentageFormat = BOOK_PERCENTAGE_WHOLE;
  uint8_t stablePageNumbers = 0;
  uint8_t statusBarProgressBar = HIDE_PROGRESS;
  uint8_t statusBarProgressBarThickness = PROGRESS_BAR_NORMAL;
  uint8_t statusBarTitle = CHAPTER_TITLE;
  uint8_t statusBarTimeLeft = TIME_LEFT_HIDE;
  uint8_t statusBarBattery = 1;
  uint8_t xtcStatusBarMode = XTC_STATUS_BAR_HIDE;
  // Clock visibility mode (requires an RTC-backed clock).
  uint8_t hideClock = HIDE_CLOCK_ALWAYS;
  // Clock UTC offset in quarter-hour steps, biased by 48 so it fits in uint8_t.
  // Value 48 = UTC+0, 0 = UTC-12:00, 104 = UTC+14:00.
  // Quarter-hour granularity supports oddball zones like Nepal (+5:45) and Chatham (+12:45).
  uint8_t clockUtcOffsetQ = 48;
  // Clock display format: 0 = 24-hour, 1 = 12-hour
  uint8_t clockFormat = 0;
  // Date display format. Values match HalClock::DateFormat; 0 preserves the existing "Jan 01, 2026" default.
  uint8_t dateFormat = DATE_FORMAT_MONTH_DAY_YEAR_LONG;
  // Separator for numeric dates. Text-based date formats do not use it.
  uint8_t dateSeparator = DATE_SEPARATOR_SLASH;
  // Set once an NTP sync succeeds. Used to skip re-syncing on every WiFi connect.
  // Resetting to 0 (e.g. via the web UI) forces a re-sync on next WiFi connect.
  uint8_t clockHasBeenSynced = 0;
  // Set once an NTP sync writes both date and time. Kept separate so older
  // time-only syncs do not unlock date display with stale RTC date registers.
  uint8_t clockDateHasBeenSynced = 0;
  // Text rendering settings
  uint8_t extraParagraphSpacing = 1;
  uint8_t forceParagraphIndents = 0;
  uint8_t textAntiAliasing = 1;
  // Touch screen reader zones/gestures on boards with a touch controller.
  uint8_t touchReaderControls = TOUCH_READER_ON;
  // Page-turn gestures remain independently configurable while touch reader controls stay enabled.
  uint8_t pageTurnGesture = TAP_AND_SWIPE;
  uint8_t previousPageGesture = TAP_AND_SWIPE;
  uint8_t customBootscreenEnabled = 1;
  uint8_t tapToHideStatusBar = 1;
  // Disables all touchscreen input while a reader is active. Reader menus temporarily override this.
  uint8_t disableReaderTouchscreen = 0;
  // Available only on multi-touch hardware; defaults on for pinch font resizing.
  uint8_t pinchFontResizeEnabled = 1;
  // Two-finger twist rotates the reader screen. Multi-touch hardware only.
  uint8_t twoFingerRotationEnabled = 1;
  // Configurable two-finger swipes. A non-empty action may be assigned to one direction only.
  uint8_t twoFingerSwipeUp = TWO_FINGER_SWIPE_NOT_SET;
  uint8_t twoFingerSwipeDown = TWO_FINGER_SWIPE_NOT_SET;
  uint8_t twoFingerSwipeLeft = TWO_FINGER_SWIPE_NOT_SET;
  uint8_t twoFingerSwipeRight = TWO_FINGER_SWIPE_NOT_SET;
  // Short power button action behaviour
  uint8_t shortPwrBtn = IGNORE;
  // Long power button action behaviour
  uint8_t longPwrBtn = SLEEP;
  // Power + Up shortcut action. Disabled by default so the established
  // Power + Down screenshot chord remains screenshot-only.
  uint8_t powerChordAction = CHORD_DISABLED;
  // Up + Down shortcut action. On touch hardware, while the reader touchscreen
  // is disabled, this chord instead opens Settings as the recovery route.
  uint8_t sideButtonChordAction = CHORD_DISABLED;
  // X4 Pro capacitive Home-key actions. Values below SHORT_PWRBTN_COUNT map
  // directly to the matching power-button shortcut action.
  uint8_t homeButtonTapAction = HOME_BUTTON_BACK_HOME;
  uint8_t homeButtonDoubleTapAction = HOME_BUTTON_TOGGLE_FRONTLIGHT;
  uint8_t homeButtonLongPressAction = HOME_BUTTON_READER_MENU;
  // Home-key devices can lock the capacitive Home key while a reader page is
  // active. Reader menus temporarily override this without changing the value.
  uint8_t homeButtonInReaderEnabled = 1;
  // EPUB reading orientation settings
  // 0 = portrait (default), 1 = landscape clockwise, 2 = inverted, 3 = landscape counter-clockwise
  uint8_t orientation = PORTRAIT;
  // Button layouts (front layout retained for migration only)
  uint8_t frontButtonLayout = BACK_CONFIRM_LEFT_RIGHT;
  uint8_t sideButtonLayout = PREV_NEXT;
  uint8_t frontButtonOrientationAware = FRONT_ORIENTATION_AWARE_OFF;
  uint8_t sideButtonOrientationAware = 0;
  // Action performed when side buttons are long-pressed in reader
  uint8_t sideButtonLongPress = SIDE_LONG_CHAPTER_SKIP;
  // Front button remap (logical -> hardware)
  // Used by MappedInputManager to translate logical buttons into physical front buttons.
  uint8_t frontButtonBack = FRONT_HW_BACK;
  uint8_t frontButtonConfirm = FRONT_HW_CONFIRM;
  uint8_t frontButtonLeft = FRONT_HW_LEFT;
  uint8_t frontButtonRight = FRONT_HW_RIGHT;
  // Reader-specific front button remap (overrides system mapping while in reader activities).
  // readerFrontButtonsEnabled = 0 means the reader uses the system mapping above.
  uint8_t readerFrontButtonsEnabled = 0;
  uint8_t readerFrontButtonBack = FRONT_HW_BACK;
  uint8_t readerFrontButtonConfirm = FRONT_HW_CONFIRM;
  uint8_t readerFrontButtonLeft = FRONT_HW_LEFT;
  uint8_t readerFrontButtonRight = FRONT_HW_RIGHT;
  // Reader font settings
  uint8_t fontFamily = LEXENDDECA;
  // The physical reader size selected by the user. Built-in and SD-card font
  // families resolve this to their closest available file.
  uint8_t readerFontPointSize = 14;
  uint8_t& fontPointSize = readerFontPointSize;  // modern API, one persisted value
  // Transient compatibility state for JSON settings written before fontSize
  // stored a point size. SdCardFontSystem resolves it once the family catalog
  // is available, then persists readerFontPointSize.
  uint8_t legacySdFontSizeStep = UINT8_MAX;
  uint8_t sdFontSizeRange = SD_FONT_RANGE_TINY;
  uint8_t lineSpacing = NORMAL;  // migration only; new saves use lineHeightPercent
  uint8_t lineHeightPercent = 100;
  uint8_t wordSpacing = 0;
  uint8_t paragraphAlignment = JUSTIFIED;
  // Auto-sleep timeout setting (default 10 minutes). Legacy sleepTimeout enum values are migration-only.
  uint8_t sleepTimeoutMinutes = 10;
  // E-ink refresh frequency (default 15 pages)
  uint8_t refreshFrequency = REFRESH_15;
  uint8_t hyphenationEnabled = 0;

  // Reader screen margins. Legacy single-axis settings initialize both values.
  uint8_t screenMarginVertical = 5;
  uint8_t screenMarginHorizontal = 5;
  // Show EPUB publisher pagebreak labels in the reader margin when present.
  uint8_t publisherPageNumbers = 0;
  // OPDS browser settings
  char opdsServerUrl[128] = "";
  char opdsUsername[64] = "";
  char opdsPassword[64] = "";
  // OPDS download destination (empty = SD root). Edited from the OPDS server list.
  char opdsDownloadFolder[64] = "";
  // Nearby file receive destination (empty = SD root).
  char nearbyReceiveFolder[64] = "";
  // Hide battery percentage
  uint8_t hideBatteryPercentage = HIDE_NEVER;
  // Long-press page turn button behavior
  uint8_t longPressButtonBehavior = OFF;
  // UI Theme
  uint8_t uiTheme = LYRA;
  // Recent Books screen layout
  uint8_t recentBooksView = RECENT_BOOKS_LIST;
  // UI scale (list fonts + row heights); touch boards default one step larger
  uint8_t uiScale = defaultUiScale();
  // Sunlight fading compensation
  uint8_t fadingFix = 0;
  // Quick-return from footnotes when a footnote shortcut is active.
  uint8_t pwrBtnFootnoteBack = 1;
  // Use book's embedded CSS styles for EPUB rendering (1 = enabled, 0 = disabled)
  uint8_t embeddedStyle = 1;
  // EPUB section indexing policy. The current chapter keeps its active build.
  uint8_t indexingMethod = INDEXING_FULL_SECTION;
  // Focus Reading - emphasizes the first part of words with bold
  uint8_t focusReadingEnabled = 0;
  // Guide Dots - places a middle dot between words to guide the eye
  uint8_t guideReadingEnabled = 0;
  // Per-book EPUB render mode runtime value. This is intentionally not saved as a global setting.
  uint8_t epubRenderMode = 0;
  // SD card font family name, including optional range suffix (empty = use built-in fontFamily)
  char sdFontFamilyName[64] = "";
  // Global dictionary SD-card font (empty = use the reader font).
  char dictionarySdFontFamilyName[64] = "";
  // Zero follows the active reader size.
  uint8_t dictionaryFontPointSize = 0;
  // Show hidden files/directories (starting with '.') in the file browser (0 = hidden, 1 = show)
  uint8_t showHiddenFiles = 0;
  // Hide file extensions in the file browser right-side value column (0 = show, 1 = hide)
  uint8_t hideFileExtension = 0;
  // File browser display row style (0 = one-line theme list, 1 = two-line compact display)
  uint8_t fileBrowserDisplay = FILE_BROWSER_DISPLAY_1_LINE;
  // Remove a book from the Recent Books list when its End-of-Book screen is reached (0 = off, 1 = on)
  uint8_t removeReadBooksFromRecents = 0;
  // Move epub to /Read/ folder on SD card when marked as finished (0 = disabled, 1 = enabled)
  uint8_t moveFinishedToReadFolder = 0;
  // Automatically write a dated global reading-stats backup before sleep when an RTC is available (0 = off, 1 = on).
  uint8_t autoBackupStats = 1;
  // Idle threshold for reading stats, stored in 10-second units to fit uint8_t.
  uint8_t readingIdleTimeThresholdUnits = 30;
  // Image rendering mode in EPUB reader
  uint8_t imageRendering = IMAGES_DISPLAY;
  // Long-press Confirm (menu button) quick action in reader (0 = off)
  uint8_t longPressMenuAction = LONG_MENU_OFF;
  // Long-press Back quick action in reader (defaults to the historical file browser shortcut)
  uint8_t longPressBackAction = LONG_MENU_FILE_BROWSER;
  // Five reusable reader commands and their single owning shortcut. Keep these
  // adjacent so old settings files simply retain their default-initialized tail.
  uint8_t quickActionSlots[5] = {IGNORE, IGNORE, IGNORE, IGNORE, IGNORE};
  uint8_t quickActionsTrigger = 0;
  // Tilt-based page turning with the Sticky IMU.
  uint8_t tiltPageTurn = TILT_OFF;
  uint8_t tiltPageTurnDirection = TILT_LEFT_RIGHT;
  // Frontlight quick-panel state (boards with FREEINK_CAP_FRONTLIGHT, legacy
  // Pro). Applied at boot, edited only from the frontlight panel; persisted as
  // category-less entries so they stay out of the Settings screen. Writes are
  // debounced by the panel (saved once on exit), not per slider tick.
  uint8_t frontlightBrightness = 60;
  uint8_t frontlightWarmth = 50;  // 0 = cool .. 100 = warm
  uint8_t frontlightOn = 0;
  // When enabled, restore a previously-on light after sleep. A previous off
  // state falls through to a complete schedule.
  uint8_t frontlightRestoreOnWake = 1;
  // Daily wake-only schedule, in local minutes since midnight.
  // An unset endpoint keeps the schedule inactive; its value is retained while
  // the schedule toggle is off so it can be re-enabled without re-entry.
  uint8_t frontlightScheduleEnabled = 0;
  uint16_t frontlightScheduleStart = 0xFFFF;
  uint16_t frontlightScheduleEnd = 0xFFFF;
  // Language setting (Language enum index, default 0 = EN)
  uint8_t language = 0;
  // Enabled keyboard layouts. Zero derives a default from the UI language;
  // non-zero bits follow KeyboardLayoutSet::ALL table order.
  uint16_t keyboardLayouts = 0;
  // Custom KOReader sync device display name. Empty means use the hardware default.
  char deviceName[21] = "";
  // Quick Resume: keep current content visible with moon icon instead of showing a static sleep screen.
  uint8_t quickResumeSleepScreen = QUICK_RESUME_NEVER;
  // Public opt-out; pace/history are not reset by this preference.
  uint8_t trackReadingStats = 1;

  // CrossPoint additions coexist with owner values; never reinterpret owner enum IDs.
  enum STATUS_BAR_CLOCK_MODE {
    STATUS_BAR_CLOCK_HIDE = 0,
    STATUS_BAR_CLOCK_RIGHT = 1,
    STATUS_BAR_CLOCK_LEFT = 2,
    STATUS_BAR_CLOCK_MODE_COUNT
  };
  enum CLOCK_DST_MODE { CLOCK_DST_AUTO = 0, CLOCK_DST_ON = 1, CLOCK_DST_OFF = 2, CLOCK_DST_MODE_COUNT };
  enum LONG_PRESS_MENU_FUNCTION {
    LP_MENU_KOSYNC = 0,
    LP_MENU_DISABLED = 1,
    LP_MENU_BOOKMARK = 2,
    LP_MENU_DICTIONARY = 3,
    LP_MENU_READER_MENU = 4,
    LP_MENU_CREATE_CLIPPING = 5,
    LP_MENU_FOOTNOTES = 6,
    LONG_PRESS_MENU_FUNCTION_COUNT
  };
  enum READER_MENU_STYLE { READER_MENU_LIST = 0, READER_MENU_TOOLBAR = 1, READER_MENU_STYLE_COUNT };
  enum VIBRATION { VIBRATION_TOUCH = 0, VIBRATION_TOUCH_PAGE = 1, VIBRATION_OFF = 2 };
  enum HAPTIC_INTENSITY { HAPTIC_LOW = 0, HAPTIC_MEDIUM = 1, HAPTIC_HIGH = 2 };
  enum SLIDE_SWITCH_ACTION : uint8_t {
    SLIDE_SWITCH_FRONTLIGHT = 0,
    SLIDE_SWITCH_NIGHT_MODE = 1,
    SLIDE_SWITCH_TOUCH_CONTROLS = 2
  };
  enum SHOW_READER_MENU { READER_MENU_OFF = 0, READER_MENU_TAP = 1, READER_MENU_SWIPE_UP = 2, SHOW_READER_MENU_COUNT };
  enum MODERN_LINE_COMPRESSION {
    LINE_SPACING_100 = 0,
    LINE_SPACING_110 = 1,
    LINE_SPACING_120 = 2,
    LINE_SPACING_130 = 3,
    LINE_SPACING_145 = 4,
    LINE_SPACING_160 = 5,
    LINE_SPACING_175 = 6
  };
  static constexpr uint8_t DEFAULT_FONT_POINT_SIZE = 14, LEGACY_FONT_SIZE_MAX = 3;
  static constexpr uint8_t CHARACTER_SPACING_OFFSET = 2, WORD_SPACING_MIN = 50, WORD_SPACING_MAX = 200,
                           WORD_SPACING_STEP = 25;
  static constexpr uint8_t SCREEN_MARGIN_MIN = 5, SCREEN_MARGIN_MAX = 40, SCREEN_MARGIN_STEP = 5;
  uint8_t& screenMargin = screenMarginHorizontal;
  uint8_t characterSpacing = CHARACTER_SPACING_OFFSET;
  int8_t getCharacterSpacing() const { return static_cast<int8_t>(characterSpacing) - CHARACTER_SPACING_OFFSET; }
  uint8_t wordSpacingPercent = 100;
  uint8_t statusBarClock = STATUS_BAR_CLOCK_HIDE, clockTimezone = 255, clockDst = CLOCK_DST_AUTO, clockShowInHeader = 0;
  uint8_t paragraphIndentSpaces = 2, doublePwrBtn = IGNORE, longPressMenuFunction = LP_MENU_DISABLED;
  uint8_t frontButtonFollowOrientation = 0, sideButtonFollowOrientation = 0;
  uint8_t pluginsEnabled = 0, pluginHubPromptHidden = 0, vocabularyHistoryEnabled = 0;
  uint8_t showFileExtensions = 1, libraryUseMetadata = 1, libraryRecentOpenedOnly = 0, backShortToFileBrowser = 0;
  uint8_t opdsFilenameFormat = 0, vibration = VIBRATION_OFF, hapticIntensity = HAPTIC_HIGH;
  uint8_t readerMenuStyle = READER_MENU_LIST, showReaderMenu = READER_MENU_TAP,
          slideSwitchAction = SLIDE_SWITCH_FRONTLIGHT;
  char dictionaryName[32] = {};
  bleturner::Config ble;
  uint8_t sideButtonUpShort = PREVIOUS_PAGE, sideButtonDownShort = PAGE_TURN;
  uint8_t sideButtonUpLong = SIDE_PREVIOUS_CHAPTER, sideButtonDownLong = SIDE_NEXT_CHAPTER;
  uint8_t leftEdgeUp = 0, leftEdgeDown = 0, rightEdgeUp = 0, rightEdgeDown = 0;
  uint8_t librarySortMethod = 4, librarySortDescending = 1, libraryListExpanded = 1;
  uint8_t libraryShowSeries = 1, libraryShowGenre = 1, libraryShowEpub = 1, libraryShowXtc = 1;
  uint8_t libraryShowTxt = 1, libraryShowMarkdown = 1, libraryHideFinishedBooks = 0, swapLibraryFileBrowser = 0;
  bool supportsLibraryFileBrowserSwap() const { return uiTheme == MINIMAL || uiTheme == DASHBOARD; }
  bool isLibraryFileBrowserSwapped() const { return supportsLibraryFileBrowserSwap() && swapLibraryFileBrowser; }
  ReaderStatusBarConfig topReaderStatusBar{},
      bottomReaderStatusBar = migrateBottomStatusBar({true, false, true, 1, 0, true, 0, 2, 1});
  uint8_t legacyXtcTopUsesBottom = 0;
  DisplayStatusBarConfig displayStatusBar;
  static bool hasPowerDoubleClick() { return true; }
  static uint8_t defaultDoublePwrBtn() { return IGNORE; }
  static bool fadingFixSupported() { return false; }  // Sticky product
  void clearSdFontFamily() {
    sdFontFamilyName[0] = '\0';
    saveToFile();
  }

  // Called after legacy owner parsing. Manually persisted modern fields are not
  // coupled to a board-dependent SettingsList option count.
  void writeCompatibilitySettings(JsonDocument& doc) const;
  bool readCompatibilitySettings(JsonVariantConst doc, bool importingCrossPoint);

  ~CrossPointSettings() = default;

  static constexpr uint16_t POWER_BUTTON_WAKE_SHORT_MS = 10;
  static constexpr uint16_t POWER_BUTTON_WAKE_LONG_MS = 200;
  static constexpr uint16_t POWER_BUTTON_LONG_PRESS_MS = 400;
  static constexpr uint8_t MIN_SLEEP_TIMEOUT_MINUTES = 1;
  static constexpr uint8_t SLEEP_TIMEOUT_NEVER_MINUTES = 31;
  static constexpr uint8_t MAX_SLEEP_TIMEOUT_MINUTES = SLEEP_TIMEOUT_NEVER_MINUTES;
  static constexpr uint8_t SD_FONT_MAX_SIZE_STEPS = 8;
  static constexpr uint8_t MIN_READER_FONT_POINT_SIZE = 8;
  static constexpr uint8_t MIN_LINE_HEIGHT_PERCENT = 70;
  static constexpr uint8_t MAX_LINE_HEIGHT_PERCENT = 200;
  static constexpr uint8_t LINE_HEIGHT_PERCENT_STEP = 1;
  static constexpr uint8_t MIN_SCREEN_MARGIN = 5;
  static constexpr uint8_t MAX_SCREEN_MARGIN = 150;
  static constexpr uint8_t SCREEN_MARGIN_SMALL_STEP = 1;
  static constexpr uint8_t SCREEN_MARGIN_LARGE_STEP = 5;
  static constexpr uint8_t MAX_WORD_SPACING = 4;
  static constexpr uint16_t DEFAULT_READING_IDLE_TIME_THRESHOLD_SECONDS = 5 * 60;
  static constexpr uint16_t MIN_READING_IDLE_TIME_THRESHOLD_SECONDS = 30;
  static constexpr uint16_t MAX_READING_IDLE_TIME_THRESHOLD_SECONDS = 10 * 60;
  static constexpr uint8_t READING_IDLE_TIME_THRESHOLD_UNIT_SECONDS = 10;
  static constexpr uint8_t MIN_READING_IDLE_TIME_THRESHOLD_UNITS =
      MIN_READING_IDLE_TIME_THRESHOLD_SECONDS / READING_IDLE_TIME_THRESHOLD_UNIT_SECONDS;
  static constexpr uint8_t MAX_READING_IDLE_TIME_THRESHOLD_UNITS =
      MAX_READING_IDLE_TIME_THRESHOLD_SECONDS / READING_IDLE_TIME_THRESHOLD_UNIT_SECONDS;
  static constexpr size_t MIN_DEVICE_NAME_LENGTH = 2;
  static constexpr size_t MAX_DEVICE_NAME_LENGTH = sizeof(deviceName) - 1;

  uint16_t getPowerButtonWakeDuration() const {
    return (shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) ? POWER_BUTTON_WAKE_SHORT_MS
                                                                    : POWER_BUTTON_WAKE_LONG_MS;
  }

  bool shouldShowClockInReader() const { return hideClock == HIDE_CLOCK_NEVER; }
  bool shouldShowClockOutsideReader() const {
    return hideClock == HIDE_CLOCK_NEVER || hideClock == HIDE_CLOCK_IN_READER;
  }
  bool shouldTrackReadingStats() const { return trackReadingStats != 0; }
  static const char* getDefaultDeviceName();
  const char* getEffectiveDeviceName() const;
  uint16_t getReadingIdleTimeThresholdSeconds() const;

  // Callback to resolve SD card font IDs. Set by SdCardFontSystem::begin().
  // Returns font ID or 0 if not found.
  using SdFontIdResolver = int (*)(void* ctx, const char* familyName, uint8_t pointSize);
  SdFontIdResolver sdFontIdResolver = nullptr;
  void* sdFontResolverCtx = nullptr;

  uint16_t getPowerButtonDuration() const { return getPowerButtonWakeDuration(); }
  uint16_t getPowerButtonLongPressDuration() const { return POWER_BUTTON_LONG_PRESS_MS; }
  static uint8_t getActiveReaderFontSizeCount();
  static uint8_t getStoredReaderFontSize(FONT_SIZE size);
  static uint8_t getReaderFontPointSize(FONT_SIZE size);
  static uint8_t getSdFontRangePointSize(uint8_t range, uint8_t step);
  static bool isSdFontPointSizeAllowedForRange(uint8_t pointSize, uint8_t range);
  FONT_SIZE getEffectiveReaderFontSize() const;
  uint8_t getSdFontTargetPointSize() const;
  bool changeReaderFontSize(bool larger, FontSizeStepMode mode = FontSizeStepMode::Wrap);
  int getReaderFontId() const;
  int getBuiltInReaderFontId() const;

  // If count_only is true, returns the number of settings items that would be written.
  uint8_t writeSettings(HalFile& file, bool count_only = false) const;

  bool saveToFile() const;
  bool loadFromFile();
  static const char* getFilePath() { return "/.crosspoint/crosssmudge-settings.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc, bool importingCrossPoint = false);

  struct StatusBarSpec {
    bool showChapterPageCount = false;
    bool showBookProgressPercent = false;
    bool showStablePageNumbers = false;
    uint8_t titleMode = HIDE_TITLE;
    uint8_t timeLeftMode = TIME_LEFT_HIDE;
    bool showBattery = false;
    bool showBatteryPercent = false;
    bool showClock = false;
    uint8_t clockMode = 0;
    bool clock12h = false;
    bool showsClock() const { return showClock || clockMode != 0; }
    uint8_t progressBarMode = HIDE_PROGRESS;
    uint8_t progressBarHeightPx = 0;
    uint8_t xtcMode = XTC_STATUS_BAR_HIDE;

    bool textLaneVisible(bool clockAvailable) const {
      return showChapterPageCount || showBookProgressPercent || showStablePageNumbers || titleMode != HIDE_TITLE ||
             timeLeftMode != TIME_LEFT_HIDE || showBattery || (showClock && clockAvailable);
    }
    bool showsProgressBar() const { return progressBarMode != HIDE_PROGRESS; }
    bool showsTitle() const { return titleMode != HIDE_TITLE; }
  };

  StatusBarSpec statusBarSpec() const;
  ReaderRenderSpec readerRenderSpec(uint16_t viewportWidth, uint16_t viewportHeight,
                                    EpubRenderMode renderMode = EpubRenderMode::CrossSmudgeDefault) const;

  static void validateFrontButtonMapping(CrossPointSettings& settings);
  static void validateReaderFrontButtonMapping(CrossPointSettings& settings);
  static bool isTwoFingerSwipeActionAvailable(uint8_t action, bool frontlightPresent, bool hasColorTemperature);
  static bool normalizeTwoFingerSwipeActions(CrossPointSettings& settings,
                                             uint8_t CrossPointSettings::* editedField = nullptr);
  static uint8_t sleepTimeoutEnumToMinutes(uint8_t legacyValue);
  static uint8_t sleepScreenStorageToMode(uint8_t storedValue);
  static uint8_t sleepScreenModeToStorage(uint8_t mode);
  static uint8_t legacyLineSpacingToPercent(uint8_t legacyValue, uint8_t fontFamily, bool sdFontSelected);
  static uint8_t clampedLineHeightPercent(uint8_t value);
  static uint8_t readingIdleTimeThresholdUnitsForSeconds(uint16_t seconds);
  static uint16_t readingIdleTimeThresholdSecondsForUnits(uint8_t units);
#ifdef SIMULATOR
  static bool verifySleepTimeoutMigrationContract();
  static bool verifySleepScreenMigrationContract();
#endif

 private:
  bool loadFromBinaryFile();
  bool migrateLanguageBinaryFile();

 public:
  float getReaderLineCompression() const;
  unsigned long getSleepTimeoutMs() const;
  int getRefreshFrequency() const;
};

inline void CrossPointSettings::writeCompatibilitySettings(JsonDocument& doc) const {
  doc["clockTimezone"] = clockTimezone;
  doc["clockDst"] = clockDst;
  doc["clockShowInHeader"] = clockShowInHeader;
  doc["statusBarClock"] = statusBarClock;
  doc["paragraphIndentSpaces"] = paragraphIndentSpaces;
  doc["doublePwrBtn"] = doublePwrBtn;
  doc["longPressMenuFunction"] = longPressMenuFunction;
  doc["frontButtonFollowOrientation"] = frontButtonFollowOrientation;
  doc["sideButtonFollowOrientation"] = sideButtonFollowOrientation;
  doc["pluginsEnabled"] = pluginsEnabled;
  doc["pluginHubPromptHidden"] = pluginHubPromptHidden;
  doc["vocabularyHistoryEnabled"] = vocabularyHistoryEnabled;
  doc["showFileExtensions"] = showFileExtensions;
  doc["libraryUseMetadata"] = libraryUseMetadata;
  doc["libraryRecentOpenedOnly"] = libraryRecentOpenedOnly;
  doc["backShortToFileBrowser"] = backShortToFileBrowser;
  doc["opdsFilenameFormat"] = opdsFilenameFormat;
  doc["vibration"] = vibration;
  doc["hapticIntensity"] = hapticIntensity;
  doc["readerMenuStyle"] = readerMenuStyle;
  doc["showReaderMenu"] = showReaderMenu;
  doc["slideSwitchAction"] = slideSwitchAction;
  doc["trackReadingStats"] = trackReadingStats;
  doc["sideButtonUpShort"] = sideButtonUpShort;
  doc["sideButtonDownShort"] = sideButtonDownShort;
  doc["sideButtonUpLong"] = sideButtonUpLong;
  doc["sideButtonDownLong"] = sideButtonDownLong;
  doc["leftEdgeUp"] = leftEdgeUp;
  doc["leftEdgeDown"] = leftEdgeDown;
  doc["rightEdgeUp"] = rightEdgeUp;
  doc["rightEdgeDown"] = rightEdgeDown;
  doc["librarySortMethod"] = librarySortMethod;
  doc["librarySortDescending"] = librarySortDescending;
  doc["libraryListExpanded"] = libraryListExpanded;
  doc["libraryShowSeries"] = libraryShowSeries;
  doc["libraryShowGenre"] = libraryShowGenre;
  doc["libraryShowEpub"] = libraryShowEpub;
  doc["libraryShowXtc"] = libraryShowXtc;
  doc["libraryShowTxt"] = libraryShowTxt;
  doc["libraryShowMarkdown"] = libraryShowMarkdown;
  doc["libraryHideFinishedBooks"] = libraryHideFinishedBooks;
  doc["swapLibraryFileBrowser"] = swapLibraryFileBrowser;
  doc["characterSpacing"] = characterSpacing;
  doc["wordSpacingPercent"] = wordSpacingPercent;
  doc["dictionaryName"] = dictionaryName;
  doc["settingsCompatibilityVersion"] = 1;
  auto bars = doc["readerStatusBars"].to<JsonObject>();
  bars["version"] = 1;
  writeReaderStatusBarJson(bars["top"].to<JsonObject>(), topReaderStatusBar);
  writeReaderStatusBarJson(bars["bottom"].to<JsonObject>(), bottomReaderStatusBar);
  bars["xtcMode"] = xtcStatusBarMode;
  bars["legacyXtcTopUsesBottom"] = legacyXtcTopUsesBottom != 0;
  bleturner::writeJson(ble, doc);
}

inline bool CrossPointSettings::readCompatibilitySettings(JsonVariantConst doc, bool importingCrossPoint) {
  bool changed = false;
  const auto load = [&](const char* key, uint8_t& field, int count) {
    if (doc[key].isNull()) return;
    if (doc[key].is<int>() && doc[key].as<int>() >= 0 && doc[key].as<int>() < count)
      field = doc[key].as<int>();
    else
      changed = true;
  };
  if (doc["clockTimezone"].is<uint8_t>()) clockTimezone = doc["clockTimezone"].as<uint8_t>();
  load("clockDst", clockDst, 3);
  load("clockShowInHeader", clockShowInHeader, 2);
  load("statusBarClock", statusBarClock, 3);
  load("paragraphIndentSpaces", paragraphIndentSpaces, 6);
  load("doublePwrBtn", doublePwrBtn, SHORT_PWRBTN_COUNT);
  load("longPressMenuFunction", longPressMenuFunction, 7);
  load("frontButtonFollowOrientation", frontButtonFollowOrientation, 2);
  load("sideButtonFollowOrientation", sideButtonFollowOrientation, 2);
  load("pluginsEnabled", pluginsEnabled, 2);
  load("pluginHubPromptHidden", pluginHubPromptHidden, 2);
  load("vocabularyHistoryEnabled", vocabularyHistoryEnabled, 2);
  load("showFileExtensions", showFileExtensions, 2);
  load("libraryUseMetadata", libraryUseMetadata, 2);
  load("libraryRecentOpenedOnly", libraryRecentOpenedOnly, 2);
  load("backShortToFileBrowser", backShortToFileBrowser, 2);
  load("opdsFilenameFormat", opdsFilenameFormat, 3);
  load("vibration", vibration, 3);
  load("hapticIntensity", hapticIntensity, 3);
  load("readerMenuStyle", readerMenuStyle, 2);
  load("showReaderMenu", showReaderMenu, 3);
  load("slideSwitchAction", slideSwitchAction, 3);
  load("trackReadingStats", trackReadingStats, 2);
  if (!doc["sideButtonUpShort"].isNull()) {
    const int action = doc["sideButtonUpShort"] | -1;
    if (doc["sideButtonUpShort"].is<int>() && ((action >= 0 && action < SHORT_PWRBTN_COUNT) ||
                                               (action >= SIDE_PREVIOUS_CHAPTER && action <= SIDE_ROTATE_FLIP)))
      sideButtonUpShort = action;
    else
      changed = true;
  }
  if (!doc["sideButtonDownShort"].isNull()) {
    const int action = doc["sideButtonDownShort"] | -1;
    if (doc["sideButtonDownShort"].is<int>() && ((action >= 0 && action < SHORT_PWRBTN_COUNT) ||
                                                 (action >= SIDE_PREVIOUS_CHAPTER && action <= SIDE_ROTATE_FLIP)))
      sideButtonDownShort = action;
    else
      changed = true;
  }
  if (!doc["sideButtonUpLong"].isNull()) {
    const int action = doc["sideButtonUpLong"] | -1;
    if (doc["sideButtonUpLong"].is<int>() && ((action >= 0 && action < SHORT_PWRBTN_COUNT) ||
                                              (action >= SIDE_PREVIOUS_CHAPTER && action <= SIDE_ROTATE_FLIP)))
      sideButtonUpLong = action;
    else
      changed = true;
  }
  if (!doc["sideButtonDownLong"].isNull()) {
    const int action = doc["sideButtonDownLong"] | -1;
    if (doc["sideButtonDownLong"].is<int>() && ((action >= 0 && action < SHORT_PWRBTN_COUNT) ||
                                                (action >= SIDE_PREVIOUS_CHAPTER && action <= SIDE_ROTATE_FLIP)))
      sideButtonDownLong = action;
    else
      changed = true;
  }
  load("leftEdgeUp", leftEdgeUp, 9);
  load("leftEdgeDown", leftEdgeDown, 9);
  load("rightEdgeUp", rightEdgeUp, 9);
  load("rightEdgeDown", rightEdgeDown, 9);
  load("librarySortMethod", librarySortMethod, 7);
  load("librarySortDescending", librarySortDescending, 2);
  load("libraryListExpanded", libraryListExpanded, 2);
  load("libraryShowSeries", libraryShowSeries, 2);
  load("libraryShowGenre", libraryShowGenre, 2);
  load("libraryShowEpub", libraryShowEpub, 2);
  load("libraryShowXtc", libraryShowXtc, 2);
  load("libraryShowTxt", libraryShowTxt, 2);
  load("libraryShowMarkdown", libraryShowMarkdown, 2);
  load("libraryHideFinishedBooks", libraryHideFinishedBooks, 2);
  load("swapLibraryFileBrowser", swapLibraryFileBrowser, 2);
  load("characterSpacing", characterSpacing, 5);
  if (doc["wordSpacingPercent"].is<int>()) {
    const int v = doc["wordSpacingPercent"].as<int>();
    if (v >= 50 && v <= 200)
      wordSpacingPercent = v;
    else
      changed = true;
  }
  if (doc["dictionaryName"].is<const char*>()) {
    const char* value = doc["dictionaryName"].as<const char*>();
    const size_t length = std::min(std::strlen(value), sizeof(dictionaryName) - 1);
    std::memmove(dictionaryName, value, length);
    dictionaryName[length] = '\0';
  }
  // The generic fallback may itself contain an older owner document. Owner
  // keys identify its enum domain even when it lives at the generic filename.
  const bool upstream = importingCrossPoint && doc["lineHeightPercent"].isNull() && doc["longPwrBtn"].isNull() &&
                        doc["forceParagraphIndents"].isNull();
  if (upstream) {
    const uint8_t font = doc["fontFamily"] | uint8_t(1);
    fontFamily = NOTOSANS;
    if (sdFontFamilyName[0] == '\0' && font != 1) {
      const char* family = font == 0 ? "NotoSerif" : font == 2 ? "OpenDyslexic" : "";
      std::strncpy(sdFontFamilyName, family, sizeof(sdFontFamilyName) - 1);
    }
    const int size = doc["fontSize"] | 14;
    readerFontPointSize = size >= 0 && size <= 3 ? 12 + size * 2 : size >= 8 && size <= 255 ? size : 14;
    const int sleep = doc["sleepScreen"] | 0;
    constexpr uint8_t sleeps[] = {
        DARK, LIGHT, CUSTOM, COVER, COVER_CUSTOM, BLANK, QUICK_RESUME, TRANSPARENT_CUSTOM, COVER_TRANSPARENT};
    if (sleep >= 0 && sleep < 9) sleepScreen = sleeps[sleep];
    if (doc["uiTheme"] == 4) uiTheme = COVER_GRID;
    const int power = doc["shortPwrBtn"] | 0;
    constexpr uint8_t actions[] = {IGNORE,        SLEEP,           PAGE_TURN,        FORCE_REFRESH,
                                   FOOTNOTES,     PWR_CONFIRM,     CREATE_CLIPPING,  TOGGLE_BOOKMARK,
                                   SYNC_PROGRESS, PWR_READER_MENU, TOGGLE_FRONTLIGHT};
    shortPwrBtn = power >= 0 && power < 11 ? actions[power] : uint8_t(IGNORE);
    const int dbl = doc["doublePwrBtn"] | 0;
    doublePwrBtn = dbl >= 0 && dbl < 11 ? actions[dbl] : uint8_t(IGNORE);
    constexpr uint8_t heights[] = {100, 110, 120, 130, 145, 160, 175};
    const int height = doc["lineHeight"] | -1;
    if (height >= 0 && height < 7)
      lineHeightPercent = heights[height];
    else if (doc["lineSpacing"].is<int>()) {
      const int old = doc["lineSpacing"].as<int>();
      if (old >= 0 && old < 4) lineHeightPercent = heights[old + 1];
    }
    const int spacing = doc["wordSpacing"] | 100;
    if (spacing >= 50 && spacing <= 200) wordSpacingPercent = spacing;
    wordSpacing = 0;
    changed = true;
  }
  if (!upstream) {
    // These appended raw choices are valid even before a board-specific menu
    // learns to display them. Never let that menu reset a durable choice.
    const int power = doc["shortPwrBtn"] | -1;
    if (power >= LIBRARY && power < SHORT_PWRBTN_COUNT) shortPwrBtn = power;
    if (doc["imageRendering"] == static_cast<int>(IMAGES_DISPLAY_BW)) imageRendering = IMAGES_DISPLAY_BW;
  }
  if (doc["showFileExtensions"].isNull())
    showFileExtensions = !hideFileExtension;
  else
    hideFileExtension = !showFileExtensions;
  if (doc["frontButtonFollowOrientation"].isNull()) frontButtonFollowOrientation = frontButtonOrientationAware != 0;
  if (doc["sideButtonFollowOrientation"].isNull()) sideButtonFollowOrientation = sideButtonOrientationAware;
  if (doc["statusBarClock"].isNull())
    statusBarClock = hideClock == HIDE_CLOCK_NEVER ? STATUS_BAR_CLOCK_RIGHT : STATUS_BAR_CLOCK_HIDE;
  if (doc["paragraphIndentSpaces"].isNull() && !upstream) paragraphIndentSpaces = forceParagraphIndents ? 2 : 0;
  const uint8_t up = sideButtonLayout == NEXT_PREV || sideButtonLayout == NEXT_NEXT ? PAGE_TURN
                     : sideButtonLayout == SIDE_BUTTONS_DISABLED                    ? IGNORE
                                                                                    : PREVIOUS_PAGE;
  const uint8_t down = sideButtonLayout == NEXT_PREV || sideButtonLayout == PREV_PREV ? PREVIOUS_PAGE
                       : sideButtonLayout == SIDE_BUTTONS_DISABLED                    ? IGNORE
                                                                                      : PAGE_TURN;
  if (doc["sideButtonUpShort"].isNull()) sideButtonUpShort = up;
  if (doc["sideButtonDownShort"].isNull()) sideButtonDownShort = down;
  const auto hold = [&](bool isUp) -> uint8_t {
    if (sideButtonLayout == SIDE_BUTTONS_DISABLED) return IGNORE;
    switch (sideButtonLongPress) {
      case SIDE_LONG_CHAPTER_SKIP:
        return isUp ? SIDE_PREVIOUS_CHAPTER : SIDE_NEXT_CHAPTER;
      case SIDE_LONG_FONT_SIZE:
        return isUp ? SIDE_INCREASE_FONT : SIDE_DECREASE_FONT;
      case SIDE_LONG_ORIENTATION_CHANGE:
        return isUp ? SIDE_ROTATE_COUNTERCLOCKWISE : SIDE_ROTATE_CLOCKWISE;
      default:
        return IGNORE;
    }
  };
  if (doc["sideButtonUpLong"].isNull()) sideButtonUpLong = hold(true);
  if (doc["sideButtonDownLong"].isNull()) sideButtonDownLong = hold(false);
  const auto bars = doc["readerStatusBars"];
  if (bars["version"] != 1) {
    bottomReaderStatusBar = migrateBottomStatusBar(
        {statusBarChapterPageCount != 0, stablePageNumbers != 0, statusBarBookProgressPercentage != 0, statusBarTitle,
         statusBarTimeLeft, statusBarBattery != 0, statusBarBookPercentageFormat, statusBarProgressBar,
         statusBarProgressBarThickness});
    topReaderStatusBar = {};
    if (hideClock == HIDE_CLOCK_NEVER)
      topReaderStatusBar.slots[ReaderStatusBarConfig::CENTER] = ReaderStatusBarItem::Clock;
    legacyXtcTopUsesBottom = xtcStatusBarMode == XTC_STATUS_BAR_TOP;
    changed = true;
  } else {
    changed |= repairReaderStatusBarJson(bars["top"], topReaderStatusBar, true, BOOK_PERCENTAGE_FORMAT_COUNT,
                                         STATUS_BAR_PROGRESS_BAR_COUNT, STATUS_BAR_PROGRESS_BAR_THICKNESS_COUNT);
    changed |= repairReaderStatusBarJson(bars["bottom"], bottomReaderStatusBar, true, BOOK_PERCENTAGE_FORMAT_COUNT,
                                         STATUS_BAR_PROGRESS_BAR_COUNT, STATUS_BAR_PROGRESS_BAR_THICKNESS_COUNT);
    legacyXtcTopUsesBottom = bars["legacyXtcTopUsesBottom"] | false;
    const int xtc = bars["xtcMode"] | -1;
    if (xtc >= 0 && xtc < XTC_STATUS_BAR_MODE_COUNT)
      xtcStatusBarMode = xtc;
    else
      changed = true;
  }
  changed |= !bleturner::readJson(ble, doc);
  return changed;
}

// Helper macro to access settings
#define SETTINGS CrossPointSettings::getInstance()
