#!/usr/bin/env python3
"""Offline production-code sound regression entrypoint; no packages or RF."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory(prefix="findmy-sound-", dir=os.environ.get("TMPDIR", "/root/.hermes/cache/scratch")) as tmp:
    for mode, flags in [("normal", []), ("ubsan", ["-fsanitize=undefined", "-fno-sanitize-recover=all"])]:
        binary = str(Path(tmp) / mode)
        subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic", "-fno-exceptions", "-fno-rtti", *flags,
                        "-I", str(ROOT / "lib/FindMySound"), str(Path(__file__).with_name("sound_test.cpp")), "-o", binary], check=True)
        subprocess.run([binary], check=True)
        print(mode + ": PASS", flush=True)
