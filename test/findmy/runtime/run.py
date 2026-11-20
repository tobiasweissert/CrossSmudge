#!/usr/bin/env python3
"""Compile and execute the actual portable tracking-standby admission policy."""
from pathlib import Path
import os
import sys
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]

class TrackingPolicyTest(unittest.TestCase):
    def test_standby_requires_explicit_opt_in_and_a_working_beacon(self):
        source = r'''
#include <cassert>
#if __has_include("TrackingPolicy.h")
#include "TrackingPolicy.h"
int main() {
  using namespace crosscub::findmy;
  TrackingPolicy policy;
  assert(!policy.shouldReplaceDeepSleep());
  policy.update(true, false, true, true);
  assert(!policy.shouldReplaceDeepSleep());
  policy.update(true, true, true, true);
  assert(policy.shouldReplaceDeepSleep());
  policy.update(false, true, true, true);
  assert(!policy.shouldReplaceDeepSleep());
}
#else
int main() { assert(false && "tracking standby admission not implemented"); }
#endif
'''
        with tempfile.TemporaryDirectory(prefix="findmy-runtime-", dir=os.environ.get("TMPDIR", "/root/.hermes/cache/scratch")) as directory:
            directory = Path(directory)
            cpp = directory / "check.cpp"
            binary = directory / "check"
            cpp.write_text(source)
            subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "lib/FindMyRuntime"), str(cpp), "-o", str(binary)], check=True)
            result = subprocess.run([str(binary)], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())

class ProductionIntegrationTest(unittest.TestCase):
    def test_actual_core_firmware_private_state_and_owner_ui_controls(self):
        subprocess.run([sys.executable, str(ROOT / 'test/findmy/runtime/check.py')], check=True)

    def test_supplemental_firmware_hook_guards(self):
        subprocess.run([sys.executable, str(ROOT / 'test/findmy/runtime/hooks.py')], check=True)

if __name__ == "__main__":
    unittest.main()
