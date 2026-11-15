#!/usr/bin/env python3
"""Compile production presenter/model with native controller; no target/UI execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
source = ROOT / 'src/apps_local/sdapps/ModuleActivity.cpp'
code = source.read_text()
# Exact production slices: missing markers fail rather than silently substituting
# a test implementation. Device routing, renderer and navigation are stubs.
helpers = code[code.index('size_t boundedLength('):code.index('size_t documentBytes(')]
model = code[code.index('  struct Model {'):code.index('  HalMemory::PsramBuffer modelBytes;')]
present = code[code.index('int ModuleActivity::present('):code.index('void ModuleActivity::emit(')]
fixture = r'''
#include "AppAbi.h"
#include <cassert>
#include <cstring>
namespace fui { struct ListItem { const char* label=nullptr; int actionValue=0; }; }
struct RenderLock {};
class ModuleActivity {
 public:
 struct Runtime {
 MODEL
 Model* model=nullptr;
 struct Design { bool valid=false; }; Design* design=nullptr;
 struct Nav { void reset() {} } nav;
 unsigned topLine=0;bool bodyGesture=false;
 };
 Runtime* runtime;
 void closeRouting() {}
 void resetUi() {}
 void requestUpdate() {}
 int present(const CpView*);
};
HELPERS
PRESENT
static ModuleActivity::Runtime::Model model;
static ModuleActivity::Runtime runtime;
extern "C" int production_present(const CpView* view) {
 runtime.model=&model;
 ModuleActivity activity{&runtime};
 const int result=activity.present(view);
 assert(result==1);
 assert(model.count==view->row_count);
 for(unsigned i=0;i<model.count;++i) {
  assert(model.ids[i]==view->rows[i].id);
  assert(model.flags[i]==view->rows[i].flags);
  assert(model.items[i].label==model.labels[i]);
  assert(model.items[i].actionValue==static_cast<int>(i));
  assert(strcmp(model.labels[i],view->rows[i].label)==0);
 }
 return result;
}
'''.replace('MODEL', model).replace('HELPERS', helpers).replace('PRESENT', present)
generated = a.output / 'production_presenter.cpp'
generated.write_text(fixture)
test_source = ROOT / 'host-tests/sdapps/browser_test.cpp'
test = test_source.read_text()
test = test.replace('static int present(void*,const CpView* v){',
                    'extern "C" int production_present(const CpView*);\nstatic int present(void*,const CpView* v){\n assert(production_present(v));')
assert test != test_source.read_text()
controller_test = a.output / 'controller_presenter.cpp'
controller_test.write_text(test)
flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
         '-fno-omit-frame-pointer', '-no-pie', '-I'+str(ROOT/'src/apps_local/sdapps')]
obj = a.output / 'browser.o'
exe = a.output / 'controller-presenter'
commands = [
 ['cc', '-std=c11', *flags, '-Dapp_main=browser_entry', '-c', str(ROOT/'sd-apps/browser/app.c'), '-o', str(obj)],
 ['g++', '-std=c++20', *flags, str(generated), str(controller_test), str(obj), '-o', str(exe)],
 [str(exe)],
]
results = []
for command in commands:
 r = subprocess.run(command, capture_output=True, text=True, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0'})
 results.append({'command':command, 'exit':r.returncode, 'output':r.stdout+r.stderr})
 if r.returncode: break
inputs = [source, test_source, ROOT/'sd-apps/browser/app.c', ROOT/'sd-apps/common/Text.h', ROOT/'src/apps_local/sdapps/AppAbi.h', Path(__file__).resolve()]
(a.output/'result.json').write_text(json.dumps({'runs':results, 'inputs':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs},
 'limit':'Native production presenter/model slices and real C controller; routing/render/nav/lock stubs. No Xtensa execution.'}, indent=2)+'\n')
print('production controller/presenter', results[-1]['exit'])
raise SystemExit(results[-1]['exit'])
