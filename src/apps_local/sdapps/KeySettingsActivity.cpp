#include "KeySettingsActivity.h"
#if defined(FREEINK_DEVICE_STICKY)
#include <Memory.h>
#include <algorithm>
#include <Logging.h>
#include "services/ProviderKey.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
namespace sdapps {
void KeySettingsActivity::onEnter() { UiListActivity::onEnter(); refresh("Key is stored in unencrypted device NVS"); }
void KeySettingsActivity::refresh(const char* message) {
  std::string key;
  const bool present = stickychat::loadProviderKey(key);
  std::fill(key.begin(), key.end(), '\0');
  { RenderLock lock; configured = present; status = message;
    rows[0].label = configured ? "Replace key (configured, masked)" : "Enter API key";
    rows[0].subtitle = status;
    rows[1].label = "Remove API key";
    rows[2].label = "Private on device; not encrypted at rest";
    rows[2].subtitle = "Removal is logical deletion, not secure erasure. Enter keys only here.";
    for (int i=0;i<3;++i) rows[i].actionValue=i;
    resetUi();
  }
  requestUpdate();
}
void KeySettingsActivity::buildScreen(UiScreen& screen) {
  const auto& m=UITheme::getInstance().getMetrics();const auto safe=UITheme::getInstance().getScreenSafeArea(renderer,true,false);
  screen.setContentMarginFromScreen({static_cast<int16_t>(safe.y+m.topPadding+m.headerHeight),
    static_cast<int16_t>(renderer.getScreenWidth()-safe.x-safe.width),
    static_cast<int16_t>(renderer.getScreenHeight()-safe.y-safe.height+m.buttonHintsHeight),static_cast<int16_t>(safe.x)});
  freeink::ui::ListProps props{};props.items=rows;props.count=3;props.action=ACTION_ROW;
  props.inputMask=freeink::ui::InputTouch;props.subtitleText=screen.theme().smallText;props.subtitleText.maxLines=3;
  syncListViewport(screen,props);screen.list(props);
}
void KeySettingsActivity::activateIndex(int index) {
  if(index==0) {
    auto activity=makeUniqueNoThrow<KeyboardEntryActivity>(renderer,mappedInput,"OpenAI API key","",512,InputType::Password);
    if(!activity){LOG_ERR("SDAPP","OOM: key keyboard");refresh("Could not open keyboard");return;}
    startActivityForResult(std::move(activity),[this](const ActivityResult& result){
      if(result.isCancelled){refresh("Key unchanged");return;}
      const auto* value=std::get_if<KeyboardResult>(&result.data);
      refresh(value && stickychat::saveProviderKey(value->text)?"API key saved":"Invalid key or storage error; key not saved");
    });
  } else if(index==1) {
    auto activity=makeUniqueNoThrow<ConfirmationActivity>(renderer,mappedInput,"Remove API key?","Logical deletion only; physical flash recovery may remain possible.");
    if(!activity){LOG_ERR("SDAPP","OOM: key confirmation");return;}
    startActivityForResult(std::move(activity),[this](const ActivityResult& result){
      if(!result.isCancelled)refresh(stickychat::removeProviderKey()?"API key removed":"Could not remove API key");
    });
  }
}
}
#endif
