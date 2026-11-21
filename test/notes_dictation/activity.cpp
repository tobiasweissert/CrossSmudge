#include "../../src/activities/notes/DictationActivity.h"
#include "ui.h"
#define LOG_DBG(...) ((void)0)
static bool sConnectionAttemptLoggingActive;
#include "picker.inc"
using namespace dictation;
int main() {
  assert(configure("fixture-neutral"));
  GfxRenderer r;
  MappedInputManager m;
  DictationActivity ui(r, m);
  auto action = [&](const char* label) {
    ui.render({});
    assert(std::find(r.labels.begin(), r.labels.end(), label) != r.labels.end());
    ui.loop();
    ui.loop();
    m.confirm = true;
    ui.loop();
    assert(!ui.finished);
    ui.loop();
    m.press = m.held = true;
    ui.loop();
    m.press = m.held = false;
    m.confirm = true;
    ui.loop();
  };
  WiFi.connected = false;
  WiFi.radio = WIFI_OFF;
  action("Connect Wi-Fi");
  assert(dynamic_cast<WifiSelectionActivity*>(ui.child.get()));
  auto picker = [&](bool success) {
    auto* child = dynamic_cast<WifiSelectionActivity*>(ui.child.get());
    assert(child);
    WiFi.radio = WIFI_STA;
    WiFi.connected = success;
    child->onComplete(success);
    child->onExit();
    ui.complete(child->result);
  };
  picker(false);
  assert(WiFi.radio == WIFI_OFF && !micStarts && !posts);
  action("Connect Wi-Fi");
  picker(true);
  ui.render({});
  m.confirm = true;
  ui.loop();
  ui.loop();
  assert(!micStarts && !task && !posts);
  action("Record (5 seconds)");
  assert(!posts);
  tick = [&] {
    assert(!posts);
    action("Send audio to OpenAI");
  };
  task();
  assert(posts == 1);
  action("Review transcript");
  assert(dynamic_cast<KeyboardEntryActivity*>(ui.child.get()) && !ui.finished);
  ui.complete({false, KeyboardResult{"Manually reviewed"}});
  assert(ui.finished && std::get<KeyboardResult>(ui.result.data).text == "Manually reviewed");
  ui.onExit();
  assert(!WiFi.connected && WiFi.radio == WIFI_OFF);
  WiFi.connected = true;
  WiFi.radio = WIFI_STA;
  DictationActivity cancel(r, m);
  cancel.render({});
  m.back = true;
  cancel.loop();
  assert(cancel.result.isCancelled && cancel.finished && posts == 1);
  cancel.onExit();
  for (bool panel : {false, true}) {
    DictationActivity owner(r, m);
    owner.render({});
    owner.loop();
    owner.loop();
    m.press = m.held = true;
    owner.loop();
    m.held = m.press = false;
    m.confirm = true;
    owner.loop();
    const int before = micStarts;
    tick = [&] { task(); };
    if (panel)
      owner.onFrontlightPanelOpened();
    else
      owner.onInputLockChanged(true);
    owner.loop();
    assert(owner.finished && owner.result.isCancelled && posts == 1 && micStarts == before && !power);
    owner.onExit();
  }
  DictationActivity reconnect(r, m);
  WiFi.connected = false;
  reconnect.render({});
  reconnect.loop();
  reconnect.loop();
  m.touch = true;
  reconnect.loop();
  WiFi.connected = true;
  reconnect.render({});
  reconnect.loop();
  m.touch = false;
  m.tap = true;
  reconnect.loop();
  reconnect.render({});
  assert(std::find(r.labels.begin(), r.labels.end(), "Record (5 seconds)") != r.labels.end());
  reconnect.onExit();
  for (bool touchscreen : {false, true}) {
    DictationActivity held(r, m);
    held.render({});
    held.loop();
    held.loop();
    m.held = m.press = true;
    held.loop();
    m.held = m.press = false;
    m.confirm = true;
    held.loop();
    held.render({});
    held.loop();
    held.loop();
    m.touch = touchscreen;
    m.held = m.press = !touchscreen;
    held.loop();
    m.press = false;
    bool delivered = false;
    tick = [&] {
      if (delivered) return;
      delivered = true;
      held.render({});
      held.loop();
      m.held = m.touch = false;
      m.tap = touchscreen;
      m.confirm = !touchscreen;
      held.loop();
      held.loop();
    };
    task();
    held.render({});
    assert(delivered && posts == 1 &&
           std::find(r.labels.begin(), r.labels.end(), "Recording expired.") != r.labels.end());
    held.onExit();
  }
  puts("PASS Stop press held through Ready paint cannot authorize Send (button and touch)");
  for (bool landscape : {false, true}) {
    r.w = landscape ? 800 : 480;
    r.h = landscape ? 480 : 800;
    I18N.setLanguage(Language::DE);
    DictationActivity de(r, m);
    de.render({});
    assert(std::find(r.labels.begin(), r.labels.end(), "Aufnehmen (5 Sekunden)") != r.labels.end());
    de.loop();
    de.loop();
    m.held = m.press = true;
    de.loop();
    m.held = m.press = false;
    m.confirm = true;
    de.loop();
    tick = [&] {
      de.render({});
      assert(std::find(r.labels.begin(), r.labels.end(), "Audio an OpenAI senden") != r.labels.end());
      assert(std::any_of(r.labels.begin(), r.labels.end(), [](auto& s) { return s.find("Kosten") != s.npos; }));
    };
    task();
    de.onExit();
    I18N.setLanguage(Language::FR);
    DictationActivity unavailable(r, m);
    unavailable.render({});
    assert(std::find(r.labels.begin(), r.labels.end(), "Unavailable") != r.labels.end());
    unavailable.onExit();
  }
  puts("PASS original German consent and paid action in both panel dimensions; unsupported locale unavailable");
  puts(
      "PASS panel and lock cancel queued capture; held touch across reconnect does not record; borrowed Wi-Fi never "
      "disconnected");
  puts("PASS actual native activity: Record, paid Send, editable review, explicit Done result, Back cancellation");
}
