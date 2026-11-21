#!/usr/bin/env python3
"""Compile the complete production class declaration and real ctor/dtor bodies.

Only external SDK/UI types and the nonreturning render loop are host seams.
Debug destruction must still abort; NDEBUG permits observing derived/base cleanup.
This is not a scheduler, complete translation-unit, target, or device test.
"""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import os
import resource
import signal
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def definition(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    header_path = ROOT / 'src/activities/ActivityManager.h'
    impl_path = ROOT / 'src/activities/ActivityManager.cpp'
    header, impl = header_path.read_text(), impl_path.read_text()
    declaration = header[header.index('class ActivityManager {'):header.index('\nextern ActivityManager')]
    enums = header[header.index('enum class RequestUpdateResult'):header.index('\n/**')]
    constructor = definition(impl, 'ActivityManager::ActivityManager(')
    destructor = definition(impl, 'ActivityManager::~ActivityManager()')
    preamble = r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
struct GfxRenderer {};
struct MappedInputManager {};
struct RenderLock {};
struct ReaderActivity {};
struct portMUX_TYPE {};
#define portMUX_INITIALIZER_UNLOCKED {}
using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
static int mutexToken;
SemaphoreHandle_t xSemaphoreCreateMutex() { return &mutexToken; }
struct CrossPointSettings { enum SHORT_PWRBTN { None }; };
enum class SettingsSection { None };
enum class QuickLockTrigger { None };
enum class RemoteButton { None };
struct ScreenshotInfo {};
struct EpdFontFamily { enum Style { REGULAR }; };
struct HalDisplay { enum RefreshMode { FAST_REFRESH }; };
static int activityDestroyed = 0;
class Activity { public: virtual ~Activity() { ++activityDestroyed; } };
'''
    checks = r'''
static_assert(std::is_polymorphic_v<ActivityManager>, "preserve virtual rendering contract");
static_assert(std::has_virtual_destructor_v<ActivityManager>, "ActivityManager needs virtual destruction");
void ActivityManager::renderTaskLoop() { std::abort(); }
static int derivedDestroyed = 0;
static int resourceDestroyed = 0;
struct Resource { ~Resource() { ++resourceDestroyed; } };
struct DerivedManager : ActivityManager {
  std::unique_ptr<Resource> owned = std::make_unique<Resource>();
  DerivedManager(GfxRenderer& r, MappedInputManager& i) : ActivityManager(r, i) {
    currentActivity = std::make_unique<Activity>();
    pendingActivity = std::make_unique<Activity>();
    stackActivities.push_back(std::make_unique<Activity>());
  }
  ~DerivedManager() { ++derivedDestroyed; }
};
#define CHECK(condition) do { if (!(condition)) std::abort(); } while (false)
int main() {
  GfxRenderer renderer;
  MappedInputManager input;
#ifdef NDEBUG
  // The firmware deliberately forbids teardown in debug. Only NDEBUG allows
  // observation of virtual dispatch and destruction of real class members.
  std::unique_ptr<ActivityManager> manager = std::make_unique<DerivedManager>(renderer, input);
  manager.reset();
  CHECK(derivedDestroyed == 1);
  CHECK(resourceDestroyed == 1);
  CHECK(activityDestroyed == 3);
#else
  ActivityManager manager(renderer, input);
#endif
}
'''
    source = output / 'probe.cpp'
    source.write_text(preamble + enums + declaration + constructor + '\n' + destructor + checks)
    bindings = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in [header_path, impl_path, Path(__file__)]}
    (output / 'inputs.json').write_text(json.dumps(bindings, indent=2) + '\n')
    rows = []
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    with open('/root/.hermes/cache/scratch/crosscub-review-probes.lock', 'w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        for release in [True, False]:
            binary = output / ('release' if release else 'debug')
            command = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', str(source), '-o', str(binary)]
            if release:
                command += ['-DNDEBUG']
            if args.sanitize:
                command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
            (output / (binary.name + '-compile.log')).write_text(compiled.stdout + compiled.stderr)
            rows.append({'command': command, 'exit': compiled.returncode})
            (output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
            if compiled.returncode:
                print(compiled.stderr)
                return compiled.returncode
            run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15,
                                 env=env, preexec_fn=no_core)
            (output / (binary.name + '-run.log')).write_text(run.stdout + run.stderr)
            expected = 0 if release else -signal.SIGABRT
            rows.append({'command': [str(binary)], 'exit': run.returncode, 'expected': expected})
            (output / 'results.json').write_text(json.dumps(rows, indent=2) + '\n')
            if run.returncode != expected:
                raise AssertionError(rows[-1])
            if not release:
                assert 'assert' in run.stderr.lower() and 'false' in run.stderr
        print('PASS: virtual base ownership cleanup (3 activities + derived resource); debug assert preserved')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
