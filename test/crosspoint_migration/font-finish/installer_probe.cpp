#include <FontInstaller.h>

#include <cassert>
int main() {
  assert(FontInstaller::isValidFamilyName("Lexend Deca"));
  assert(FontInstaller::isValidFamilyName("Bitter-SD"));
  assert(!FontInstaller::isValidFamilyName("../Bitter"));
  assert(FontInstaller::isValidCpfontFilename("Lexend.Deca_14.cpfont"));
  assert(!FontInstaller::isValidCpfontFilename("Bitter.cpfont.tmp"));
  assert(!FontInstaller::isValidCpfontFilename("../Bitter_14.cpfont"));
  assert(FontInstaller::isValidVectorFontFilename("Font-Regular.OTF"));
  assert(FontInstaller::isValidVectorFontFilename("Font.ttc"));
  assert(!FontInstaller::isValidVectorFontFilename("Font.ttf.tmp"));
  assert(!FontInstaller::isValidVectorFontFilename("a/Font.ttf"));
}
