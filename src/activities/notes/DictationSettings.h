#pragma once
#include <DictationPrivacy.h>
#include <I18n.h>

#include "DictationDevice.h"
#include "activities/reader/QrDisplayActivity.h"
#include "activities/util/ConfirmationActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "activities/util/OptionSelectionActivity.h"
namespace dictation {
inline void openSettings(Activity& owner, GfxRenderer& renderer, MappedInputManager& input) {
  std::vector<std::string> choices{
      tr(STR_VOICE_QR), dictation::configured() ? tr(STR_VOICE_REPLACE) : tr(STR_VOICE_ENTER), tr(STR_VOICE_FORGET)};
  owner.startActivityForResult(
      std::make_unique<OptionSelectionActivity>(renderer, input, tr(STR_NOTES_DICTATION), StrId::STR_NOTES_DICTATION,
                                                choices, 0, false, true),
      [&owner, &renderer, &input](const ActivityResult& r) {
        auto option = std::get_if<OptionSelectionResult>(&r.data);
        if (r.isCancelled || !option) return;
        if (option->index == 0)
          owner.startActivityForResult(std::make_unique<QrDisplayActivity>(renderer, input, dictation::setupUrl),
                                       [](const ActivityResult&) {});
        if (option->index == 2) dictation::forget();
        if (option->index != 1) return;
        if (!provisioningAvailable()) {
          owner.startActivityForResult(
              std::make_unique<ConfirmationActivity>(renderer, input, tr(STR_VOICE_UNAVAILABLE), tr(STR_VOICE_PRIVACY)),
              [](const ActivityResult&) {});
          return;
        }
        dictationPrivacy::arm();
        owner.startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, input, tr(STR_VOICE_BILLED), "",
                                                                             dictation::maxKey, InputType::Password, 1),
                                     [&owner, &renderer, &input](const ActivityResult& entered) {
                                       auto k = std::get_if<KeyboardResult>(&entered.data);
                                       if (!entered.isCancelled && k && !dictation::configure(k->text))
                                         owner.startActivityForResult(
                                             std::make_unique<ConfirmationActivity>(
                                                 renderer, input, tr(STR_VOICE_INVALID), tr(STR_VOICE_PRINTABLE)),
                                             [](const ActivityResult&) {});
                                     });
      });
}
}  // namespace dictation
