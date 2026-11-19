#!/usr/bin/env python3
"""Dependency-free product contracts; run directly or through CTest."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


def profile(name):
    config = configparser.ConfigParser(interpolation=None, strict=False)
    config.read(ROOT / 'platformio.ini')
    def resolve(section):
        result = {}
        for parent in config.get(section, 'extends', fallback='').split(','):
            if parent.strip():
                result.update(resolve(parent.strip()))
        result.update(dict(config[section]))
        return result
    return config, resolve('env:' + name)


import round1_regressions


class Product(round1_regressions.Round1):
    def test_normal_build_uses_owner_store_and_keeps_safe_paths(self):
        import subprocess, tempfile
        with tempfile.TemporaryDirectory(prefix='crosscub-store-') as tmp:
            source = Path(tmp) / 'store.cpp'
            source.write_text('''#include "src/network/AppStoreSource.h"
#include <cassert>
#include <cstring>
int main() {
  assert(!strcmp(AppStoreSource::baseUrl, "https://raw.githubusercontent.com/tobiasweissert/Crosscub/integration/sticky-test-all/apps/"));
  assert(AppStoreSource::safePackagePath("browser/main.lua"));
  for (auto s : {"../escape", "/absolute", "https://evil", "a//b", "a/%2e", "a/.."})
    assert(!AppStoreSource::safePackagePath(s));
}
'''.replace('#include <cassert>', '#include <cassert>\n#include <initializer_list>'))
            subprocess.run(['g++', '-std=c++17', '-I'+str(ROOT), str(source), '-o', tmp+'/store'], check=True)
            subprocess.run([tmp+'/store'], check=True)

    def test_ota_asset_cannot_select_legacy_firmware(self):
        import re, subprocess, tempfile
        ota = (ROOT / 'src/network/OtaUpdater.cpp').read_text()
        stem = re.search(r'constexpr char firmwareAssetStem\[\].*?;', ota).group()
        name = re.search(r'constexpr char firmwareAssetName\[\].*?;', ota).group()
        functions = '\n'.join(re.search(r'bool '+fn+r'\([^\n]*\) \{.*?\n\}', ota, re.S).group()
                              for fn in ('startsWith', 'endsWith', 'isMatchingFirmwareAssetName'))
        with tempfile.TemporaryDirectory(prefix='crosscub-ota-') as tmp:
            src = Path(tmp) / 'ota.cpp'
            src.write_text('#include <cstring>\n#include <cassert>\n#define CROSSSMUDGE_FIRMWARE_DEVICE_TYPE "sticky"\n'
                           + stem + name + '\nconstexpr char binSuffix[] = ".bin";\n' + functions
                           + '\nint main(){assert(!isMatchingFirmwareAssetName("firmware-sticky.bin"));'
                           + 'assert(!isMatchingFirmwareAssetName("firmware-sticky-v9.0.bin"));'
                           + 'assert(isMatchingFirmwareAssetName("crosscub-sticky-v2.0.bin"));}\n')
            subprocess.run(['g++', str(src), '-o', tmp+'/ota'], check=True)
            subprocess.run([tmp+'/ota'], check=True)

    def test_default_advertised_name_is_crosscub_without_changing_saved_names(self):
        import re, subprocess, tempfile
        source = (ROOT / 'src/CrossPointSettings.cpp').read_text()
        body = re.search(r'const char\* CrossPointSettings::getDefaultDeviceName\(\) \{.*?\n\}', source, re.S).group()
        with tempfile.TemporaryDirectory(prefix='crosscub-name-') as tmp:
            cpp = Path(tmp) / 'name.cpp'
            cpp.write_text('#include <cstring>\n#include <cassert>\nstruct BoardConfig{static bool isSticky(){return true;}static bool isX4Pro(){return false;}};struct GPIO{bool deviceIsX3(){return false;}bool deviceIsX4(){return false;}}gpio;struct CrossPointSettings{static const char* getDefaultDeviceName();};\n' + body + '\nint main(){assert(!strcmp(CrossPointSettings::getDefaultDeviceName(),"Crosscub Sticky"));}')
            subprocess.run(['g++', str(cpp), '-o', tmp+'/name'], check=True)
            subprocess.run([tmp+'/name'], check=True)

    def test_ota_policy(self):
        import subprocess, tempfile
        with tempfile.TemporaryDirectory(prefix='crosscub-policy-') as tmp:
            subprocess.run(['g++', '-std=c++17', '-I'+str(ROOT), str(ROOT/'test/crosscub/ota_policy.cpp'), '-o', tmp+'/policy'], check=True)
            subprocess.run([tmp+'/policy'], check=True)

    def test_firmware_rejects_non_sticky_profile(self):
        import subprocess, tempfile
        with tempfile.TemporaryDirectory(prefix='crosscub-board-') as tmp:
            stub = Path(tmp) / 'BoardConfig.h'
            stub.write_text('#define FREEINK_CAP_TOUCH 1\n#define FREEINK_CAP_USB_MSC 0\n')
            cpp = Path(tmp) / 'board.cpp'
            cpp.write_text('#include "AppCapabilities.h"\nint main(){}\n')
            cmd = ['g++', '-fsyntax-only', '-I'+tmp, '-I'+str(ROOT/'include'),
                   '-DARDUINO_ARCH_ESP32=1', '-DCROSSSMUDGE_APP_CAP_TOUCH=1',
                   '-DCROSSINK_APP_CAP_USB_DRIVE=0', str(cpp)]
            valid = subprocess.run(cmd+['-DFREEINK_DEVICE_STICKY=1'], capture_output=True)
            self.assertEqual(valid.returncode, 0, valid.stderr.decode())
            for flags in [['-DFREEINK_DEVICE_STICKY=0'], ['-DFREEINK_DEVICE_X4PRO=1'],
                          ['-DFREEINK_DEVICE_STICKY=1', '-DFREEINK_DEVICE_X4PRO=1']]:
                invalid = subprocess.run(cmd+flags, capture_output=True)
                self.assertNotEqual(invalid.returncode, 0, str(flags)+' accepted a non-Sticky firmware')

    def test_default_build_is_sticky_only(self):
        config, standard = profile('sticky')
        self.assertEqual(config['platformio']['default_envs'], 'sticky')
        for section in config.sections():
            if section.startswith('env:'):
                _, values = profile(section[4:])
                if 'simulator' not in section:
                    self.assertEqual(values['board'], 'esp32-s3-devkitc1-n16r8', section)
                    self.assertEqual(values['custom_firmware_device_type'], 'sticky', section)
        self.assertIn('-DFREEINK_DEVICE_STICKY=1', standard['build_flags'])
        self.assertIn('-DFREEINK_FB_PSRAM=1', standard['build_flags'])
        self.assertEqual(standard['board_build.flash_mode'], 'dio')
        self.assertEqual(standard['board_build.flash_size'], '16MB')
        self.assertEqual(standard['board_build.partitions'], 'partitions.csv')


if __name__ == '__main__':
    unittest.main()
