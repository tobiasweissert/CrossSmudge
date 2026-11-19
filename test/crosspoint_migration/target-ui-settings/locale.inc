I18n& I18n::getInstance() {static I18n i;return i;}
void I18n::setLanguage(Language language) {_language=language;}
TEST(Locale, LibraryRtlUsesGeneratedLocaleDomain) {
  for (auto lang : {Language::AR, Language::HE}) {
    I18N.setLanguage(lang);
    // RTL_LibraryActivity
    // RTL_LibrarySettingsActivity
  }
}
