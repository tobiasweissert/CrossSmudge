#!/usr/bin/env python3
"""Compile the complete production HalClock TU with host RTC/network doubles.
No real NTP, TLS, ESP-IDF, or device timing qualification.
"""
import os
import pathlib
import subprocess
import tempfile
ROOT = pathlib.Path(__file__).resolve().parents[3]
def main():
    with tempfile.TemporaryDirectory(prefix='sdk-ui-clock-', dir=os.environ['TMPDIR']) as td:
        d = pathlib.Path(td)
        headers = {
            'Arduino.h': '#pragma once\n#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n#include <cstddef>\n#include <ctime>\ninline unsigned long fakeMillis=100; inline unsigned long millis(){return fakeMillis;}\n',
            'Rtc.h': '#pragma once\n#include <cstdint>\nclass Rtc { public: struct DateTime { uint16_t year=2024; uint8_t month=1,day=1,hour=0,minute=0,second=0,weekday=1; }; static inline DateTime value{}; static inline bool readable=true; bool begin(){return true;} bool now(DateTime& d){d=value;return readable;} bool set(const DateTime& d){value=d;return true;} };\n',
            'Logging.h': '#pragma once\n#define LOG_INF(...) ((void)0)\n#define LOG_ERR(...) ((void)0)\n',
            'WiFi.h': '#pragma once\nconstexpr int WL_CONNECTED=3; struct Wifi {int status(){return WL_CONNECTED;}}; inline Wifi WiFi;\n',
            'esp_netif.h': '#pragma once\nusing esp_err_t=int; constexpr int ESP_OK=0; inline const char* esp_err_to_name(int){return "fixture";} inline int esp_netif_tcpip_exec(int(*f)(void*),void* p){return f(p);}\n#define pdMS_TO_TICKS(n) (n)\n',
            'esp_netif_sntp.h': '#pragma once\n#include "esp_netif.h"\nstruct esp_sntp_config_t{int value;};\n#define ESP_NETIF_SNTP_DEFAULT_CONFIG(host) esp_sntp_config_t{0}\ninline void esp_netif_sntp_deinit(){} inline int esp_netif_sntp_init(esp_sntp_config_t*){return ESP_OK;} inline int esp_netif_sntp_sync_wait(int){return ESP_OK;}\n',
            'lwip/dns.h': '#pragma once\ninline void dns_clear_cache(){}\n',
        }
        for p,s in headers.items():
            (d/p).parent.mkdir(parents=True, exist_ok=True)
            (d/p).write_text(s)
        # Out-of-class static initialization avoids compiler limitations for nested defaults.
        (d/'Rtc.h').write_text(headers['Rtc.h'].replace('static inline DateTime value{};', 'static DateTime value;')+'\ninline Rtc::DateTime Rtc::value{};\n')
        (d/'main.cpp').write_text(r'''
#include "HalClock.h"
#include <cassert>
#include <cstring>
int main(){
 halClock.begin();
 Rtc::value={2024,3,31,0,30,15,0};
 halClock.setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
 tm local{}; assert(halClock.localTime(local)); assert(local.tm_hour==1 && local.tm_min==30);
 char buf[64]; assert(halClock.formatTime(buf,sizeof(buf),false)); assert(std::strcmp(buf,"01:30")==0);
 Rtc::value.hour=1; fakeMillis+=11000;
 assert(halClock.localTime(local)); assert(local.tm_hour==3 && local.tm_min==30);
 uint8_t h,m; assert(halClock.getTime(h,m)); assert(h==1 && m==30);
 assert(halClock.formatTime(buf,sizeof(buf),static_cast<uint8_t>(48),false)); assert(std::strcmp(buf,"01:30")==0);
 halClock.setTimezone("UTC0");
 assert(halClock.formatTime(buf,sizeof(buf),true)); assert(std::strcmp(buf,"1:30 AM")==0);
 assert(!halClock.formatTime(buf,3,false));
 Rtc::value={2024,1,1,0,30,0,1}; fakeMillis+=11000;
 assert(halClock.formatDate(buf,sizeof(buf),44,HalClock::YEAR_MONTH_DAY_NUMERIC)); assert(std::strcmp(buf,"2023/12/31")==0);
 Rtc::readable=false; fakeMillis+=11000; assert(halClock.getTime(h,m));
 assert(!halClock.hasSynchronizedSystemTime()); assert(halClock.syncSystemTimeFromNTP()); assert(halClock.hasSynchronizedSystemTime());
}
''')
        command=['g++','-std=c++20','-Wall','-Wextra','-Werror','-fno-exceptions','-fno-rtti','-I'+td,'-I'+str(ROOT/'lib/hal'),str(ROOT/'lib/hal/HalClock.cpp'),str(d/'main.cpp'),'-o',str(d/'clock')]
        if os.environ.get('SANITIZE') == '1': command[1:1] = ['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        print('COMMAND', ' '.join(command), flush=True)
        subprocess.run(command,check=True)
        subprocess.run([str(d/'clock')],check=True)
        print('PASS: production HalClock UTC, DST jump, owner offset/date, bounds, cached RTC failure and system-time synchronization')
if __name__=='__main__': main()
