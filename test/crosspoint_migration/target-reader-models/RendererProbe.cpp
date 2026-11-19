#include <FreeInkUIGfxRenderer.h>
#include <GfxRenderer.h>

#include <cassert>
#include <cstring>

int main() {
  HalDisplay display(64, 48);
  GfxRenderer renderer(display);
  renderer.begin();
  assert(!renderer.isStripTargetActive());
  for (auto orientation : {GfxRenderer::Portrait, GfxRenderer::LandscapeClockwise, GfxRenderer::PortraitInverted,
                           GfxRenderer::LandscapeCounterClockwise}) {
    renderer.setOrientation(orientation);
    renderer.clearScreen();
    uint8_t scratch[64 / 8 * 2];
    std::memset(scratch, 0xFF, sizeof(scratch));
    renderer.beginStripTarget(scratch, 0, 2);
    const GfxRenderer& readOnly = renderer;
    assert(readOnly.isStripTargetActive());
    assert(readOnly.getWriteTarget() == scratch);
    renderer.clearScreen(0);
    for (auto byte : scratch) assert(byte == 0);
    for (size_t i = 0; i < renderer.getBufferSize(); ++i) assert(renderer.getFrameBuffer()[i] == 0xFF);
    renderer.endStripTarget();
    assert(!readOnly.isStripTargetActive());
    assert(readOnly.getWriteTarget() == renderer.getFrameBuffer());
  }
  freeink::ui::GfxRendererTarget sdkTarget(renderer, true);
  const auto device = sdkTarget.deviceContext();
  assert(device.width == renderer.getScreenWidth());
  assert(device.height == renderer.getScreenHeight());
}
