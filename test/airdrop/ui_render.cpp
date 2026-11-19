// Renders the exact production content builder through pinned FreeInkUI components.
// SVG target uses declared monospace metrics; this is layout evidence, not panel/font/RF proof.
#include <FreeInkApp.h>
#include <FreeInkUI.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "network/AirDropReceiver.h"
#include "ui_render_strings.inc"
namespace fui = freeink::ui;
constexpr fui::ActionId Accept = 1, Decline = 2, Cancel = 3, Retry = 4;
struct GfxRenderer {
  int width = 480, height = 800;
};
struct MappedInputManager {};
struct HeaderRect {
  int x, y, width, height;
};
struct Metrics {
  int contentSidePadding = 24, buttonHintsHeight = 40;
};
struct UITheme {
  Metrics metrics;
  const Metrics& getMetrics() const { return metrics; }
  static UITheme& getInstance() {
    static UITheme theme;
    return theme;
  }
};
namespace TouchHeaderBackButton {
HeaderRect headerRect(const GfxRenderer& r, const MappedInputManager&) { return {0, 0, r.width, 60}; }
}  // namespace TouchHeaderBackButton
struct AirDropReceiveActivity {
  using UiApp = fui::FreeInkApp<4, 4>;
  GfxRenderer renderer;
  MappedInputManager mappedInput;
  airdrop::Status status_{};
  char textBuffer_[256]{};
  bool leaving_ = false, decisionSent_ = false, browserFailed_ = false, startFailed_ = false, radioStopped_ = false;
  void buildScreen(UiApp::ScreenType&);
};
static std::string escape(const char* s) {
  std::string out;
  for (; *s; ++s) {
    switch (*s) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out += *s;
    }
  }
  return out;
}
struct SvgTarget : fui::DrawTarget {
  int small = 24, body = 24;
  std::ostringstream svg;
  std::string requiredHost, requiredFilename;
  bool hostComplete = false, filenameComplete = false, deadlineComplete = false;
  int textCount = 0;
  int16_t lineHeight(fui::FontId f) const override { return static_cast<int16_t>(f == 0 ? small : body); }
  fui::Size measureText(fui::FontId f, const char* s, fui::TextStyle) const override {
    // Count UTF-8 glyph starts. Stable declared host metric, no claim of firmware kerning.
    int n = 0;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p)
      if ((*p & 0xc0) != 0x80) ++n;
    return {static_cast<int16_t>(n * lineHeight(f) / 2), lineHeight(f)};
  }
  void fill(fui::Rect r, fui::Paint p, uint8_t radius, uint8_t) override {
    if (p.kind == fui::PaintKind::None) return;
    svg << "<rect x='" << r.x << "' y='" << r.y << "' width='" << r.width << "' height='" << r.height << "' rx='"
        << int(radius) << "' fill='" << (p.color == fui::Color::White ? "white" : "black") << "'/>\n";
  }
  void stroke(fui::Rect r, fui::Paint p, uint8_t width, uint8_t radius, uint8_t) override {
    if (p.kind == fui::PaintKind::None) return;
    svg << "<rect x='" << r.x << "' y='" << r.y << "' width='" << r.width << "' height='" << r.height << "' rx='"
        << int(radius) << "' fill='none' stroke='black' stroke-width='" << int(width) << "'/>\n";
  }
  void line(fui::Point a, fui::Point b, uint8_t width, fui::Paint) override {
    svg << "<line x1='" << a.x << "' y1='" << a.y << "' x2='" << b.x << "' y2='" << b.y
        << "' stroke='black' stroke-width='" << int(width) << "'/>\n";
  }
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {}
  void text(fui::Rect r, const char* value, fui::TextStyle style) override {
    ++textCount;
    std::string shown;
    fui::layoutText(*this, r, value, style, [&](const char* line, fui::Rect at) {
      shown += line;
      svg << "<text x='" << at.x << "' y='" << (at.y + lineHeight(style.font) * 4 / 5)
          << "' font-family='monospace' font-size='" << lineHeight(style.font) << "' font-weight='"
          << (style.bold ? "bold" : "normal") << "' fill='" << (style.inverted ? "white" : "black") << "'>"
          << escape(line) << "</text>\n";
    });
    if (requiredFilename == value) filenameComplete = shown == requiredFilename;
    if (std::strncmp(value, "Decide within", 13) == 0) deadlineComplete = shown == value;
    if (requiredHost == value) {
      hostComplete = shown == requiredHost;
      if (!hostComplete)
        std::fprintf(stderr, "host clipped h=%d maxLines=%u shown=%zu required=%zu\n", r.height, style.maxLines,
                     shown.size(), requiredHost.size());
    }
  }
};
#include "ui_render_extracted.inc"
int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string out = argv[1];
  int files = 0;
  for (int landscape = 0; landscape < 2; ++landscape)
    for (int large = 0; large < 2; ++large)
      for (int scene = 0; scene < 8; ++scene) {
        AirDropReceiveActivity activity;
        activity.renderer.width = landscape ? 800 : 480;
        activity.renderer.height = landscape ? 480 : 800;
        auto& status = activity.status_;
        status.nowMs = 10000;
        status.deadlineMs = 40000;
        std::memset(status.offer.sender, 's', 128);
        status.offer.sender[128] = 0;
        const std::string host = std::string(60, 'a') + "." + std::string(60, 'b') + ".com";
        std::strcpy(status.offer.host, host.c_str());
        std::snprintf(status.offer.url, sizeof(status.offer.url), "https://%s/a/native/shared/link#fragment",
                      host.c_str());
        const std::string filenameLimit = std::string(115, 'f') + ".epub";
        std::strcpy(status.offer.filename, filenameLimit.c_str());
        status.offer.fileSize = airdrop::MaxFileBytes;
        const char* name = "";
        switch (scene) {
          case 0:
            name = "url-consent";
            status.state = airdrop::State::Pending;
            status.offer.kind = airdrop::OfferKind::Url;
            break;
          case 1:
            name = "file-consent";
            status.state = airdrop::State::Pending;
            status.offer.kind = airdrop::OfferKind::File;
            break;
          case 2:
            name = "progress";
            status.state = airdrop::State::Receiving;
            status.wireBytes = 12000000;
            status.payloadBytes = 10000000;
            status.committedBytes = 9000000;
            break;
          case 3:
            name = "stopping";
            status.state = airdrop::State::Stopping;
            activity.leaving_ = true;
            break;
          case 4:
            name = "retained-radio";
            status.state = airdrop::State::Error;
            status.ownershipUncertain = true;
            break;
          case 5:
            name = "retained-sd";
            status.state = airdrop::State::Error;
            status.publicationUncertain = true;
            activity.radioStopped_ = true;
            break;
          case 6:
            name = "saved";
            status.state = airdrop::State::Success;
            std::strcpy(status.savedPath, "/Inbox/Image-heavy (2).epub");
            break;
          case 7:
            name = "browser-retry";
            status.state = airdrop::State::UrlReady;
            activity.browserFailed_ = true;
            activity.radioStopped_ = true;
            break;
        }
        SvgTarget target;
        if (large) {
          target.small = 28;
          target.body = 28;
        }
        target.requiredHost = scene == 0 ? host : "";
        target.requiredFilename = scene == 1 ? filenameLimit : "";
        fui::DeviceContext device;
        device.width = activity.renderer.width;
        device.height = activity.renderer.height;
        device.hasTouch = true;
        device.safeArea = {8, 8, 8, 8};
        fui::InputSnapshot input;
        fui::InteractionBuffer<4> interactions;
        fui::Frame<4> frame(target, device, input, interactions);
        fui::ThemeTokens theme;
        theme.smallText.font = 0;
        theme.bodyText.font = 1;
        theme.spaceMd = large ? 10 : 8;
        theme.rowHeight = large ? 56 : 48;
        fui::Screen<4> screen(frame, theme);
        target.text({24, 8, static_cast<int16_t>(device.width - 48), 48}, "AirDrop", fui::TextStyle{1});
        activity.buildScreen(screen);
        assert(target.textCount > 2);
        assert(!interactions.overflowed());
        if (scene <= 1) {
          assert(interactions.count() == 2);
          assert(interactions.data()[0].action == Decline);
          assert(interactions.data()[1].action == Accept);
        }
        if (scene == 0) assert(target.hostComplete);
        if (scene == 1) assert(target.filenameComplete);
        if (scene <= 1) assert(target.deadlineComplete);
        for (size_t i = 0; i < interactions.count(); ++i) {
          const auto& r = interactions.data()[i].rect;
          assert(r.height >= 44 && r.x >= 8 && r.right() <= device.width - 8 && r.bottom() <= device.height - 8);
        }
        std::string filename =
            out + "/" + name + (landscape ? "-landscape" : "-portrait") + (large ? "-large" : "-normal") + ".svg";
        std::ofstream file(filename);
        file << "<svg xmlns='http://www.w3.org/2000/svg' width='" << device.width << "' height='" << device.height
             << "' viewBox='0 0 " << device.width << " " << device.height
             << "'><rect width='100%' height='100%' fill='white'/>\n"
             << target.svg.str() << "</svg>\n";
        ++files;
      }
  std::printf(
      "PASS %d SVG content renders through actual production builder + pinned SDK; both orientations/scales, intact "
      "long host and consent actions\n",
      files);
}
