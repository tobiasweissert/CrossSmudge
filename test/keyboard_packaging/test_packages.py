#!/usr/bin/env python3
"""Routine, offline packaging contracts; no release snapshot or task-state reads.

Run: python3 -B -m unittest discover -s test/keyboard_packaging -v
Pure Python membership checks always run. System Lua 5.4 is an explicit optional
syntax/fallback dependency, not the firmware SDK or a render qualification.
"""
import ctypes
import ctypes.util
import json
from pathlib import Path, PurePosixPath
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
HELPERS = {a: 'keyboard.lua' for a in ('browser', 'flashcards', 'habits', 'pomodoro', 'weather')}
HELPERS.update({a: 'keypad.lua' for a in ('wordle', 'sudoku', 'cp-calculator')})


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def validate_catalog(catalog):
    assert isinstance(catalog, dict) and isinstance(catalog.get('apps'), list), 'catalog apps must be a list'
    ids = [e['id'] for e in catalog['apps']]
    assert len(ids) == len(set(ids)), 'duplicate catalog id'
    return {e['id']: e for e in catalog['apps']}


def safe_relative(name):
    assert isinstance(name, str) and name, 'empty/nonstring package path'
    path = PurePosixPath(name)
    assert not path.is_absolute() and '..' not in path.parts and '\\' not in name, 'unsafe package path'
    assert path.as_posix() == name and name != '.', 'noncanonical package path'
    return name


def validate_manifest(entry, manifest):
    # Store copy can intentionally differ from the app's shorter description.
    assert isinstance(manifest.get('description'), str) and manifest['description'], 'missing description'
    for field in ('id', 'name', 'version', 'author'):
        assert manifest[field] == entry[field], f'{entry["id"]}: manifest/catalog {field} mismatch'
    for field in ('min_api', 'requires_psram'):
        # Either metadata surface may omit optional eligibility flags.
        if field in manifest and field in entry:
            assert manifest[field] == entry[field], f'{entry["id"]}: manifest/catalog {field} mismatch'
    assert re.fullmatch(r'\d+\.\d+\.\d+', manifest['version']), 'invalid package version'
    assert manifest['entry'] in entry['files'], 'manifest entry not packaged'
    assert 'manifest.json' in entry['files'], 'manifest not packaged'


def dofile_dependencies(source):
    # Literal calls plus quoted choices in the current one-line dispatchers.
    # This is a static membership oracle, not a general Lua expression evaluator.
    deps = set(re.findall(r'\bdofile\s*\(\s*[\'\"]([^\'\"]+)[\'\"]\s*\)', source))
    for line in source.splitlines():
        if re.search(r'\bdofile\s*\(', line):
            deps.update(re.findall(r'[\'\"]([^\'\"]+\.lua)[\'\"]', line))
    return deps


def validate_packages(root, helpers=HELPERS):
    entries = validate_catalog(read(root / 'apps/catalog.json'))
    for app, helper in helpers.items():
        safe_relative(app)
        assert app in entries, f'{app}: missing catalog record'
        entry = entries[app]
        files = entry['files']
        assert isinstance(files, list) and files, f'{app}: files must be a nonempty list'
        assert len(files) == len(set(files)), f'{app}: duplicate package file'
        assert helper in files, f'{app}: missing helper {helper}'
        folder = root / 'apps' / app
        for name in files:
            safe_relative(name)
            path = folder / name
            assert not path.is_symlink() and path.is_file(), f'{app}: missing/nonregular payload {name}'
            assert folder.resolve() in path.resolve().parents, 'payload escapes app folder'
            if name.endswith('.lua'):
                missing = dofile_dependencies(path.read_text(encoding='utf-8')) - set(files)
                assert not missing, f'{app}/{name}: undeclared dofile dependency {sorted(missing)}'
        validate_manifest(entry, read(folder / 'manifest.json'))
    return entries


class LuaUnavailable(RuntimeError):
    pass


def execute_lua(script):
    library = ctypes.util.find_library('lua5.4')
    if not library:
        raise LuaUnavailable('system liblua5.4 is required for Lua syntax/fallback execution; pure Python membership remains available')
    try:
        lib = ctypes.CDLL(library)
    except OSError as error:
        raise LuaUnavailable(f'cannot load system liblua5.4: {error}') from error
    lib.luaL_newstate.restype = ctypes.c_void_p
    lib.luaL_openlibs.argtypes = [ctypes.c_void_p]
    lib.luaL_loadstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    lib.lua_pcallk.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_ssize_t, ctypes.c_void_p]
    lib.lua_tolstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    lib.lua_tolstring.restype = ctypes.c_char_p
    lib.lua_close.argtypes = [ctypes.c_void_p]
    state = lib.luaL_newstate()
    assert state, 'system Lua state allocation failed'
    try:
        lib.luaL_openlibs(state)
        result = lib.luaL_loadstring(state, script.encode('utf-8'))
        if result == 0:
            result = lib.lua_pcallk(state, 0, 0, 0, 0, None)
        error = lib.lua_tolstring(state, -1, None) if result else None
        assert result == 0, error.decode('utf-8', errors='replace') if error else f'Lua status {result}'
    finally:
        lib.lua_close(state)


class Packages(unittest.TestCase):
    def test_current_catalog_and_complete_helper_packages(self):
        validate_packages(ROOT)

    def test_public_metadata_fixture_replay(self):
        import hashlib
        fixture = read(Path(__file__).parent / 'fixtures/public-metadata.json')
        entries = validate_catalog(fixture['catalog'])
        self.assertEqual(set(entries), set(HELPERS))
        for app, entry in entries.items():
            with self.subTest(app=app):
                manifest = fixture['manifests'][app]
                validate_manifest(entry, manifest)
                self.assertIn(HELPERS[app], entry['files'])
                data = (json.dumps(manifest, indent=2) + '\n').encode()
                blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
                self.assertEqual(blob, fixture['manifest_blob_ids'][f'apps/{app}/manifest.json'])
                canonical = json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode()
                self.assertEqual(hashlib.sha256(canonical).hexdigest(), fixture['manifest_canonical_sha256'][app])

    def test_system_lua_syntax_and_old_firmware_fallback(self):
        import os
        try:
            execute_lua('assert(_VERSION == "Lua 5.4")')
        except LuaUnavailable as error:
            if os.environ.get('KEYBOARD_PACKAGING_REQUIRE_LUA') == '1':
                self.fail(str(error))
            self.skipTest(str(error))
        entries = validate_packages(ROOT)
        fixture = (Path(__file__).parent / 'fallback.lua').read_text(encoding='utf-8')
        for app, helper in HELPERS.items():
            with self.subTest(app=app):
                folder = ROOT / 'apps' / app
                script = 'local folder=' + json.dumps(str(folder)) + '\n'
                for name in entries[app]['files']:
                    if name.endswith('.lua'):
                        script += 'assert(loadfile(folder.."/"..' + json.dumps(name) + '))\n'
                execute_lua(script + 'local helper=' + json.dumps(helper) + '\n' + fixture)


if __name__ == '__main__':
    unittest.main()
