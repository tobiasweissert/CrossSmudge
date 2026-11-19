import re

def block(text, token):
    a=text.index(token);k=text.index('{',a);d=1;z=k+1
    while d:
        d+=(text[z]=='{')-(text[z]=='}');z+=1
    return text[a:z]

def emit(root,out):
    h=(root/'src/activities/reader/EpubReaderActivity.h').read_text()
    t=(root/'src/activities/reader/EpubReaderActivity.cpp').read_text()
    snapshot=block(h,'struct ReaderSettingsSnapshot')+';'
    book=block(h,'struct BookReaderSettingsData')+';'
    constants=t[t.index('constexpr uint16_t DEFAULT_AUTO_PAGE_TURN_INTERVAL_S'):t.index('constexpr char BALANCED_SECTION_CACHE_SUFFIX')]
    code='''#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <HalStorage.h>
#include <Logging.h>
struct CrossPointSettings {
 static constexpr uint8_t MIN_SCREEN_MARGIN=0,INDEXING_FULL_SECTION=0,INDEXING_METHOD_COUNT=2,MAX_WORD_SPACING=20;
};
class EpubReaderActivity {public:
'''+snapshot+book+'''
};
using BookReaderSettingsData=EpubReaderActivity::BookReaderSettingsData;
struct TestSettings : EpubReaderActivity::ReaderSettingsSnapshot {
 char dictionarySdFontFamilyName[64]=""; uint8_t dictionaryFontPointSize=0;
} SETTINGS;
enum class EpubRenderMode {CrossSmudgeDefault=0};
uint8_t normalizeRenderModeRaw(uint8_t v){return v<=2?v:0;}
'''+constants+'\n'
    tokens=['uint16_t clampAutoPageTurnIntervalSeconds(', 'bool readExact(', 'bool writeExact(', 'bool readU8(', 'bool writeU8(', 'bool readU16(', 'bool writeU16(', 'void captureReaderSettings(', 'bool readReaderSettingsSnapshot(', 'bool writeReaderSettingsSnapshot(', 'BookReaderSettingsData loadBookReaderSettingsFile(', 'bool saveBookReaderSettingsFile(']
    for token in tokens:
        code+=block(t,token)+'\n'
    code+='''int main(){
 BookReaderSettingsData original;original.hasCustomReaderSettings=true;
 original.readerSettings.wordSpacing=7;
 original.readerSettings.characterSpacing=4;
 original.readerSettings.wordSpacingPercent=137;
 original.readerSettings.paragraphIndentSpaces=5;
 strcpy(original.dictionarySdFontFamilyName,"Owner Font");original.dictionaryFontPointSize=18;original.hasDictionaryFontOverride=true;
 assert(saveBookReaderSettingsFile("/book",original));
 auto read=loadBookReaderSettingsFile("/book");
 assert(read.hasCustomReaderSettings && read.readerSettings.wordSpacing==7);
 assert(read.readerSettings.characterSpacing==4&&read.readerSettings.wordSpacingPercent==137&&read.readerSettings.paragraphIndentSpaces==5);
 assert(read.hasDictionaryFontOverride&&read.dictionaryFontPointSize==18);
 // A v9 owner file has the identical prefix and inherits newly introduced layout fields.
 HalFile f;Storage.openFileForRead("TEST","/book/reader_settings.bin",f);std::vector<uint8_t> bytes(f.fileSize());f.read(bytes.data(),bytes.size());f.close();
 bytes[0]=9;bytes.resize(bytes.size()-3);
 Storage.openFileForWrite("TEST","/book/reader_settings.bin",f);f.write(bytes.data(),bytes.size());f.close();
 SETTINGS.characterSpacing=1;SETTINGS.wordSpacingPercent=123;SETTINGS.paragraphIndentSpaces=3;
 read=loadBookReaderSettingsFile("/book");
 assert(read.hasCustomReaderSettings&&read.readerSettings.wordSpacing==7);
 assert(read.readerSettings.characterSpacing==1&&read.readerSettings.wordSpacingPercent==123&&read.readerSettings.paragraphIndentSpaces==3);
 bytes[0]=10;Storage.openFileForWrite("TEST","/book/reader_settings.bin",f);f.write(bytes.data(),bytes.size());f.close();
 read=loadBookReaderSettingsFile("/book");assert(!read.hasCustomReaderSettings);
 puts("PASS actual per-book settings writer/reader: v10 modern typography, exact owner prefix, v9 inherit, truncated-tail refusal");
}
'''
    (out/'activities.cpp').write_text(code)
