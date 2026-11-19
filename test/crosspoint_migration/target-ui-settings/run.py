#!/usr/bin/env python3
"""Bounded API/behavior probes. Real pinned SDK UI + settings/locale headers.
Extracted firmware methods are labeled; no target build or SDK initialization.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

R = Path(__file__).resolve().parents[3]
T = Path(__file__).resolve().parent
SDK = R / 'freeink-sdk'
GTEST = Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')

def function(text, anchor):
    start = text.index(anchor)
    end = text.index('{', start)
    depth = 1
    end += 1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    return text[start:end]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--evidence', type=Path, required=True)
    p.add_argument('--slice', choices=['network', 'airdrop', 'home', 'spacing', 'locale', 'constructor', 'all'], default='all')
    p.add_argument('--sanitize', action='store_true')
    a = p.parse_args()
    a.evidence.mkdir(parents=True, exist_ok=True)
    assert subprocess.check_output(['git', '-C', str(SDK), 'rev-parse', 'HEAD'], text=True).strip() == '634a3f8c067a3714a6a7c622015138af096ab5d2'
    records = []
    with tempfile.TemporaryDirectory(prefix='target-ui-settings-', dir=os.environ['TMPDIR']) as td:
        d = Path(td)
        # Executables must never shadow standard headers such as <locale>.
        binaries = d / 'bin'
        binaries.mkdir()
        (d/'Epub').mkdir()
        (d/'Epub/ReaderRenderSpec.h').write_text('#pragma once\nenum class EpubRenderMode {CrossSmudgeDefault};\nstruct ReaderRenderSpec {};\n')
        for stub in ['Arduino.h', 'Logging.h']:
            (d/stub).write_bytes((T.parent/'settings/stubs'/stub).read_bytes())
        (d/'HalStorage.h').write_text('#pragma once\nclass HalFile {};\n')
        # Renderer adapter is a peer-owned hardware boundary; UI types stay real.
        (d/'FreeInkUIGfxRenderer.h').write_text('#pragma once\nnamespace freeink::ui {class GfxRendererTarget {};}\n')
        (d/'BlePageTurner.h').write_text('#pragma once\nnamespace bleturner {struct Config {bool enabled=false;};}\n')
        (d/'BlePageTurnerJson.h').write_text('#pragma once\n#include <ArduinoJson.h>\n#include <BlePageTurner.h>\nnamespace bleturner {inline void writeJson(const Config& c,JsonDocument& d){d["bleEnabled"]=c.enabled;} inline bool readJson(Config& c,JsonVariantConst d){c.enabled=d["bleEnabled"] | false;return true;}}\n')
        spec = importlib.util.spec_from_file_location('gen_i18n', R/'scripts/gen_i18n.py')
        gen = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(gen)
        languages, names, codes, keys, _, _ = gen.load_translations(str(R/'lib/I18n/translations'), False)
        gen.generate_keys_header(languages, names, codes, keys, str(d/'I18nKeys.h'), False, {'EN'})
        common = '#include <gtest/gtest.h>\n#include <FreeInkApp.h>\n#include <FreeInkUIIcon.h>\n#include <I18n.h>\n#include <CrossPointSettings.h>\n'
        names = ['network', 'airdrop', 'home', 'spacing', 'locale', 'constructor'] if a.slice == 'all' else [a.slice]
        for name in names:
            source = common + (T/(name+'.inc')).read_text()
            helpers = (R/'src/components/UiAppHelpers.h').read_text()
            snapshot_visible = '#include "components/UiAppHelpers.h"' in (R/'src/activities/network/AirDropReceiveActivity.cpp').read_text()
            source = source.replace('// SNAPSHOT_BODY', function(helpers, 'inline freeink::ui::InputSnapshot touchSnapshotFrom(') if snapshot_visible else '')
            source = source.replace('// AIRDROP_ROUTE', function((R/'src/activities/network/AirDropReceiveActivity.cpp').read_text(), 'bool AirDropReceiveActivity::routeConsentInput()'))
            icon_visible = '#include "components/UiAppHelpers.h"' in (R/'src/activities/network/NetworkModeSelectionActivity.cpp').read_text()
            icon_enum = re.search(r'enum UIIcon \{.*?\};', (R/'src/components/themes/BaseTheme.h').read_text(), re.S).group()
            source = source.replace('// ICON_BODY', icon_enum + '\n' + function(helpers, 'inline freeink::ui::BitmapRef listIconFor(') if icon_visible else '')
            for target in ['LibraryActivity', 'LibrarySettingsActivity']:
                text = (R/f'src/activities/library/{target}.cpp').read_text()
                expr = re.search(r'props.rtl = (.*);', text).group(1)
                source = source.replace('// RTL_'+target, 'EXPECT_TRUE('+expr+');')
            # Alias is compiled as written, never extended with a fake template.
            header = (R/'src/activities/network/NetworkModeSelectionActivity.h').read_text()
            source = source.replace('// NETWORK_ALIAS', re.search(r'using UiHost = .*;', header).group())
            settings_header = (R/'src/activities/settings/SettingsActivity.h').read_text()
            constructors = re.findall(r'(?:explicit )?SettingsActivity\([^;]+;', settings_header)
            declarations = '#include "activities/settings/SettingsSection.h"\nclass GfxRenderer; class MappedInputManager;\nclass SettingsActivity {public: enum class View : uint8_t {Root, FileBrowser};\n' + '\n'.join(constructors) + '\n};\n'
            if name == 'constructor':
                # Entire real header body, only includes/lifecycle dependencies
                # replaced. Expose private state to observe constructor behavior.
                declarations = '#include "activities/settings/SettingsSection.h"\n' + '\n'.join(line for line in settings_header.splitlines() if not line.startswith('#include') and line != '#pragma once')
                declarations = declarations.replace(' private:', ' public:')
                source = source.replace('// SETTINGS_HEADER', declarations)
                cpp = (R/'src/activities/settings/SettingsActivity.cpp').read_text()
                start = cpp.index('SettingsActivity::SettingsActivity(')
                end = cpp.index('void SettingsActivity::rebuildSettingsLists()')
                source = source.replace('// SETTINGS_METHODS', cpp[start:end])
            else:
                source = source.replace('// SETTINGS_HEADER', declarations)
            (d/(name+'.cpp')).write_text(source)
            cmd = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-comment', '-fno-exceptions', '-fno-rtti', '-DARDUINOJSON_ENABLE_ARDUINO_STRING=0', '-I'+str(d), '-I'+str(R/'src'), '-I'+str(R/'lib/I18n'), '-I'+str(R/'lib/Serialization'), '-I/root/github/.host-data/crosssmudge/gallery-native-deps/sticky-simulator/ArduinoJson/src', '-isystem'+str(SDK/'libs/ui/FreeInkUI/include'), '-isystem'+str(SDK/'libs/assets/Icons/include'), '-I'+str(GTEST/'include'), '-I'+str(GTEST), str(d/(name+'.cpp')), str(GTEST/'src/gtest-all.cc'), str(GTEST/'src/gtest_main.cc'), '-pthread', '-o', str(binaries/name)]
            if name == 'constructor':
                cmd += [str(SDK/'libs/ui/FreeInkUI/src/FreeInkUI.cpp')]
            if a.sanitize: cmd[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
            (a.evidence/(name+'-compile.log')).write_text(result.stdout+result.stderr)
            record = {'name': name, 'command': cmd, 'compile_exit': result.returncode, 'probe_sha256': hashlib.sha256(source.encode()).hexdigest()}
            if result.returncode == 0:
                record['binary_sha256'] = hashlib.sha256((binaries/name).read_bytes()).hexdigest()
                argv = [str(binaries/name), '--gtest_output=xml:'+str(a.evidence/(name+'.xml'))]
                run = subprocess.run(argv, capture_output=True, text=True, timeout=30)
                (a.evidence/(name+'-run.log')).write_text(run.stdout+run.stderr)
                record.update(run_command=argv, run_exit=run.returncode)
            records.append(record)
            print(name, record.get('run_exit', result.returncode))
            if result.returncode: print(result.stderr[:3000])
        files = list(T.glob('*')) + [R/'src/HomeButtonSettings.h', R/'src/ReaderLineSpacing.h', R/'src/activities/settings/SettingsActivity.h', R/'src/activities/settings/SettingsActivity.cpp', R/'src/activities/network/NetworkModeSelectionActivity.h', R/'src/activities/network/NetworkModeSelectionActivity.cpp', R/'src/activities/network/AirDropReceiveActivity.cpp', R/'src/activities/library/LibraryActivity.cpp', R/'src/activities/library/LibrarySettingsActivity.cpp', R/'src/components/UiAppHelpers.h', R/'src/components/UiAppHost.h', R/'src/components/AirDropConsentInput.h', R/'src/CrossPointSettings.h', R/'src/util/HomeButtonInput.h', d/'I18nKeys.h']
        # Every compiler dependency is hash-bound, including transitive SDK headers.
        files += list((SDK/'libs/ui/FreeInkUI/include').rglob('*.h'))
        inputs = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in files if f.is_file()}
        (a.evidence/'receipt.json').write_text(json.dumps({'records': records, 'inputs': inputs, 'sdk': '634a3f8c067a3714a6a7c622015138af096ab5d2', 'boundary': 'Real SDK UI and generated production locale/settings types; extracted glue plus hardware/application doubles. Not whole target TUs or physical UI.'}, indent=2))
    return int(any(x.get('run_exit', x['compile_exit']) for x in records))

if __name__ == '__main__':
    raise SystemExit(main())
