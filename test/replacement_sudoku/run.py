#!/usr/bin/env python3
"""Standalone exact-VM Sudoku tests. No firmware, SDK, or sibling PR required."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
parser = argparse.ArgumentParser()
parser.add_argument("--legacy-regression", action="store_true")
parser.add_argument("--output", type=Path)
parser.add_argument("--app-dir", type=Path, default=ROOT / "apps/sudoku",
                    help="Extracted candidate package to test instead of working tree app")
args = parser.parse_args()
for name, expected in {
    "SudokuCore.cpp": "4e73036f80ff1eb09dc6a5b5f2e60b15ee8b8d113141bdd1ee1ea4100acc3fa9",
    "SudokuCore.h": "1eddd0672c092afe05fb10b70fb534704e889981f8d3fc787cda65610f27f8ff",
}.items():
    assert hashlib.sha256((HERE / "oracle" / name).read_bytes()).hexdigest() == expected
scratch = Path(os.environ.get("TMPDIR", "/root/.hermes/cache/scratch"))
scratch.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix="sudoku-verify-", dir=scratch) as temp:
    work = Path(temp)
    runner = work / "lua-runner"
    subprocess.run(["cc", "-O2", "-I" + str(ROOT / "lib/Lua/src"),
                    str(HERE / "runner.c"),
                    *map(str, sorted((ROOT / "lib/Lua/src").glob("*.c"))),
                    "-lm", "-o", str(runner)], check=True)
    app = work / "app"
    shutil.copytree(args.app_dir, app)
    results = []
    if args.legacy_regression:
        tests = ["legacy_regression.lua"]
    else:
        oracle = work / "oracle"
        subprocess.run(["c++", "-O2", "-std=c++17", str(HERE / "oracle.cpp"),
                        str(HERE / "oracle/SudokuCore.cpp"), "-o", str(oracle)], check=True)
        with (app / "oracle_cases.lua").open("w") as output:
            subprocess.run([str(oracle)], stdout=output, check=True, timeout=60)
        tests = ["core.lua", "generator.lua", "journal.lua"]
    for name in tests:
        p = subprocess.run([str(runner), str(HERE / name), "2097152"], cwd=app,
                           capture_output=True, text=True, timeout=180)
        metrics = json.loads(p.stdout.strip().splitlines()[-1]) if p.stdout.strip() else {}
        results.append(dict(test=name, returncode=p.returncode, stdout=p.stdout,
                            stderr=p.stderr, metrics=metrics))
    if not args.legacy_regression:
        for width, height in [(480, 800), (800, 480)]:
            p = subprocess.run([str(runner), str(HERE / 'hint.lua'), '2097152',
                                str(width), str(height)], cwd=app,
                               capture_output=True, text=True, timeout=180)
            metrics = json.loads(p.stdout.strip().splitlines()[-1]) if p.stdout.strip() else {}
            results.append(dict(test='hint.lua', width=width, height=height,
                                returncode=p.returncode, stdout=p.stdout,
                                stderr=p.stderr, metrics=metrics))
            p = subprocess.run([str(runner), str(HERE / 'runtime.lua'), '2097152',
                                str(width), str(height)], cwd=app,
                               capture_output=True, text=True, timeout=180)
            metrics = json.loads(p.stdout.strip().splitlines()[-1]) if p.stdout.strip() else {}
            results.append(dict(test='runtime.lua', width=width, height=height,
                                returncode=p.returncode, stdout=p.stdout,
                                stderr=p.stderr, metrics=metrics))
            p = subprocess.run([str(runner), str(HERE / 'visual.lua'), '2097152',
                                str(width), str(height)], cwd=app,
                               capture_output=True, text=True, timeout=180)
            metrics = json.loads(p.stdout.strip().splitlines()[-1]) if p.stdout.strip() else {}
            results.append(dict(test='visual.lua', width=width, height=height,
                                returncode=p.returncode, stdout=p.stdout,
                                stderr=p.stderr, metrics=metrics))
            if p.returncode == 0 and args.output:
                shutil.copyfile(app/'adapter.svg', args.output.parent/f'sudoku-adapter-{width}x{height}.svg')
    capacity = None
    if not args.legacy_regression:
        p = subprocess.run([str(runner), str(HERE / 'guard.lua'), '76800'], cwd=app,
                           capture_output=True, text=True, timeout=30)
        metrics = json.loads(p.stdout.strip().splitlines()[-1]) if p.stdout.strip() else {}
        results.append(dict(test='guard.lua', returncode=p.returncode, stdout=p.stdout,
                            stderr=p.stderr, metrics=metrics))
        # Separate observation: do not hide C3 failure inside the passing S3 suite.
        (app / 'capacity.lua').write_text("local c=dofile('cp_core.lua'); c.generate(0,12648430,1)\n")
        p = subprocess.run([str(runner), 'capacity.lua', '76800'], cwd=app,
                           capture_output=True, text=True, timeout=180)
        capacity = dict(limit=76800, supported=p.returncode == 0,
                        stdout=p.stdout, stderr=p.stderr, returncode=p.returncode)
    payload = dict(adapter="native stock API adapter; not hardware", results=results,
                   c3_capacity_observation=capacity)
    if args.output:
        args.output.write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps(payload, indent=2))
    assert all(r["returncode"] == 0 and r["metrics"].get("after_close") == 0 for r in results)
