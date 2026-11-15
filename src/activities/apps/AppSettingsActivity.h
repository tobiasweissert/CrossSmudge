#pragma once

#include <algorithm>
#include <cstdio>
#include <string>

#include "SmudgeSettings.h"
#include <HalLuaHttp.h>
#include <ctime>
#include "Memory.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/apps/lua/LuaConsentActivity.h"
#include "activities/Activity.h"
#include "components/UITheme.h"

class AppSettingsActivity : public Activity {
 public:
  AppSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AppSettings", renderer, mappedInput) {}

  ~AppSettingsActivity() override { SmudgeSettings::getInstance().save(); }

  void onEnter() override {
    Activity::onEnter();
    auto& settings = SmudgeSettings::getInstance();
    cursorIndex = (settings.sortMode == MenuSortMode::Alphabetical) ? 0 : 1;
    requestUpdate();
  }

  void loop() override {
    auto& settings = SmudgeSettings::getInstance();
    constexpr int totalItems = 4;

    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      settings.save();
      finish();
      return;
    }

    int touchedIndex = -1;
    if (mappedInput.wasItemTapped(touchedIndex) && touchedIndex >= 0 && touchedIndex < totalItems) {
      cursorIndex = touchedIndex;
      applySelection();
      return;
    }

    if (mappedInput.wasReleased(MappedInputManager::Button::Right) ||
        mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      cursorIndex = (cursorIndex + 1) % totalItems;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Left) ||
               mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      cursorIndex = (cursorIndex - 1 + totalItems) % totalItems;
      requestUpdate();
    } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      applySelection();
    }
  }

  void render(RenderLock&& lock) override {
    renderer.clearScreen();

    const int pageWidth = renderer.getScreenWidth();
    const int pageHeight = renderer.getScreenHeight();
    const auto& metrics = UITheme::getInstance().getMetrics();

    const int headerY = metrics.topPadding;
    const int headerH = metrics.headerHeight;
    const int startY = headerY + headerH + metrics.verticalSpacing;
    const int menuHeight = pageHeight - startY - metrics.buttonHintsHeight - metrics.verticalSpacing;

    auto& settings = SmudgeSettings::getInstance();
    constexpr int totalItems = 4;

    // 1. Draw Header
    GUI.drawHeader(renderer, Rect{0, headerY, pageWidth, headerH}, "App Settings");

    // 2. Draw Menu with Sort Options
    GUI.drawButtonMenu(
        renderer, Rect{0, startY, pageWidth, menuHeight}, totalItems, cursorIndex,
        [&settings](int index) -> const char* {
          if (index == 0) {
            return (settings.sortMode == MenuSortMode::Alphabetical) ? "Alphabetical (A - Z)  [Active]"
                                                                     : "Alphabetical (A - Z)";
          } else if (index == 1) {
            return (settings.sortMode == MenuSortMode::MostUsed) ? "Most Frequently Used  [Active]"
                                                                 : "Most Frequently Used";
          }
          if (index == 2) return tr(STR_LUA_NETWORK_REVOKE);
          if (index == 3) return tr(STR_LUA_TRUST_TIME);
          return "";
        },
        [](int index) { return (index == 0) ? UIIcon::Applications : UIIcon::Recent; });

    // 3. Draw Footer Hints
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

    if (operationResult_) GUI.drawPopup(renderer, operationResult_ > 0 ? tr(STR_DONE) : tr(STR_ERROR_GENERAL_FAILURE));
    renderer.displayBuffer();
  }

 private:
  int cursorIndex = 0;
  int operationResult_ = 0;

  void applySelection() {
    operationResult_ = 0;
    auto& settings = SmudgeSettings::getInstance();
    if (cursorIndex == 0) {
      settings.sortMode = MenuSortMode::Alphabetical;
    } else if (cursorIndex == 1) {
      settings.sortMode = MenuSortMode::MostUsed;
    } else {
      const bool revoke = cursorIndex == 2;
      const auto proposal=HalLuaHttp::proposeTime();
      char date[40]{}; time_t now=proposal.epoch; tm utc{};
      if (gmtime_r(&now,&utc)) strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S UTC",&utc);
      std::unique_ptr<Activity> prompt;
      if (revoke) prompt=makeUniqueNoThrow<ConfirmationActivity>(renderer,mappedInput,tr(STR_LUA_NETWORK_REVOKE),"",true);
      else prompt=makeUniqueNoThrow<LuaConsentActivity>(renderer,mappedInput,date,tr(STR_LUA_TRUST_TIME),tr(STR_CONFIRM));
      if (!prompt) { LOG_ERR("LHTTP", "Dialog allocation failed"); return; }
      startActivityForResult(std::move(prompt), [this,revoke,proposal](const ActivityResult& result) {
        if (!result.isCancelled) {
          bool ok=revoke ? HalLuaHttp::revokeAll() : HalLuaHttp::trustTime(proposal);
          if (!ok) LOG_ERR("LHTTP", "Permission/time operation failed");
          operationResult_ = ok ? 1 : -1;
        }
        requestUpdate();
      });
      return;
    }
    settings.save();
    requestUpdate();
  }
};