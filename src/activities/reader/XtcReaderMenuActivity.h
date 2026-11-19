#pragma once

#include <I18n.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "CrossPointSettings.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

// Shared reader chrome lives here while the migration's production-file boundary
// is frozen. Paint, viewport reservation and taps use the same oriented band.
namespace ReaderChrome {
struct Values {
  std::string bookTitle, chapterTitle, timeBook, timeChapter, notice;
  float bookPercent = 0, chapterPercent = 0;
  int page = 0, pages = 0, stablePage = 0, stablePages = 0;
  bool estimated = false, bookmarked = false;
};
const ReaderStatusBarConfig& config(ReaderStatusBarPosition position, bool xtc = false);
int height(const GfxRenderer& renderer, ReaderStatusBarPosition position, bool xtc = false, bool forceText = false);
Rect band(const GfxRenderer& renderer, ReaderStatusBarPosition position, bool xtc = false, bool forceText = false);
bool tapped(const GfxRenderer& renderer, int y, bool xtc = false, bool forceFooter = false);
void draw(const GfxRenderer& renderer, ReaderStatusBarPosition position, const Values& values,
          bool visible = true, bool xtc = false);
}  // namespace ReaderChrome

class XtcReaderMenuActivity final : public Activity {
 public:
  enum class MenuAction {
    SELECT_CHAPTER,
    READING_STATS,
    TOGGLE_COMPLETED,
    DELETE_STATS,
    DELETE_CACHE,
    SEND_NEARBY_BOOK,
    DISABLE_TOUCHSCREEN,
    TOGGLE_STATS,
  };

  XtcReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title, bool hasChapters,
                        bool isBookCompleted, bool statsEnabled = true);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
  bool allowGlobalHomeGesture() const override { return false; }

 private:
  using UiHost = UiAppHost;
  using UiApp = UiHost::App;
  static constexpr size_t kMaxMenuItems = 8;

  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  static std::vector<MenuItem> buildMenuItems(bool hasChapters, bool isBookCompleted, bool hasTouch);
  void finishCancelled();

  ButtonNavigator buttonNavigator;
  std::string title;
  std::vector<MenuItem> items;
  std::array<freeink::ui::ListItem, kMaxMenuItems> listItems{};
  int selectedIndex = 0;
  bool statsEnabled = true;
  UiHost ui;
  int visibleRows = 1;
  int topIndex = 0;
  int listHeaderHeight = 0;

  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
  void refreshListItems();
  void selectCurrent();
};
