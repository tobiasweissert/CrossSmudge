#!/usr/bin/env python3
"""Focused Sticky absence regression; real runtime and extracted settings consumer.

SDK headers are read-only. No SDK/BT implementation or network is needed.
"""
import argparse
import configparser
import fcntl
import os
import resource
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--sdk', type=Path, default=ROOT / 'freeink-sdk')
parser.add_argument('--case', choices=['config', 'runtime', 'menu', 'enabled', 'preserved', 'airdrop'])
args = parser.parse_args()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
lock = open(Path(os.environ.get('TMPDIR', '/root/.hermes/cache/scratch')) /
            'crosspoint-qualification.lock', 'a')
fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
os.environ.setdefault('UBSAN_OPTIONS', 'halt_on_error=1:print_stacktrace=1')


def native(source, files=(), defines=()):
    with tempfile.TemporaryDirectory(prefix='sticky-no-bluetooth-') as directory:
        cpp = Path(directory) / 'test.cpp'
        cpp.write_text(source)
        exe = Path(directory) / 'test'
        command = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'lib/BlePageTurner/include'),
                   '-I' + str(ROOT / 'lib/BlePageTurner/src'),
                   '-I' + str(args.sdk / 'libs/network/BleKeyboardHost/include')]
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
        command += list(defines) + [str(cpp)] + [str(ROOT / f) for f in files] + ['-o', str(exe)]
        subprocess.run(command, check=True)
        subprocess.run([str(exe)], check=True, timeout=30)


class StickyNoBluetooth(unittest.TestCase):
    def test_config(self):
        config = configparser.ConfigParser(interpolation=None, strict=False)
        config.read(ROOT / 'platformio.ini')
        def resolve(section, key):
            value = config.get(section, key, fallback='')
            return re.sub(r'\$\{([^}.]+)\.([^}]+)\}', lambda m: resolve(*m.groups()), value)
        for section in ['base', 'env:sticky', 'env:sticky-debug', 'env:sticky-simulator']:
            flags = resolve(section, 'build_flags')
            self.assertIn('-DFREEINK_CAP_BLE_HID_HOST=0', flags)
            self.assertNotIn('-DFREEINK_CAP_BLE_HID_HOST=1', flags)
            self.assertNotIn('nimble', flags.lower())
        deps = resolve('base', 'lib_deps')
        self.assertNotIn('BleKeyboardHost=', deps)
        self.assertNotIn('NimBLE', deps)
        for preserved in ['NearbyTransfer=', 'SecureNet=', 'FreeInkFont=']:
            self.assertIn(preserved, deps)
        sdk = resolve('env:sticky', 'custom_sdkconfig')
        self.assertIn('CONFIG_BT_ENABLED=n', sdk)
        self.assertNotIn('CONFIG_BT_ENABLED=y', sdk)
        self.assertNotIn('CONFIG_BT_NIMBLE', sdk)
        self.assertEqual(config['base']['board_upload.maximum_size'], '6553600')

    def test_runtime(self):
        native(r'''
#include <cassert>
#include <cstring>
#include "BlePageTurner.h"
#include "Runtime.h"
using namespace bleturner;
int calls = 0;
Heap heap() { ++calls; return {200000, 150000}; }
bool yes() { ++calls; return true; }
bool no() { ++calls; return false; }
bool deliver(Action) { ++calls; return true; }
void restart() { ++calls; }
void hold(bool) { ++calls; }
void log(bool, const char*, va_list) { ++calls; }
int main() {
  detail::resetForTests();
  Config saved;
  saved.enabled = 1;
  std::strcpy(saved.peerAddr, "01:02:03:04:05:06");
  std::strcpy(saved.peerName, "Saved remote");
  saved.prevKeyUsage = kUsageUp;
  saved.nextKeyUsage = kUsageDown;
  const Config original = saved;
  const Host host{heap, yes, deliver, yes, no, restart, hold, log, nullptr};
  begin(host, saved);
  assert(!status().compiledIn);
  assert(!switchOn());
  assert(!detail::startAsync());
  for (auto where : {Where::Home, Where::Reader, Where::PairingScreen, Where::Elsewhere}) {
    Scene scene{};
    scene.where = where;
    scene.visit = 1;
    scene.pageShown = true;
    requestConnect();
    for (int i = 0; i < 8; ++i) {
      scene.wifiOn = i % 2;
      scene.storageBusy = i % 3 == 0;
      assert(!tick(scene));
      acknowledgeLinkNote();
      assert(linkNote() == LinkNote::None);
      assert(!holdsHeap());
      assert(!status().readerDeferred);
    }
  }
  assert(!pair(saved.peerAddr, saved.peerName));
  assert(!forget(saved.peerAddr));
  assert(!service());
  scan(1000);
  Event event{};
  assert(!pollEvent(event));
  assert(bondCount() == 0 && foundCount() == 0);
  assert(beforeChapterBuild() == BuildRelease::NotHeld);
  afterPaint();
  assert(beforeScreenChange());
  assert(beforeSleep(100));
  assert(stopNow(100));
  assert(std::memcmp(&original, &saved, sizeof saved) == 0);
  // Disabled capability must not reclaim font caches, request WiFi yield, or deliver input.
  assert(calls == 0);
}
''', ['lib/BlePageTurner/src/Runtime.cpp', 'lib/BlePageTurner/src/NoRadio.cpp',
      'lib/BlePageTurner/src/SdkRadio.cpp'], ['-DBLETURNER_TESTING', '-DFREEINK_CAP_BLE_HID_HOST=0'])

    def test_enabled(self):
        native(r'''
#include <cassert>
#include <cstring>
#include "BlePageTurner.h"
#include "Fakes.h"
int main() {
  using namespace bleturner;
  fake::reset();
  Config saved;
  saved.enabled = 1;
  std::strcpy(saved.peerAddr, "11:22:33:44:55:66");
  begin(fake::hostFns(), saved);
  assert(status().compiledIn);
  assert(switchOn());
  assert(status().running && fake::radio().beginCalls == 1);
  assert(fake::host().releaseCalls == 1);
  auto scene = fake::reading();
  scene.storageBusy = true;
  tick(scene);
  assert(!status().running);
  assert(fake::radio().endCalls == 1);
  // The enabled port still supports explicit forgetting, unlike the disabled port.
  assert(forget(saved.peerAddr));
  assert(saved.peerAddr[0] == '\0');
  assert(fake::radio().forgotten.size() == 1);
}
''', ['lib/BlePageTurner/src/Runtime.cpp', 'lib/BlePageTurner/test/Fakes.cpp'],
            ['-DBLETURNER_TESTING', '-DFREEINK_CAP_BLE_HID_HOST=1',
             '-I' + str(ROOT / 'lib/BlePageTurner/test')])

    def test_preserved(self):
        # Positive preservation controls: compare actual bytes, not source-file sizes.
        base = '21948ff091c347911ffbc10798ec6039614720ad'
        paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', base,
            'lib/EpdFont', 'lib/GfxRenderer', 'lib/AirDrop', 'lib/StaticBuiltinData',
            'lib/I18n', 'src/CrossPointSettings.cpp', 'src/CrossPointSettings.h',
            'src/network', 'scripts/gen_static_builtin_data.py'], cwd=ROOT, text=True).splitlines()
        self.assertTrue(paths)
        for path in paths:
            original = subprocess.check_output(['git', 'show', base + ':' + path], cwd=ROOT)
            self.assertEqual((ROOT / path).read_bytes(), original, path)
        subprocess.run(['python3', str(ROOT / 'test/static_builtin_data/test_generator.py'),
                        'NumericArrays.test_all_76_canonical_font_arrays', '-v'], cwd=ROOT, check=True)

    def test_airdrop(self):
        # Positive controls for mandatory consent and retained WiFi/AWDL lifecycle.
        for runner in ['consent_run.py', 'awdl_run.py']:
            subprocess.run(['python3', str(ROOT / 'test/airdrop' / runner)] +
                           (['--sanitize'] if args.sanitize else []), cwd=ROOT, check=True)

    def test_menu(self):
        source = (ROOT / 'src/activities/settings/SettingsActivity.cpp').read_text()
        start = source.index('  systemDeviceSettings.push_back(SettingInfo::Action(StrId::STR_BT_PAGE_TURNER')
        end = source.index('  systemDeviceSettings.push_back(SettingInfo::Action(StrId::STR_ABOUT', start)
        # Include the immediately preceding capability guard when present.
        guard = source.rfind('#if ', 0, start)
        if guard >= 0 and source[guard:start].strip().startswith('#if ') and '\n' not in source[guard:start].strip():
            start = guard
        menu = source[start:end]
        start = source.index('      case SettingAction::Bluetooth: {')
        end = source.index('      case SettingAction::ClockSettings:', start)
        action = source[start:end]
        for enabled in [0, 1]:
            native('''
#include <cassert>
#include <memory>
#include <vector>
#include <utility>
enum class StrId { STR_BT_PAGE_TURNER };
enum class SettingAction { Bluetooth };
struct SettingInfo { static int Action(StrId, SettingAction) { return 1; } };
int created = 0;
struct BleSettingsActivity {};
template<class T> auto makeUniqueNoThrow(int, int) { ++created; return std::make_unique<T>(); }
void startActivityForResult(std::unique_ptr<BleSettingsActivity>, int) {}
void requestUpdate() {}
int main() {
  std::vector<int> systemDeviceSettings;
''' + menu + '''
  assert(systemDeviceSettings.size() == FREEINK_CAP_BLE_HID_HOST);
  int renderer = 0, mappedInput = 0, resultHandler = 0;
  (void)renderer; (void)mappedInput; (void)resultHandler;
  switch (SettingAction::Bluetooth) {
''' + action + '''
  }
  assert(created == FREEINK_CAP_BLE_HID_HOST);
}
''', defines=['-DFREEINK_CAP_BLE_HID_HOST=' + str(enabled)])


suite = unittest.TestSuite()
for name in ([args.case] if args.case else ['config', 'runtime', 'menu', 'enabled', 'preserved', 'airdrop']):
    suite.addTest(StickyNoBluetooth('test_' + name))
raise SystemExit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
