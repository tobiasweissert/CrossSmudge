// Explicit rendering/application doubles; every UI data type and FreeInkApp is real.
struct GfxRenderer {
  enum class Orientation {Portrait, Landscape};
  Orientation getOrientation() const {return Orientation::Landscape;}
};
struct MappedInputManager {};
struct RenderLock {};
class Activity {
 public:
  Activity(const char*, GfxRenderer&, MappedInputManager&) {}
  virtual ~Activity() = default;
  virtual bool allowGlobalHomeSwipeGesture() const {return true;}
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
};
class ButtonNavigator {};
class OptionPopup {};
namespace freeink::ui {
class GfxRendererTarget : public DrawTarget {
 public:
  Size measureText(FontId, const char*, TextStyle) const override {return {10, 20};}
  int16_t lineHeight(FontId) const override {return 20;}
  void fill(Rect, Paint, uint8_t, uint8_t) override {}
  void stroke(Rect, Paint, uint8_t, uint8_t, uint8_t) override {}
  void line(Point, Point, uint8_t, Paint) override {}
  void triangle(Point, Point, Point, Paint) override {}
  void text(Rect, const char*, TextStyle) override {}
  void bitmap(Rect, BitmapRef, BitmapMode, Paint, Rotation) override {}
  DeviceContext deviceContext() const {return {};}
};
}
freeink::ui::GfxRendererTarget makeUiTarget(const GfxRenderer&) {return {};}
// SETTINGS_HEADER
// SETTINGS_METHODS
void SettingsActivity::onEnter() {}
void SettingsActivity::onExit() {}
void SettingsActivity::loop() {}
void SettingsActivity::render(RenderLock&&) {}
TEST(SettingsConstructor, ExactSectionOverloadCannotBecomeBoolean) {
  static_assert(std::is_constructible_v<SettingsActivity, GfxRenderer&, MappedInputManager&, const SettingsSection&>);
  static_assert(std::is_constructible_v<SettingsActivity, GfxRenderer&, MappedInputManager&, bool, bool, SettingsActivity::View>);
  GfxRenderer r;MappedInputManager input;
  SettingsActivity root(r,input);
  EXPECT_FALSE(root.dismissOnUpSwipe);EXPECT_FALSE(root.returnToParentOnClose);
  SettingsActivity drawer(r,input,true,true);
  EXPECT_TRUE(drawer.dismissOnUpSwipe);EXPECT_TRUE(drawer.returnToParentOnClose);
  for (auto section : {SettingsSection::General,SettingsSection::Display,SettingsSection::Reader,SettingsSection::Controls,SettingsSection::Library,SettingsSection::Network}) {
    SettingsActivity child(r,input,section);
    EXPECT_TRUE(child.returnToParentOnClose);EXPECT_FALSE(child.dismissOnUpSwipe);
    EXPECT_EQ(child.entrySection,section);
    EXPECT_EQ(child.entryOrientation,GfxRenderer::Orientation::Landscape);
  }
}
TEST(SettingsConstructor, SectionEntryRetainsOwnerTabsAndFocusesExistingActions) {
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::Display),0);
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::Reader),1);
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::Controls),2);
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::General),3);
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::Library),3);
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::Network),3);
  EXPECT_EQ(SettingsActivity::categoryForSection(SettingsSection::End),0);
  GfxRenderer r;MappedInputManager input;SettingsActivity child(r,input,SettingsSection::Network);
  std::vector<SettingInfo> rows{
    SettingInfo::Action(StrId::STR_NONE_OPT,SettingAction::About),
    SettingInfo::Action(StrId::STR_NONE_OPT,SettingAction::Library),
    SettingInfo::Action(StrId::STR_NONE_OPT,SettingAction::Network)};
  child.currentSettings=&rows;
  child.focusEntrySection();EXPECT_EQ(child.selectedSettingIndex,3);
  child.entrySection=SettingsSection::Library;child.focusEntrySection();EXPECT_EQ(child.selectedSettingIndex,2);
  child.entrySection=SettingsSection::Display;child.focusEntrySection();EXPECT_EQ(child.selectedSettingIndex,2);
}
