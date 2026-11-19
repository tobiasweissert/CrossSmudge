#include "HomeButtonSettings.h"
TEST(HomeActions, PersistedOwnerOrdinalsRemainUnchanged) {
  static_assert(static_cast<unsigned>(HomeButtonAction::Home) == 0);
  static_assert(static_cast<unsigned>(HomeButtonAction::ReaderMenu) == 9);
  static_assert(static_cast<unsigned>(HomeButtonAction::ToggleFrontlight) == 10);
  static_assert(static_cast<unsigned>(HomeButtonAction::FileBrowser) == 12);
  EXPECT_EQ(std::size(home_button::ACTION_LABELS), 13u);
  EXPECT_EQ(home_button::ACTION_LABELS[9], StrId::STR_READER_MENU);
  EXPECT_STREQ(home_button::KEYS[0], "homeButtonTapAction");
  EXPECT_STREQ(home_button::KEYS[1], "homeButtonDoubleTapAction");
  EXPECT_STREQ(home_button::KEYS[2], "homeButtonLongPressAction");
  EXPECT_TRUE(home_button::isSetting(&CrossPointSettings::homeButtonTapAction));
  EXPECT_FALSE(home_button::isSetting(&CrossPointSettings::lineHeightPercent));
}
