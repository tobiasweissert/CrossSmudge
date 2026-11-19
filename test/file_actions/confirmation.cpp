#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#define private public
#include "Subject.h"
#undef private
int main() {
  for (bool landscape : {false, true}) {
    GfxRenderer::width = landscape ? 800 : 480;
    GfxRenderer::height = landscape ? 480 : 800;
    GfxRenderer renderer;
    HalGPIO gpio;
    MappedInputManager input(gpio, renderer);
    FileMutationConfirmationActivity subject(renderer, input, "Delete?", "Original.epub");
    subject.onEnter();
    const auto layout = subject.confirmPopup.getLayout(renderer);
    const auto box = layout.options.at(1);
    input.x = box.x + box.width / 2;
    input.y = box.y + box.height / 2;
    // A release-only snapshot queued before the first completed paint is not consent.
    input.tap = true;
    subject.loop();
    assert(subject.finished == 0 && "queued tap must not confirm before paint");
    input.frame();
    subject.render(Activity::RenderLock(subject));
    subject.loop();
    subject.loop();
    input.tap = true;
    subject.loop();
    assert(subject.finished == 0 && "release-only tap after paint is not fresh consent");
    input.frame();
    input.down = true;
    input.contact = true;
    subject.loop();
    input.frame();
    input.contact = false;
    input.tap = true;
    subject.loop();
    assert(subject.finished == 1 && !subject.result.isCancelled && "fresh touch must confirm exactly once");
    {
      MappedInputManager keys(gpio, renderer);
      FileMutationConfirmationActivity hw(renderer, keys, "Delete?", "Keys.epub");
      hw.onEnter();
      hw.render(Activity::RenderLock(hw));
      hw.loop();
      hw.loop();
      keys.press.insert(MappedInputManager::Button::Down);
      hw.loop();
      keys.frame();
      keys.release.insert(MappedInputManager::Button::Down);
      ButtonNavigator::injectNextRelease();
      hw.loop();
      keys.frame();
      hw.render(Activity::RenderLock(hw));
      hw.loop();
      hw.loop();
      keys.press.insert(MappedInputManager::Button::Confirm);
      hw.loop();
      assert(hw.finished == 1 && !hw.result.isCancelled && "fresh hardware selects visible Confirm");
    }
    {
      MappedInputManager keys(gpio, renderer);
      FileMutationConfirmationActivity cancel(renderer, keys, "Delete?", "Cancel.epub");
      cancel.onEnter();
      cancel.render(Activity::RenderLock(cancel));
      cancel.loop();
      cancel.loop();
      keys.press.insert(MappedInputManager::Button::Back);
      cancel.loop();
      keys.frame();
      keys.release.insert(MappedInputManager::Button::Back);
      cancel.loop();
      keys.frame();
      cancel.loop();
      assert(cancel.finished == 1 && cancel.result.isCancelled);
    }
    {
      MappedInputManager touch(gpio, renderer);
      FileMutationConfirmationActivity repaint(renderer, touch, "Delete?", "Repaint.epub");
      repaint.onEnter();
      repaint.render(Activity::RenderLock(repaint));
      repaint.loop();
      repaint.loop();
      touch.x = input.x;
      touch.y = input.y;
      touch.down = true;
      touch.contact = true;
      repaint.loop();
      repaint.render(Activity::RenderLock(repaint));
      touch.frame();
      touch.contact = false;
      touch.tap = true;
      repaint.loop();
      assert(repaint.finished == 0 && "repaint invalidates an outstanding contact");
    }
  }
  std::cout << "PASS mutation confirmation stale/fresh touch hardware Back repaint both orientations\n";
}
