#!/usr/bin/env python3
"""Compile production menu tables and web exit/dispatch methods under route spies."""
from pathlib import Path
import re,subprocess,tempfile,json
ROOT=Path(__file__).resolve().parents[2]
menu=(ROOT/'src/activities/network/NetworkModeSelectionActivity.cpp').read_text()
header=(ROOT/'src/activities/network/NetworkModeSelectionActivity.h').read_text()
web=(ROOT/'src/activities/network/CrossPointWebServerActivity.cpp').read_text()
enum=re.search(r'enum class NetworkMode \{.*?\};',header,re.S).group()
assert 'AIRDROP_RECEIVE' in enum, 'RED: AirDrop enum absent'
arrays=menu[menu.index('constexpr NetworkMode menuModes[]'):menu.index('}  // namespace')]
keys=sorted(set(re.findall(r'StrId::(STR_\w+)',arrays)))
strids='enum class StrId {'+','.join(keys)+'};\n'
icons='enum class UIIcon {Wifi,Library,Hotspot,Transfer};\n'
def method(name):
 m=re.search(r'(?:bool|void) CrossPointWebServerActivity::'+name+r'\([^)]*\) \{',web)
 assert m,name
 a=m.start();i=web.index('{',a);depth=1;j=i+1
 while depth:
  if web[j]=='{':depth+=1
  if web[j]=='}':depth-=1
  j+=1
 return web[a:j]
prelude='''
#include <cassert>
#include <cstddef>
#include <string>
#include <cstdio>
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
static int resets=0,legacyStops=0,managerCalls=0,returns=0;
static bool allocate=true;
constexpr int WIFI_MODE_NULL=0;
struct {int getMode(){return 1;}void softAPdisconnect(bool){++legacyStops;}void disconnect(bool){++legacyStops;}} WiFi;
struct {void end(){}} MDNS;
struct {int getFreeHeap(){return 100;}} ESP;
static void silentRestart(){++resets;}
static void silentRestartToReader(){++resets;}
static void stopDnsServer(){}
static void delay(int){}
struct Dns {void stop(){}};static Dns* dnsServer=nullptr;
struct Activity {void onExit(){}};
enum class WebServerActivityState {SHUTTING_DOWN};
struct Manager {bool goToAirDropReceive(const std::string&){++managerCalls;return allocate;}void returnFromAirDrop(const std::string&,bool){++returns;}} activityManager;
'''
with tempfile.TemporaryDirectory(prefix='airdrop-ui-')as td:
 for sticky in (0,1):
  for usb in (0,1):
   expected=5+usb+sticky
   cpp=prelude+enum+'\n'+strids+'enum class UIIcon{Wifi,Library,Hotspot,Transfer};\n'+arrays+'''
struct CrossPointWebServerActivity:Activity{
 bool airDropHandoff=false,isApMode=false;std::string returnBookPath;
 WebServerActivityState state{};void stopWebServer(){};
 bool handoffAirDrop(NetworkMode mode);void onExit();
};
'''+method('handoffAirDrop')+'\n'+method('onExit')+f'''
int main(){{
 static_assert(static_cast<int>(NetworkMode::JOIN_NETWORK)==0);
 static_assert(static_cast<int>(NetworkMode::NEARBY_STATS_SYNC)==5);
 static_assert(static_cast<int>(NetworkMode::AIRDROP_RECEIVE)==6);
 static_assert(MENU_ITEM_COUNT=={expected});
 static_assert(sizeof(menuItems)/sizeof(menuItems[0])==MENU_ITEM_COUNT);
 static_assert(sizeof(menuDescs)/sizeof(menuDescs[0])==MENU_ITEM_COUNT);
 static_assert(sizeof(menuIcons)/sizeof(menuIcons[0])==MENU_ITEM_COUNT);
 for(int i=0;i<MENU_ITEM_COUNT;++i)assert(listIndexForMenuIndex(i)==i+(i>=NEARBY_SECTION_INDEX));
 CrossPointWebServerActivity a;
 for(int legacy=0;legacy<6;++legacy)assert(!a.handoffAirDrop(static_cast<NetworkMode>(legacy)));
 assert(managerCalls==0&&returns==0);
 assert(a.handoffAirDrop(NetworkMode::AIRDROP_RECEIVE));
 assert(managerCalls=={sticky});assert(returns=={1-sticky});
 a.onExit();assert(resets==0&&legacyStops==0);
 if({sticky}){{allocate=false;CrossPointWebServerActivity failed;assert(failed.handoffAirDrop(NetworkMode::AIRDROP_RECEIVE));
   failed.onExit();assert(resets==0&&legacyStops==0&&returns==1);allocate=true;}}
 CrossPointWebServerActivity legacy;legacy.onExit();assert(resets==1&&legacyStops==1);
 std::puts("PASS production menu/dispatch/zero-reset native exit and preserved legacy exit");
}}
'''
   src=Path(td)/'test.cpp';src.write_text(cpp);exe=Path(td)/'test'
   subprocess.run(['g++','-std=c++20',f'-DFREEINK_DEVICE_STICKY={sticky}',f'-DCROSSINK_APP_CAP_AIRDROP={sticky}',f'-DCROSSINK_APP_CAP_USB_DRIVE={usb}',str(src),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
 print(json.dumps({'profiles':4,'exit':0,'scope':'production menu tables and native web handoff/exit, not RF/device proof'}))
