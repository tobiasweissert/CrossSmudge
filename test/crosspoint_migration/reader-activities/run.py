#!/usr/bin/env python3
"""Bounded real stats-TU probes; hardware/filesystem doubles, not FAT qualification."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
ap = argparse.ArgumentParser()
ap.add_argument('--evidence', type=Path, required=True)
ap.add_argument('--sanitize', action='store_true')
ap.add_argument('--activities', action='store_true')
ap.add_argument('--txt', action='store_true')
ap.add_argument('--side', action='store_true')
ap.add_argument('--runtime', action='store_true')
ap.add_argument('--ble', action='store_true')
ap.add_argument('--codec', action='store_true')
args = ap.parse_args()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
stubs = out / 'stubs'
stubs.mkdir(exist_ok=True)
for name in ['HalStorage.h', 'Print.h', 'Logging.h', 'I18n.h']:
    text = (ROOT / 'test/book_reading_stats_atomic/stubs' / name).read_text()
    if name == 'HalStorage.h':
        text = text.replace('  void reset() {', '''  int writes = 0;
  bool failWrite = false;
  bool mkdir(const char*) { return true; }
  HalFile open(const char*) { return {}; }
  void reset() { writes = 0; failWrite = false;''')
        text = text.replace('    auto data = std::make_shared<HostFileData>();', '    if (failWrite) return false;\n    ++writes;\n    auto data = std::make_shared<HostFileData>();')
        text = text.replace('  size_t fileSize() const', '''  bool isDirectory() const { return false; }
  HalFile openNextFile() { return {}; }
  size_t getName(char*, size_t) { return 0; }
  size_t fileSize() const''')
    (stubs / name).write_text(text)
(stubs / 'CrossPointSettings.h').write_text('''#pragma once
#include <cstdint>
struct CrossPointSettings { bool trackReadingStats=true; uint8_t clockUtcOffsetQ=48;
bool shouldTrackReadingStats() const { return trackReadingStats; }
uint32_t getReadingIdleTimeThresholdSeconds() const { return 600; }
}; inline CrossPointSettings SETTINGS;
''')
(stubs / 'HalClock.h').write_text('''#pragma once
#include <cstdint>
struct Clock { bool getDateTime(uint16_t&,uint8_t&,uint8_t&,uint8_t&,uint8_t&) {return false;} };
inline Clock halClock;
''')
(stubs / 'esp_mac.h').write_text('#pragma once\n#include <cstdint>\ninline int esp_efuse_mac_get_default(uint8_t*) { return -1; }\n')
files = [ROOT / 'src/activities/reader' / name for name in ['BookReadingStats.cpp', 'GlobalReadingStats.cpp', 'BookStatsTracking.cpp', 'ReadingStatsUtils.cpp', 'ReaderSession.cpp']]
if args.txt or args.side or args.ble or args.codec:
    files = []
if args.runtime:
    files = [ROOT/'src/activities/reader/ReaderSession.cpp']
main = HERE/'stats.cpp'
if args.activities or args.txt or args.side or args.runtime or args.ble or args.codec:
    if args.codec:
        from settings_codec import emit
    elif args.ble:
        from ble_body import emit
    elif args.runtime:
        from runtime_body import emit
    elif args.side:
        from side_body import emit
    elif args.txt:
        from txt_body import emit
    else:
        from activity_bodies import emit
    try:
        emit(ROOT, out)
    except Exception as error:
        (out/'extract.log').write_text(str(error)+'\n')
        (out/'receipt.json').write_text(json.dumps({'steps':[{'name':'extract','returncode':1,'error':str(error)}]},indent=2))
        raise
    main = out/'activities.cpp'
cmd = ['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-fno-exceptions', '-fno-rtti', '-include', 'cstdio', '-I'+str(stubs), '-I'+str(ROOT/'src/activities/reader'), str(main), *map(str, files), '-o', str(out/'stats')]
if args.sanitize:
    cmd[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g', '-no-pie']
sources = [ROOT/'src/activities/reader'/name for name in [
    'BookReadingStats.cpp','BookReadingStats.h','GlobalReadingStats.cpp','GlobalReadingStats.h',
    'BookStatsTracking.cpp','BookStatsTracking.h','EpubReaderActivity.cpp','EpubReaderActivity.h',
    'XtcReaderActivity.cpp','XtcReaderActivity.h','TxtReaderActivity.cpp','TxtReaderActivity.h',
    'ReaderActivity.cpp','ReaderActivity.h','ReaderUtils.h','ReadingStatsUtils.cpp','ReadingStatsUtils.h',
    'ReaderSession.cpp','ReaderSession.h','ChapterBuildGate.h']]
inputs = {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest() for f in sources}
inputs.update({str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in HERE.glob('*') if f.is_file()})
receipts = []
for name, argv in [('compile', cmd), ('execute', [str(out/'stats')])]:
    p = subprocess.run(argv, capture_output=True, text=True, timeout=90)
    (out/(name+'.log')).write_text(p.stdout+p.stderr)
    receipts.append({'name':name, 'argv':argv, 'returncode':p.returncode})
    (out/'receipt.json').write_text(json.dumps({'sanitize': args.sanitize, 'inputs': inputs, 'generated_inputs': {str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in out.rglob('*') if f.is_file() and (f.suffix in ('.h','.cpp'))}, 'steps':receipts}, indent=2))
    print(name, p.returncode, p.stdout+p.stderr)
    if p.returncode: sys.exit(p.returncode)
