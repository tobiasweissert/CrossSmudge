#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
struct Date {
  int day = 0;
  bool isValid() const { return day > 0; }
};
struct ReadingStatsDateTime {
  Date date;
};
bool getCurrentLocalReadingStatsDateTime(ReadingStatsDateTime& n) {
  n.date.day = 7;
  return true;
}
struct Stats {
  static inline bool ack = true;
  static inline int writes = 0;
  bool isCompleted = false, finishedDateManual = false;
  Date finishedDate;
  int candidate = 0;
  bool save(const std::string&) {
    ++writes;
    return ack;
  }
};
struct Global {
  static inline bool ack = true;
  static inline int writes = 0;
  int completedBooks = 0;
  bool save() {
    ++writes;
    return ack;
  }
};
struct Rect {
  int x, y, width, height;
};
struct Renderer {
  int displayed = 0;
  void clearScreen() {}
  int getScreenWidth() { return 480; }
  int getScreenHeight() { return 800; }
  void displayBuffer() { ++displayed; }
};
constexpr int UI_10_FONT_ID = 1;
int messages = 0;
struct UITheme {
  static void drawCenteredWrappedText(Renderer&, Rect, int, const char*, int) { ++messages; }
};
void delay(int) {}
struct ReadingStatsResult {
  bool changed;
};
struct BookStatsActivity {
  enum class Page { Heatmap, PerBook, EditDates };
  Page page = Page::Heatmap;
  std::string bookCachePath = "book";
  Stats stats, dateEditStatsSnapshot;
  Global globalStats, dateEditGlobalStatsSnapshot;
  bool didChangeStats = false, didChangeStatsBeforeDateEdit = false, dateEditSnapshotValid = false,
       returnToHomeOnExit = false;
  bool editable = true, finished = false;
  int refreshed = 0, updates = 0;
  Renderer renderer;
  bool hasEditableBook() const { return editable; }
  void refreshAllDevicesStats() { ++refreshed; }
  void requestUpdate() { ++updates; }
  void setResult(ReadingStatsResult) {}
  void onGoHome() { finished = true; }
  void finish() { finished = true; }
  void saveStats();
  void beginDateEditing();
  void finishDateEditing(bool);
  void exitStatsActivity();
  void applyCompletedState(bool);
};
