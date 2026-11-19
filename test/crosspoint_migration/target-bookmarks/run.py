#!/usr/bin/env python3
"""Narrow real-header/codec qualification; no target link or physical UI claim."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess

R = Path(__file__).resolve().parents[3]
T = Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument('--evidence', type=Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
O = a.evidence
O.mkdir(parents=True, exist_ok=True)
B = Path('/root/.hermes/cache/scratch/target-bookmarks-build')
B.mkdir(exist_ok=True)
SDK = R/'freeink-sdk'
assert subprocess.check_output(['git', '-C', str(SDK), 'rev-parse', 'HEAD'], text=True).strip() == '634a3f8c067a3714a6a7c622015138af096ab5d2'
incs = [R/'src', SDK/'libs/assets/Icons/include', SDK/'libs/ui/FreeInkUI/include']
records = []
def run(name, cmd):
    r = subprocess.run(list(map(str, cmd)), capture_output=True, text=True, timeout=100)
    (O/(name+'.log')).write_text(r.stdout+r.stderr)
    records.append({'name': name, 'argv': list(map(str, cmd)), 'exit': r.returncode})
    (O/'exits.json').write_text(json.dumps(records, indent=2)+'\n')
    print(name, r.returncode, (r.stdout+r.stderr)[:1500], flush=True)
    return r.returncode
base = ['g++', '-std=c++20', '-O0', '-g0', '-fno-exceptions', '-fno-rtti']
if a.sanitize:
    base += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie']
def input_hashes():
    files = [R/p for p in [
        'src/BookmarkStore.h', 'src/BookmarkStore.cpp', 'src/BookmarkEntry.h',
        'src/util/BookmarkFile.h', 'src/util/BookmarkFile.cpp',
        'src/activities/reader/EpubReaderBookmarkListActivity.h',
        'src/activities/reader/EpubReaderBookmarkListActivity.cpp',
        'src/activities/reader/EpubReaderBookmarksActivity.cpp',
        'src/activities/ActivityResult.h', 'src/components/themes/BaseTheme.h',
        'src/util/ButtonNavigator.h', 'src/util/ButtonNavigator.cpp',
        'lib/Serialization/Serialization.h', 'lib/uzlib/src/uzlib_checksums.c',
        'lib/uzlib/src/uzlib.h',
        'test/crosspoint_migration/library-finish/ui_stubs/GfxRenderer.h']]
    files += [x for x in T.iterdir() if x.is_file()]
    files += list((SDK/'libs/ui/FreeInkUI/include').glob('*.h'))
    files += [SDK/'libs/ui/FreeInkUI/src/FreeInkUI.cpp', SDK/'libs/assets/Icons/include/Icon.h']
    files += list(Path('/root/github/crosssmudge/.pio/libdeps/sticky-simulator/ArduinoJson/src').rglob('*.hpp'))
    files += list(Path('/root/github/crosssmudge/.pio/libdeps/sticky-simulator/ArduinoJson/src').rglob('*.h'))
    return {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in files}

with open('/root/.hermes/cache/scratch/crosspoint-qualification.lock', 'a') as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    frozen = input_hashes()
    (O/'inputs.json').write_text(json.dumps(frozen, indent=2)+'\n')
    source = '''#include <FreeInkApp.h>
#include "components/themes/BaseTheme.h"
#include "BookmarkStore.h"
#include <type_traits>
static_assert(UIIcon::Bookmark == UIIcon::BookmarkIcon);
static_assert(std::is_same_v<decltype(BOOKMARKS.getBookmarks()), const std::vector<struct Bookmark>&>);
int main() { struct Bookmark entry{}; entry.spineIndex=7; entry.progress=.25f; return entry.spineIndex != 7; }
'''
    (O/'collision.cpp').write_text(source)
    rc = run('collision-compile', base+['-I'+str(i) for i in incs]+[O/'collision.cpp', '-o', B/'collision'])
    if rc: raise SystemExit(rc)
    assert run('collision-run', [B/'collision']) == 0
    # Real BookmarkFile.cpp / ArduinoJson; only disk transport is substituted.
    for name in ['HalStorage.h', 'PersistableStore.h', 'Logging.h']:
        (B/name).write_text((T/name).read_text())
    jsoninc = Path('/root/github/crosssmudge/.pio/libdeps/sticky-simulator/ArduinoJson/src')
    rc = run('json-compile', base+['-DARDUINOJSON_ENABLE_ARDUINO_STRING=0', '-I'+str(B), '-I'+str(jsoninc), '-I'+str(R/'src'), R/'src/util/BookmarkFile.cpp', T/'json.cpp', '-o', B/'json'])
    if rc: raise SystemExit(rc)
    assert run('json-run', [B/'json']) == 0
    # Compile the entire production store TU with the real colliding theme first.
    rc = run('binary-compile', base+['-I'+str(B)]+['-I'+str(i) for i in incs]+['-I'+str(R/'lib/Serialization'), '-I'+str(R/'lib/uzlib/src'), '-include', str(R/'src/components/themes/BaseTheme.h'), R/'src/BookmarkStore.cpp', R/'lib/uzlib/src/uzlib_checksums.c', T/'binary.cpp', '-o', B/'binary'])
    if rc: raise SystemExit(rc)
    assert run('binary-run', [B/'binary']) == 0
    def stripped(path):
        return '\n'.join(line for line in path.read_text().splitlines() if not line.startswith('#include') and line != '#pragma once')+'\n'
    ui = (T/'ui-boundaries.h').read_text()+stripped(R/'src/util/ButtonNavigator.h')
    ui += stripped(R/'src/activities/reader/EpubReaderBookmarkListActivity.h')
    ui += stripped(R/'src/activities/reader/EpubReaderBookmarkListActivity.cpp')
    (O/'list.cpp').write_text(ui)
    rc = run('list-syntax', base+['-I'+str(R/'test/crosspoint_migration/library-finish/ui_stubs')]+['-I'+str(i) for i in incs]+['-fsyntax-only', O/'list.cpp'])
    if rc: raise SystemExit(rc)
    runtime = ui.replace(' private:', ' public:') + (T/'ui-clock.h').read_text()
    runtime += stripped(R/'src/util/ButtonNavigator.cpp') + (T/'ui.cpp').read_text()
    (O/'list-runtime.cpp').write_text(runtime)
    rc = run('list-compile', base+['-I'+str(B), '-I'+str(R/'test/crosspoint_migration/library-finish/ui_stubs')]+['-I'+str(i) for i in incs]+['-I'+str(R/'lib/Serialization'), '-I'+str(R/'lib/uzlib/src'), '-include', str(R/'src/components/themes/BaseTheme.h'), O/'list-runtime.cpp', R/'src/BookmarkStore.cpp', R/'lib/uzlib/src/uzlib_checksums.c', SDK/'libs/ui/FreeInkUI/src/FreeInkUI.cpp', '-o', B/'list'])
    if rc: raise SystemExit(rc)
    assert run('list-run', [B/'list']) == 0
    legacy_source = (R/'src/activities/reader/EpubReaderBookmarksActivity.cpp').read_text()
    start = legacy_source.index('void EpubReaderBookmarksActivity::openSelectedBookmark()')
    method = legacy_source[start:legacy_source.index('\n}', start)+2]
    (O/'legacy.cpp').write_text((T/'legacy-boundary.h').read_text()+method+'\n'+(T/'legacy.cpp').read_text())
    rc = run('legacy-compile', base+['-I'+str(R/'src'), O/'legacy.cpp', '-o', B/'legacy'])
    if rc: raise SystemExit(rc)
    assert run('legacy-run', [B/'legacy']) == 0
    assert frozen == input_hashes(), 'qualification inputs changed during run'
    (O/'binaries.json').write_text(json.dumps({x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in B.iterdir() if x.name in ['collision','json','binary','list','legacy']}, indent=2)+'\n')
    (O/'summary.json').write_text(json.dumps({'all_passed': True, 'sanitize': a.sanitize, 'exits': records}, indent=2)+'\n')
    # Derived phase report, not GTest/native device XML.
    import xml.etree.ElementTree as ET
    suite = ET.Element('testsuite', name='target-bookmarks-host-phases', tests=str(len(records)), failures='0')
    for record in records:
        ET.SubElement(suite, 'testcase', name=record['name'], classname='target-bookmarks')
    ET.ElementTree(suite).write(O/'phases.xml', encoding='utf-8', xml_declaration=True)
