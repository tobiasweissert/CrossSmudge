#include "components/UiAppHost.h"
struct NetworkHostProbe {
  // NETWORK_ALIAS
};
TEST(NetworkApi, NonTemplatedHostRetainsRealAppCapacity) {
  EXPECT_TRUE((std::is_same_v<NetworkHostProbe::UiHost::App, freeink::ui::FreeInkApp<24, 6>>));
}
#include "components/icons/listIcons.h"
#include "components/icons/readingStatsIcons.h"
#include "components/icons/airdropIcons.h"
// ICON_BODY
TEST(NetworkApi, ExistingGenericRowsKeepTheir32PixelArtwork) {
  EXPECT_EQ(listIconFor(UIIcon::Wifi,32).data,icon_wifi_32.bits);
  EXPECT_EQ(listIconFor(UIIcon::Library,32).data,icon_lyra_library_32.bits);
  EXPECT_EQ(listIconFor(UIIcon::Hotspot,32).data,icon_radio_tower_32.bits);
}
TEST(NetworkApi, IconsPreserveOwnerBitsAndMaskPolarity) {
  for (auto icon : {&icon_usb_32, &icon_chevrons_left_right_ellipsis_32, &icon_reading_stats_32, &icon_airdrop_32}) {
    auto bitmap = freeink::ui::bitmapFromIcon(*icon);
    EXPECT_EQ(bitmap.data, icon->bits);
    EXPECT_EQ(bitmap.width, 32);
    EXPECT_EQ(bitmap.height, 32);
    EXPECT_EQ(bitmap.format, freeink::ui::BitmapFormat::Mask1);
  }
}
