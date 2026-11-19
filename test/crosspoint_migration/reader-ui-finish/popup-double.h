#include <functional>
struct CrossPointSettings {
  enum class SHORT_PWRBTN { QUICK_ACTIONS, LOOKUP_WORD, SYNC_PROGRESS, OTHER };
  static constexpr int LONG_MENU_LOOKUP_WORD = 1;
};
struct EpubReaderMenuActivity {
  enum class MenuAction { SYNC };
};
struct Popup {
  ReaderActivity* owner = nullptr;
  bool active = false;
  std::function<void()> cancel;
  std::function<void(CrossPointSettings::SHORT_PWRBTN)> select;
  bool isActive() const { return active; }
  void setCancelCallback(std::function<void()> f) { cancel = std::move(f); }
  void close(bool choose) {
    active = false;
    owner->overlay = false;
    if (choose)
      select(CrossPointSettings::SHORT_PWRBTN::OTHER);
    else
      cancel();
  }
};
namespace QuickActions {
template <class Update, class Select, class... Filter>
void showConfiguredPopup(Popup& p, Update, Select select, Filter...) {
  p.active = true;
  p.owner->overlay = true;
  p.select = select;
}
}  // namespace QuickActions
struct Mapped {
  bool overrideTouch = false;
  void setReaderTouchscreenOverride(bool b) { overrideTouch = b; }
};
ActivityManager activityManager;
struct PopupReader : ReaderActivity {
  Popup quickActionsPopup;
  Mapped mappedInput;
  int dispatched = 0;
  PopupReader() { quickActionsPopup.owner = this; }
  void requestUpdate() {}
  void dispatchShortcutAction(CrossPointSettings::SHORT_PWRBTN) { ++dispatched; }
  bool executeReaderShortcutAction(CrossPointSettings::SHORT_PWRBTN) { return true; }
  static bool supportsQuickAction(CrossPointSettings::SHORT_PWRBTN) { return true; }
};
struct EpubReaderActivity : PopupReader {
  void clearPendingManualPageTurns() {}
  void executeReaderQuickAction(int, bool) { ++dispatched; }
  void onReaderMenuConfirm(EpubReaderMenuActivity::MenuAction) { ++dispatched; }
  void openQuickActionsPopup();
};
struct XtcReaderActivity : PopupReader {
  bool handleShortcutAction(CrossPointSettings::SHORT_PWRBTN);
};
struct TxtReaderActivity : PopupReader {
  bool handleShortcutAction(CrossPointSettings::SHORT_PWRBTN);
};
