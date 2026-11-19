#!/usr/bin/env python3
"""Execute production visitor/factories + real settings/header types, dependency doubles."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
R=Path(__file__).resolve().parents[3];T=Path(__file__).resolve().parent
p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args();O=a.evidence;O.mkdir(parents=True,exist_ok=True);D=O/'stubs';(D/'Epub').mkdir(parents=True,exist_ok=True)
(D/'Epub/ReaderRenderSpec.h').write_text('#pragma once\nenum class EpubRenderMode {CrossSmudgeDefault};\nstruct ReaderRenderSpec {};\n')
(D/'HalStorage.h').write_text('#pragma once\nclass HalFile {};\n')
(D/'BlePageTurner.h').write_text('#pragma once\nnamespace bleturner {struct Config {bool enabled=false;};}\n')
(D/'BlePageTurnerJson.h').write_text('#pragma once\n#include <ArduinoJson.h>\n#include <BlePageTurner.h>\nnamespace bleturner {inline void writeJson(const Config& c,JsonDocument& d){d["bleEnabled"]=c.enabled;} inline bool readJson(Config& c,JsonVariantConst d){c.enabled=d["bleEnabled"] | false;return true;}}\n')
for name in ['Arduino.h','Logging.h']:(D/name).write_bytes((T.parent/'settings/stubs'/name).read_bytes())
h=(R/'src/SettingsList.h').read_text();info=(R/'src/activities/settings/SettingsActivity.h').read_text();cpp=(R/'src/SettingsList.cpp').read_text()
info=info[info.index('enum class SettingType'):info.index('class SettingsActivity')]
# Exact real factories used by the visitor; the registry/renderer/activity class
# and the stale static list are intentionally not under this host compiler.
def function(name):
 start=h.index('inline ',h.rfind('\n',0,h.index(name+'('))+1);body=h.index('{',start);depth=1;end=body+1
 # Factory bodies contain no braces in strings.
 while depth:
  if h[end]=='{':depth+=1
  elif h[end]=='}':depth-=1
  end+=1
 return h[start:end]
names=['fontSizePointLabel','appendBuiltinFontSizeOption','buildBuiltinFontSizeSetting','removeEnumRawValue','settingKeyIs','buildSleepScreenSetting','shortcutRawValue','appendShortcutOptions','buildShortcutSetting','buildHomeButtonActionSetting']
helpers='\n'.join(function(n) for n in names)
ids=sorted(set(re.findall(r'StrId::(\w+)',cpp+helpers+info)))
head='''#include <algorithm>
#include <functional>
#include <iterator>
#include <utility>
#include <vector>
#include <string>
#include <cstring>
#include <set>
#include <cassert>
#include <cstdio>
#include "CrossPointSettings.h"
#include "catalog-deps.h"
'''
head+='enum class StrId {'+','.join(ids)+'};\nstruct I18nDouble {const char* get(StrId)const{return "label";}}; inline I18nDouble I18N;\n'
head+=info
head+='''
namespace QuickActions {
inline bool tilt=false;
inline bool supportsTiltPageTurn(){return tilt;}
inline constexpr auto shortcutActionOrder=std::array{CrossPointSettings::IGNORE,CrossPointSettings::QUICK_LOCK,CrossPointSettings::PREVIOUS_PAGE,CrossPointSettings::TOGGLE_TILT_PAGE_TURN};
inline bool isActionAvailable(uint8_t){return true;}
inline StrId actionLabel(uint8_t){return StrId::STR_NONE_OPT;}
}
struct FrontlightDouble {bool present()const{return true;}}; inline FrontlightDouble Frontlight;
enum class ShortcutOptionCatalog {PowerButton,ButtonChord,LongPress,HomeButton};
constexpr uint8_t SHORTCUT_OPTION_UNAVAILABLE=UINT8_MAX;
'''
settings_impl=(R/'src/CrossPointSettings.cpp').read_text()
point_method=re.search(r'uint8_t CrossPointSettings::getReaderFontPointSize\(.*?\n\}',settings_impl,re.S).group()
source=head+helpers+'\n'+point_method+'\n'+cpp.replace('#include "SettingsList.h"','')+'\n'+(T/'catalog-main.inc').read_text()
(O/'catalog.cpp').write_text(source)
cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-unused-function','-fno-exceptions','-fno-rtti','-DARDUINOJSON_ENABLE_ARDUINO_STRING=0','-I'+str(D),'-I'+str(T),'-I'+str(R/'src'),'-I'+str(R/'lib/Serialization'),'-I/root/github/.host-data/crosssmudge/gallery-native-deps/sticky-simulator/ArduinoJson/src',str(O/'catalog.cpp'),'-o',str(O/'catalog')]
if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie']
r=subprocess.run(cmd,capture_output=True,text=True,timeout=70);log=r.stdout+r.stderr;rc=r.returncode
if not rc:
 r=subprocess.run([str(O/'catalog')],capture_output=True,text=True,timeout=20);log+=r.stdout+r.stderr;rc=r.returncode
(O/'catalog.log').write_text(log);print(log)
inputs=[R/'src/SettingsList.cpp',R/'src/SettingsList.h',R/'src/CrossPointSettings.h',R/'src/CrossPointSettings.cpp',R/'src/activities/settings/SettingsActivity.h',T/'catalog-deps.h',T/'catalog-main.inc',Path(__file__)]
(O/'catalog.json').write_text(json.dumps({'returncode':rc,'command':cmd,'coverage':'extracted production visitor + factory bodies, actual SettingInfo and settings header; translation, credentials, QuickActions and renderer doubles','inputs':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}},indent=2));raise SystemExit(rc)
