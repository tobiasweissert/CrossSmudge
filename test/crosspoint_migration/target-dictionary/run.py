#!/usr/bin/env python3
"""Bounded host compatibility tests; no SDK installation or target build.
Run: python3 test/crosspoint_migration/target-dictionary/run.py EVIDENCE_DIR
Uses real production headers/source, SDK declarations, and isolated storage doubles.
"""
from pathlib import Path
import hashlib, json, os, subprocess, sys
ROOT = Path(__file__).resolve().parents[3]
OUT = Path(sys.argv[1]).resolve()
OUT.mkdir(parents=True, exist_ok=True)
declaration_gate = subprocess.run([sys.executable, str(Path(__file__).with_name('declarations.py')), str(OUT)])
if declaration_gate.returncode:
    sys.exit(declaration_gate.returncode)
BUILD = Path('/root/.hermes/cache/scratch/target-dictionary-host')
BUILD.mkdir(parents=True, exist_ok=True)
STUB = BUILD / 'stubs'
STUB.mkdir(exist_ok=True)
def put(name, text):
    p = STUB / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)
def block(source, signature):
    start = source.index(signature)
    begin = source.index('{', start)
    depth, end = 1, begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
# Reuse the existing isolated file implementation, not production API replacements.
storage = (ROOT/'test/sd_font_registry/stubs/HalStorage.h').read_text()
storage = storage.replace('  int read(void* out, size_t size) {', '  int read() { uint8_t c; return read(&c, 1) == 1 ? c : -1; }\n  size_t write(uint8_t c) { return write(&c, 1); }\n  int read(void* out, size_t size) {')
put('HalStorage.h', storage)
put('Logging.h', '#pragma once\n#define LOG_DBG(...) ((void)0)\n#define LOG_INF(...) ((void)0)\n#define LOG_ERR(...) ((void)0)\n')
put('FsHelpers.h', '#pragma once\n#include <HalStorage.h>\nnamespace FsHelpers { inline bool resolveRootDirectoryIgnoreCase(const char* p,char* out,size_t n) { if(!Storage.exists(p)) return false; snprintf(out,n,"%s",p);return true;} }\n')
put('HalClock.h', '#pragma once\n#include <ctime>\nstruct Clock {bool localTime(tm&) {return false;}};inline Clock halClock;\n')
put('HalMemory.h', '#pragma once\n#include <cstddef>\nstruct HalMemory {struct Heap {size_t largestBlockBytes=1000000,freeBytes=1000000;};static Heap getDefaultHeap(){return {};}};\n')
put('esp_random.h', '#pragma once\n#include <cstdint>\ninline uint32_t esp_random(){static uint32_t n=0;return ++n;}\n')
put('common/FsApiConstants.h', '#pragma once\n')
put('freertos/FreeRTOS.h', '#pragma once\n')
put('freertos/task.h', '#pragma once\n')
# A symlink keeps the actual production activity declaration intact while substituting
# only the framework shell in this explicitly host-only fixture.
activity = STUB/'activities/reader/DictionaryDefinitionActivity.h'
activity.parent.mkdir(parents=True, exist_ok=True)
if not activity.exists(): activity.symlink_to(ROOT/'src/activities/reader/DictionaryDefinitionActivity.h')
put('activities/Activity.h', '''#pragma once
#include <string>
struct GfxRenderer {int getScreenWidth() const {return 80;}}; struct MappedInputManager {}; struct RenderLock {};
class Activity {protected: GfxRenderer& renderer; public: Activity(const char*,GfxRenderer& r,MappedInputManager&):renderer(r){} virtual ~Activity()=default;
virtual void onEnter(){} virtual void onExit(){} virtual void loop(){} virtual void render(RenderLock&&){} virtual bool skipLoopDelay(){return false;}};
''')
put('util/DictionaryLookupController.h', '''#pragma once
#include "util/Dictionary.h"
struct DictionaryLookupController {template<class... T> DictionaryLookupController(T&&...){} void setLookupToastEnabled(bool){} bool skipLoopDelay(){return false;}};
struct DictionaryClippingRequest {};
''')
put('util/LookupHistory.h', '#pragma once\nstruct LookupHistory {enum class Status {NotFound};};\n')
put('util/LookupChain.h', '#pragma once\nstruct LookupChain {struct Entry {};};\n')
put('util/WordSelectNavigator.h', '#pragma once\nstruct WordSelectNavigator {struct HighlightSnapshotStorage {};void setHighlightSnapshotStorage(HighlightSnapshotStorage*){}};\n')
for name in ['util/IpaUtils.h', 'util/DictLayout.h']:
    p = STUB/name
    if p.exists(): p.unlink()
source = (ROOT/'src/activities/reader/DictionaryDefinitionActivity.cpp').read_text()
# This is an exact production method-body fixture, not a second compatibility model.
method = block(source, 'void DictionaryDefinitionActivity::prepareDefinitionSource()') if 'void DictionaryDefinitionActivity::prepareDefinitionSource()' in source else ''
method += '\n' + '\n'.join(block(source, signature) for signature in ['void DictionaryDefinitionActivity::wrapPlain()', 'void DictionaryDefinitionActivity::collectLineSink('])
(BUILD/'activity-method.cpp').write_text('#include "activities/reader/DictionaryDefinitionActivity.h"\n#include "util/HtmlToPlainText.h"\n#include "util/TextPool.h"\n#include <Logging.h>\n#include <HalStorage.h>\nstatic bool definitionTextHasEtymologySpacingArtifact(const char*, bool){return false;}\n'+method+'\nvoid DictionaryDefinitionActivity::onEnter(){}\nvoid DictionaryDefinitionActivity::onExit(){}\nvoid DictionaryDefinitionActivity::loop(){}\nvoid DictionaryDefinitionActivity::render(RenderLock&&){}\nbool DictionaryDefinitionActivity::definitionTextNeedsApproximation(const char*) const {return false;}\nstd::string DictionaryDefinitionActivity::approximateDefinitionText(const char* p,bool) const {return p;}\nint DictionaryDefinitionActivity::measureWidthAdapter(void*,const char* p,EpdFontFamily::Style,bool){return std::char_traits<char>::length(p);}\n')
includes = [STUB, ROOT/'src', ROOT/'src/util', ROOT/'lib/DictHtmlRenderer', ROOT/'lib/EpdFont', ROOT/'lib/Utf8', ROOT/'lib/Epub', ROOT/'freeink-sdk/libs/book/FreeInkBook/third_party/expat', ROOT/'freeink-sdk/libs/ui/FreeInkUI/include', ROOT/'freeink-sdk/libs/font/FreeInkFont/include']
gtest = Path('/root/.hermes/cache/scratch/library-finish-deps/googletest-1.17.0/googletest')
cmd = ['g++','-std=c++20','-O0','-g0','-Wall','-Wextra','-Wno-unused-parameter']
if os.environ.get('SANITIZE'): cmd += ['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
cmd += [f'-I{x}' for x in includes+[gtest/'include',gtest]]
cmd += [str(ROOT/'test/crosspoint_migration/target-dictionary/test.cpp'),str(BUILD/'activity-method.cpp'),str(ROOT/'src/util/DictionaryRegistry.cpp'),str(ROOT/'src/vocabulary/VocabularyStore.cpp'),str(ROOT/'src/util/HtmlToPlainText.cpp'),str(ROOT/'src/util/DictLayout.cpp'),str(ROOT/'lib/Utf8/Utf8.cpp'),str(ROOT/'lib/Epub/Epub/htmlEntities.cpp'),str(gtest/'src/gtest-all.cc'),str(gtest/'src/gtest_main.cc'),'-pthread','-o',str(BUILD/'tests')]
(OUT/'command.json').write_text(json.dumps(cmd,indent=2))
p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
(OUT/'compile.log').write_text(p.stdout)
results: dict = {'compile_exit':p.returncode}
if p.returncode == 0:
    binary = BUILD/'tests'
    results['binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
    p = subprocess.run([str(binary),f'--gtest_output=xml:{OUT}/results.xml'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    (OUT/'tests.log').write_text(p.stdout)
    results['tests_exit'] = p.returncode
paths = ['src/util/DictionaryRegistry.h','src/util/DictionaryRegistry.cpp','src/activities/reader/DictionaryDefinitionActivity.h','src/activities/reader/DictionaryDefinitionActivity.cpp','src/vocabulary/VocabularyStore.h','src/vocabulary/VocabularyStore.cpp','test/crosspoint_migration/target-dictionary/run.py','test/crosspoint_migration/target-dictionary/test.cpp','freeink-sdk/libs/ui/FreeInkUI/include/FreeInkUI.h']
results['inputs'] = {s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in paths if (ROOT/s).exists()}
results['dependencies'] = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in
    [ROOT/'src/util/HtmlToPlainText.cpp', ROOT/'src/util/DictLayout.cpp', ROOT/'src/util/IpaUtils.h',
     ROOT/'src/util/TextPool.h', ROOT/'lib/Utf8/Utf8.cpp', ROOT/'lib/Epub/Epub/htmlEntities.cpp',
     Path(__file__).with_name('declarations.py'), Path(__file__).with_name('declarations.cpp'),
     BUILD/'activity-method.cpp', *[p for p in STUB.rglob('*') if p.is_file()]]}
(OUT/'receipt.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
print(p.stdout[-6000:])
sys.exit(p.returncode)
