#!/usr/bin/env python3
"""Portable negative controls for the packaging oracle (no firmware or network)."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import test_packages as packaging


class ContractControls(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='keyboard-package-unit-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.folder = self.root / 'apps/demo'
        self.folder.mkdir(parents=True)
        self.manifest = dict(id='demo', name='Demo', version='1.0.0', author='Fixture',
                             description='Synthetic test only', entry='main.lua')
        self.record = dict(self.manifest, files=['manifest.json', 'main.lua', 'keyboard.lua'])
        self.record.pop('entry')
        self.catalog = {'repo': 'synthetic/example', 'apps': [self.record]}
        (self.folder / 'main.lua').write_text("return dofile('keyboard.lua')\n")
        (self.folder / 'keyboard.lua').write_text('return {}\n')
        self.save()

    def save(self):
        (self.root / 'apps/catalog.json').write_text(json.dumps(self.catalog))
        (self.folder / 'manifest.json').write_text(json.dumps(self.manifest))

    def validate(self):
        return packaging.validate_packages(self.root, {'demo': 'keyboard.lua'})

    def test_duplicate_catalog_ids_rejected(self):
        with self.assertRaisesRegex(AssertionError, 'duplicate.*id'):
            packaging.validate_catalog({'apps': [{'id': 'foreign'}, {'id': 'foreign'}]})

    def test_live_packages_without_external_evidence(self):
        packaging.validate_packages(packaging.ROOT)

    def test_missing_helper_membership_rejected(self):
        self.record['files'].remove('keyboard.lua')
        self.save()
        with self.assertRaisesRegex(AssertionError, 'missing helper'):
            self.validate()

    def test_missing_payload_file_rejected(self):
        (self.folder / 'keyboard.lua').unlink()
        with self.assertRaisesRegex(AssertionError, 'payload keyboard.lua'):
            self.validate()

    def test_manifest_version_mismatch_rejected(self):
        self.manifest['version'] = '2.0.0'
        self.save()
        with self.assertRaisesRegex(AssertionError, 'version mismatch'):
            self.validate()

    def test_undeclared_literal_dofile_rejected(self):
        (self.folder / 'main.lua').write_text("return dofile ( 'missing.lua' )\n")
        with self.assertRaisesRegex(AssertionError, 'undeclared dofile.*missing.lua'):
            self.validate()

    def test_undeclared_dynamic_choice_rejected(self):
        (self.folder / 'main.lua').write_text('return dofile(flag and "keyboard.lua" or "missing.lua")\n')
        with self.assertRaisesRegex(AssertionError, 'undeclared dofile.*missing.lua'):
            self.validate()

    def test_duplicate_file_rejected(self):
        self.record['files'].append('keyboard.lua')
        self.save()
        with self.assertRaisesRegex(AssertionError, 'duplicate package file'):
            self.validate()

    def test_unsafe_path_rejected(self):
        self.record['files'].append('../foreign.lua')
        self.save()
        with self.assertRaisesRegex(AssertionError, 'unsafe package path'):
            self.validate()

    def test_future_source_version_and_catalog_growth_allowed(self):
        foreign = {'id': 'foreign', 'version': '9.9.9', 'files': ['unavailable.raw'],
                   'future_extension': {'nested': [True, 'preserved']}}
        self.catalog['apps'].append(copy.deepcopy(foreign))
        self.catalog['future_schema'] = {'retained': True}
        self.record['version'] = self.manifest['version'] = '2.0.0'
        (self.folder / 'keyboard.lua').write_text('-- legitimate future change\nreturn {future=true}\n')
        self.save()
        before = copy.deepcopy(self.catalog)
        entries = self.validate()
        self.assertEqual(entries['foreign'], foreign)
        self.assertEqual(self.catalog, before, 'validation must not rewrite foreign records')
        self.assertEqual(packaging.read(self.root / 'apps/catalog.json'), before)

    def test_lua_dependency_is_explicit(self):
        with patch.object(packaging.ctypes.util, 'find_library', return_value=None):
            with self.assertRaisesRegex(packaging.LuaUnavailable, 'liblua5.4'):
                packaging.execute_lua('assert(true)')

    def test_lua_invalid_syntax_and_failed_assertion_reported(self):
        try:
            packaging.execute_lua('assert(true)')
        except packaging.LuaUnavailable as error:
            self.skipTest(str(error))
        for source in ('this is not valid Lua', 'assert(false, "negative-control")'):
            with self.subTest(source=source), self.assertRaises(AssertionError):
                packaging.execute_lua(source)


if __name__ == '__main__':
    unittest.main()
