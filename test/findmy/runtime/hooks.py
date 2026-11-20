#!/usr/bin/env python3
"""Supplemental source guards, NOT native compilation or visual proof."""
from pathlib import Path
import unittest, tempfile, subprocess, os
ROOT=Path(__file__).resolve().parents[3]
class HookGuards(unittest.TestCase):
 def test_loop_services_beacon_before_quick_lock_or_other_early_returns(self):
  s=(ROOT/'src/main.cpp').read_text().split('void loop() {',1)[1]
  self.assertLess(s.index('crosscub::findmy::firmware::poll()'),s.index('return;'))
 def test_sleep_admission_precedes_activity_and_sd_teardown(self):
  s=(ROOT/'src/main.cpp').read_text().split('void enterDeepSleep(bool fromTimeout) {',1)[1]
  self.assertLess(s.index('firmware::standby()'),s.index('activityManager.goToSleep'))
  self.assertLess(s.index('firmware::beforeSleep()'),s.index('activityManager.goToSleep'))
 def test_tracking_failure_releases_lock_for_visible_error(self):
  s=(ROOT/'src/main.cpp').read_text().split('void loop() {',1)[1]
  self.assertIn('trackingLockObserved && !crosscub::findmy::firmware::standby()',s)
 def test_awdl_confirms_stop_before_wifi_release(self):
  s=(ROOT/'lib/hal/HalAirDropRadio.cpp').read_text()
  self.assertLess(s.index('firmware::beforeAirDrop()'),s.index('WiFi.mode(WIFI_OFF)'))
 def test_actual_main_timeout_condition_does_not_repeat_sleep_while_tracking(self):
  s=(ROOT/'src/main.cpp').read_text();i=s.index('buttonShortcutController.shouldQuickLockSleep(')
  condition=s[s.rfind('if (',0,i)+4:s.index('{',i)].strip()[:-1]
  code='''#include <cassert>\n#include "util/ButtonShortcutController.h"\nnamespace crosscub::findmy::firmware {bool standby(){return true;}}\nint main(){ButtonShortcutController buttonShortcutController;buttonShortcutController.toggleQuickLock(0,QuickLockTrigger::LongPower);unsigned long sleepTimeoutMs=1;auto millis=[](){return 1000U;};if ('''+condition+''') assert(false && "healthy tracking lock reentered sleep timeout");}\n'''
  with tempfile.TemporaryDirectory(dir=os.environ.get('TMPDIR','/root/.hermes/cache/scratch')) as d:
   p=Path(d);(p/'guard.cpp').write_text(code)
   subprocess.run(['g++','-std=c++17','-I'+str(ROOT/'src'),str(p/'guard.cpp'),'-o',str(p/'guard')],check=True)
   result=subprocess.run([str(p/'guard')],capture_output=True,text=True)
   self.assertEqual(result.returncode,0,result.stderr)
 def test_settings_has_owner_route(self):
  s=(ROOT/'src/activities/settings/SettingsActivity.cpp').read_text()
  self.assertIn('make_unique<FindMySettingsActivity>',s)
 def test_power_hal_defends_uncertain_ownership(self):
  s=(ROOT/'lib/hal/HalPowerManager.cpp').read_text().split('void HalPowerManager::startDeepSleep',1)[1]
  self.assertLess(s.index('firmware::beforeSleep()'),s.index('disableWiFiBeforeDeepSleep()'))
if __name__=='__main__':unittest.main()
