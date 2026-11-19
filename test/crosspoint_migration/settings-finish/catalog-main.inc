uint8_t CrossPointSettings::defaultUiScale() {return UI_SCALE_LARGE;}
int main() {
 std::set<std::string> keys;size_t count=0;bool flags[2]={};
 forEachBaseSetting([](void* p,SettingInfo& s){
   auto keys=static_cast<std::set<std::string>*>(p);
   if(s.key)assert(keys->insert(s.key).second);
 },&keys);
 forEachBaseSetting([](void* p,SettingInfo&){++*static_cast<size_t*>(p);},&count);
 assert(count==countBaseSettings());assert(count>100);
 for(auto k:{"sleepScreen","fontSize","lineHeightPercent","wordSpacing","characterSpacing","clockTimezone","clockDst","koSyncClippings","pluginsEnabled","opdsFilenameFormat","trackReadingStats"})assert(keys.contains(k));
 forEachBaseSetting([](void* p,SettingInfo& s){
   auto flags=static_cast<bool*>(p);
   if(settingKeyIs(s,"sleepScreen")){flags[0]=!s.enumRawValues.empty();assert(s.enumRawValues.front()==CrossPointSettings::BLANK);}
   if(settingKeyIs(s,"lineHeightPercent"))assert(s.valueRange.min==70&&s.valueRange.max==200);
   if(settingKeyIs(s,"frontlightScheduleStart"))flags[1]=s.value16Ptr!=nullptr;
 },flags);
 assert(flags[0]&&flags[1]);
 // Dictionary callbacks own their entries after the input vector is destroyed.
 auto dict=buildDictionarySetting(std::vector<DictionaryEntry>{{"one","/dict/one"},{"two","/dict/two"}});
 dict.valueSetter(2);assert(Dictionary::path=="/dict/two");assert(dict.valueGetter()==2);
 dict.valueSetter(0);assert(Dictionary::path.empty());
 std::printf("streaming visitor count=%zu unique_keys=%zu; owner raw enums/value16 + modern union + owned dictionary callbacks PASS\n",count,keys.size());
}
