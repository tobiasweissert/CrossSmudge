// The Arduino SDK carries a weak descriptor from its precompiled build.
// Supply this profile's descriptor at link time; esptool fills the ELF hash.
#if !defined(SIMULATOR)
#include "sdkconfig.h"
#include "esp_app_desc.h"
#include "esp_idf_version.h"
#define OWNER_STR_IMPL(x) #x
#define OWNER_STR(x) OWNER_STR_IMPL(x)
const __attribute__((section(".rodata_desc"), used)) esp_app_desc_t esp_app_desc = {
    .magic_word = ESP_APP_DESC_MAGIC_WORD,
#ifdef CONFIG_BOOTLOADER_APP_SECURE_VERSION
    .secure_version = CONFIG_BOOTLOADER_APP_SECURE_VERSION,
#endif
    .version = CROSSCUB_APP_VERSION,
    .project_name = "Crosscub",
    .time = __TIME__,
    .date = __DATE__,
    .idf_ver = OWNER_STR(ESP_IDF_VERSION_MAJOR) "." OWNER_STR(ESP_IDF_VERSION_MINOR) "." OWNER_STR(ESP_IDF_VERSION_PATCH),
    .min_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MIN_FULL,
    .max_efuse_blk_rev_full = CONFIG_ESP_EFUSE_BLOCK_REV_MAX_FULL,
    .mmu_page_size = 31 - __builtin_clz(CONFIG_MMU_PAGE_SIZE),
};
_Static_assert(sizeof(CROSSCUB_APP_VERSION) <= sizeof(esp_app_desc.version), "Crosscub version too long");
#endif
