#!/usr/bin/env python3
"""One-shot historical release qualification, NOT routine CI.
Requires explicit --root and --evidence paths, immutable original round1 receipts,
and system liblua5.4. No downloads, Git writes, SDK or render qualification.
"""
import ctypes
import ctypes.util
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT: Path
EVIDENCE: Path
ALLOWED_SOURCE_DRIFT = set()
SOURCE_FREEZE_SCOPE = 'release'
HELPERS = {a: 'keyboard.lua' for a in ('browser', 'flashcards', 'habits', 'pomodoro', 'weather')}
HELPERS.update({a: 'keypad.lua' for a in ('wordle', 'sudoku', 'cp-calculator')})

def read(path):
    return json.loads(path.read_text())

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

class Packages(unittest.TestCase):
    def setUp(self):
        self.catalog = read(ROOT / 'apps/catalog.json')
        self.live = read(EVIDENCE / 'store-catalog.json')
        self.base = read(EVIDENCE / 'baseline-catalog.json')
        self.entries = {e['id']: e for e in self.catalog['apps']}

    def test_catalog_installs_every_local_helper(self):
        missing = [f'{a}/{h}' for a, h in HELPERS.items() if h not in self.entries[a]['files']]
        self.assertEqual(missing, [], 'new keyboard dependencies missing from Store install lists')

    def test_exact_41_ids_and_only_eight_record_changes(self):
        ids = [e['id'] for e in self.catalog['apps']]
        self.assertEqual(len(ids), 41)
        self.assertEqual(len(set(ids)), 41)
        self.assertEqual(ids, [e['id'] for e in self.live['apps']])
        self.assertEqual({k:v for k,v in self.catalog.items() if k != 'apps'},
                         {k:v for k,v in self.live.items() if k != 'apps'})
        changed = {a['id'] for a,b in zip(self.catalog['apps'], self.live['apps']) if a != b}
        self.assertEqual(changed, set(HELPERS))
        for old in self.live['apps']:
            if old['id'] not in HELPERS:
                self.assertEqual(self.entries[old['id']], old)

    def test_versions_and_minimal_schema_preserving_delta(self):
        for app, helper in HELPERS.items():
            with self.subTest(app=app):
                entry = self.entries[app]
                old = next(e for e in self.live['apps'] if e['id'] == app)
                base = next(e for e in self.base['apps'] if e['id'] == app)
                version = max(tuple(map(int, x['version'].split('.'))) for x in (old, base))
                expected = '.'.join(map(str, (*version[:2], version[2] + 1)))
                self.assertEqual(entry['version'], expected)
                self.assertEqual(entry['files'], sorted(old['files'] + [helper]))
                self.assertEqual({k:v for k,v in entry.items() if k not in ('files','version')},
                                 {k:v for k,v in old.items() if k not in ('files','version')})
                manifest = read(ROOT / 'apps' / app / 'manifest.json')
                baseline = read(EVIDENCE / 'baseline-manifests' / app / 'manifest.json')
                self.assertEqual(manifest, dict(baseline, version=expected))

    def test_declared_payload_exact_bytes_and_complete_baseline(self):
        rows = read(EVIDENCE / 'filehash-manifest.json')
        indexed = {r['path']: r for r in rows}
        expected = {f'apps/{app}/{f}' for app in HELPERS for f in self.entries[app]['files']}
        expected.add('apps/catalog.json')
        self.assertEqual(len(rows), len(indexed), 'duplicate hash-manifest paths')
        self.assertEqual(set(indexed), expected)
        payload = EVIDENCE / 'selected-payload'
        actual = {p.relative_to(payload).as_posix() for p in (payload/'apps').rglob('*') if p.is_file()}
        self.assertEqual(actual, expected | {'apps/catalog.json'})
        self.assertEqual((payload/'apps/catalog.json').read_bytes(), (ROOT/'apps/catalog.json').read_bytes())
        for path, row in indexed.items():
            p = payload/path
            self.assertFalse(p.is_symlink())
            self.assertEqual(p.stat().st_size, row['bytes'])
            self.assertEqual(digest(p), row['sha256'])
            source = EVIDENCE/row['source'] if row['source'].startswith('store-preserved/') else ROOT/row['source']
            self.assertEqual(p.read_bytes(), source.read_bytes())
        for app in HELPERS:
            old = next(e for e in self.live['apps'] if e['id'] == app)
            self.assertTrue(set(old['files']) <= set(self.entries[app]['files']))

    def test_complete_assets_licenses_and_newer_store_docs_preserved(self):
        store = {e['path']: e['sha'] for e in read(EVIDENCE/'store-tree.json')['tree'] if e['type'] == 'blob'}
        runtime_deltas = {'browser/editor.lua', 'flashcards/editor.lua', 'habits/editor.lua',
                          'pomodoro/editor.lua', 'weather/editor.lua', 'wordle/board.lua',
                          'wordle/cp_app.lua', 'sudoku/board.lua', 'cp-calculator/compact.lua',
                          'cp-calculator/sticky_enhanced.lua'}
        for app in HELPERS:
            old = next(e for e in self.live['apps'] if e['id'] == app)
            for f in old['files']:
                if f == 'manifest.json' or (f == 'CHANGELOG.md' and app in ('wordle','sudoku')) or f'{app}/{f}' in runtime_deltas:
                    continue
                path = f'apps/{app}/{f}'
                data = (EVIDENCE/'selected-payload'/path).read_bytes()
                blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
                self.assertEqual(blob, store[path], path)

    def test_lua_syntax_dofile_dependency_closure_and_old_firmware_helpers(self):
        # Real system Lua 5.4 execution, not a native firmware renderer qualification.
        library = ctypes.util.find_library('lua5.4')
        self.assertTrue(library, 'one-shot execution requires system liblua5.4')
        lib = ctypes.CDLL(library)
        lib.luaL_newstate.restype = ctypes.c_void_p
        lib.luaL_openlibs.argtypes = [ctypes.c_void_p]
        lib.luaL_loadstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        lib.lua_pcallk.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ssize_t, ctypes.c_void_p]
        lib.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
        lib.lua_tolstring.restype = ctypes.c_char_p
        lib.lua_close.argtypes = [ctypes.c_void_p]
        fixture = (ROOT/'test/keyboard_packaging/fallback.lua').read_text()
        for app, helper in HELPERS.items():
            with self.subTest(app=app):
                folder = EVIDENCE/'selected-payload/apps'/app
                files = self.entries[app]['files']
                script = 'local folder=' + json.dumps(str(folder)) + '\n'
                for filename in files:
                    if filename.endswith('.lua'):
                        source = (folder/filename).read_text()
                        for dep in re.findall(r'dofile\(\s*[\'\"]([^\'\"]+)[\'\"]\s*\)', source):
                            self.assertIn(dep, files, f'{app}/{filename}: undeclared dependency {dep}')
                        # All quoted Lua choices inside dynamic dispatch must be declared, too.
                        for line in source.splitlines():
                            if 'dofile(' in line:
                                for dep in re.findall(r'[\'\"]([^\'\"]+\.lua)[\'\"]', line):
                                    self.assertIn(dep, files)
                        script += 'assert(loadfile(folder.."/"..' + json.dumps(filename) + '))\n'
                script += 'local helper=' + json.dumps(helper) + '\n' + fixture
                state = lib.luaL_newstate()
                try:
                    lib.luaL_openlibs(state)
                    result = lib.luaL_loadstring(state, script.encode())
                    if result == 0:
                        result = lib.lua_pcallk(state, 0, 0, 0, 0, None)
                    error = lib.lua_tolstring(state, -1, None) if result else None
                    self.assertEqual(result, 0, error.decode() if error else '')
                finally:
                    lib.lua_close(state)

    def test_frozen_runtime_source_and_tests_unchanged(self):
        frozen = read(EVIDENCE/'FREEZE.json')
        frozen.update(read(EVIDENCE/'FREEZE-41.json'))
        # Other firmware/test writers may have accepted successors. Their old
        # receipts stay external; this release still pins every selected app.
        selected = {p for p in frozen if p.startswith(tuple(f'apps/{a}/' for a in HELPERS))}
        checked = set(frozen) if SOURCE_FREEZE_SCOPE == 'all' else selected
        self.assertLessEqual(ALLOWED_SOURCE_DRIFT, set(frozen), 'unknown source-drift exemption')
        changed = {p for p in checked if digest(ROOT/p) != frozen[p]}
        print('Source freeze scope:', SOURCE_FREEZE_SCOPE, 'checked:', len(checked),
              'changed:', sorted(changed), 'explicitly authorized:', sorted(ALLOWED_SOURCE_DRIFT), flush=True)
        self.assertEqual(sorted(changed - ALLOWED_SOURCE_DRIFT), [])

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--allow-source-drift', action='append', default=[], metavar='REPO_PATH',
                        help='Explicit parent-authorized source-only exception; never exempts payload bytes')
    parser.add_argument('--source-freeze-scope', choices=('release', 'all'), default='release',
                        help='release checks selected app sources; all additionally checks the historical firmware/test freeze')
    args = parser.parse_args()
    SOURCE_FREEZE_SCOPE = args.source_freeze_scope
    ALLOWED_SOURCE_DRIFT = set(args.allow_source_drift)
    ROOT = args.root.resolve()
    EVIDENCE = args.evidence.resolve()
    unittest.main(argv=[__file__], verbosity=2)
