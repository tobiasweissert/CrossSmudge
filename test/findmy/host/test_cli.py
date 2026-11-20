"""Synthetic offline host contract. Run: python3 -m unittest discover -s test/findmy/host -v."""
import contextlib
import importlib.util
import io
import json
import os
import tempfile
from pathlib import Path
import socket
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
CLI = ROOT / 'tools/findmy/crosscub_findmy.py'


def load():
    spec = importlib.util.spec_from_file_location('crosscub_findmy', CLI)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def invoke(module, args):
    output = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        code = module.main(args)
    return code, output.getvalue()


class OfflineStatus(unittest.TestCase):
    def test_status_and_dry_run_never_start_process_or_network(self):
        self.assertTrue(CLI.exists(), 'offline CLI missing')
        with patch.object(socket, 'socket', side_effect=AssertionError('network forbidden')), patch.object(subprocess, 'Popen', side_effect=AssertionError('process forbidden')):
            module = load()
            for args in ([], ['status'], ['dry-run']):
                code, output = invoke(module, args)
                self.assertEqual(code, 0)
                value = json.loads(output)
                self.assertTrue(value['offline_preparation'])
                for key in ('cloud_registration', 'normal_ble_pairing', 'play_sound', 'key_generation', 'cloud_verified'):
                    self.assertIs(value[key], False)
                self.assertEqual(value['upstream_commit'], '1c0ede3989ee11bd97e9bed169a7fbc6a99db460')


def synthetic():
    return {'schema': 'crosscub-findmy-v1', 'master_private_key_hex': '00' * 27 + '01',
            'primary_ratchet_seed_hex': 'ab' * 32, 'paired_at': '2026-01-01T00:00:00Z'}


class Validation(unittest.TestCase):
    def test_validate_private_input_and_redact_bad_values(self):
        module = load()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.json'
            path.write_text(json.dumps(synthetic()))
            path.chmod(0o600)
            code, output = invoke(module, ['validate', '--input', str(path)])
            self.assertEqual(code, 0, output)
            self.assertEqual(json.loads(output), {'valid': True, 'schema': 'crosscub-findmy-v1', 'registered': False})
            order = int('FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D', 16)
            bad = [[], None, {}, dict(synthetic(), extra='secret-canary')]
            for field, values in {
                'master_private_key_hex': ['00' * 28, format(order, '056x'), 'ff' * 28, 'g' * 56, 'ab' * 27, 1, ' ' + '01' * 28],
                'primary_ratchet_seed_hex': ['ab' * 31, 'z' * 64, None],
                'paired_at': ['2026-01-01', 'bad-secret-canary', '2026-02-30T00:00:00Z'],
                'schema': ['other', None],
            }.items():
                bad.extend(dict(synthetic(), **{field: value}) for value in values)
            for value in bad:
                path.write_text(json.dumps(value))
                code, output = invoke(module, ['validate', '--input', str(path)])
                self.assertEqual(code, 2)
                self.assertEqual(json.loads(output), {'error': 'operation_refused'})
            path.write_text('{"schema":"secret-canary","schema":"crosscub-findmy-v1"}')
            self.assertEqual(invoke(module, ['validate', '--input', str(path)])[0], 2)
            path.write_text('secret-canary')
            self.assertNotIn('secret-canary', invoke(module, ['validate', '--input', str(path)])[1])
            path.write_text(json.dumps(synthetic()))
            path.chmod(0o644)
            self.assertEqual(invoke(module, ['validate', '--input', str(path)])[0], 2)
            path.chmod(0o600)
            link = Path(directory) / 'link'
            link.symlink_to(path)
            self.assertEqual(invoke(module, ['validate', '--input', str(link)])[0], 2)
            linked_directory = Path(directory) / 'dirlink'
            linked_directory.symlink_to(directory)
            self.assertEqual(invoke(module, ['validate', '--input', str(linked_directory / path.name)])[0], 2)
            os.link(path, Path(directory) / 'hardlink')
            self.assertEqual(invoke(module, ['validate', '--input', str(path)])[0], 2)


class Transfer(unittest.TestCase):
    def test_explicit_atomic_transfer_and_collision_refusal(self):
        module = load()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'source.json'
            source.write_text(json.dumps(synthetic()))
            source.chmod(0o600)
            target = Path(directory) / 'crosscub-findmy.json'
            args = ['prepare', '--input', str(source), '--output', str(target)]
            self.assertEqual(invoke(module, args)[0], 2)
            self.assertFalse(target.exists())
            with patch.object(socket, 'socket', side_effect=AssertionError('network')), patch.object(subprocess, 'Popen', side_effect=AssertionError('process')):
                code, output = invoke(module, args + ['--confirm-private-export'])
            self.assertEqual(code, 0, output)
            self.assertNotIn(synthetic()['primary_ratchet_seed_hex'], output)
            self.assertEqual(json.loads(target.read_text()), synthetic())
            self.assertEqual(target.stat().st_mode & 0o777, 0o600)
            original = target.read_bytes()
            self.assertEqual(invoke(module, args + ['--confirm-private-export'])[0], 2)
            self.assertEqual(target.read_bytes(), original)
            target.unlink()
            target.symlink_to(source)
            self.assertEqual(invoke(module, args + ['--confirm-private-export'])[0], 2)
            target.unlink()
            with patch.object(module.os, 'write', side_effect=OSError('secret-canary')):
                code, output = invoke(module, args + ['--confirm-private-export'])
            self.assertEqual(code, 2)
            self.assertNotIn('secret-canary', output)
            self.assertFalse(target.exists())
            self.assertEqual(sorted(p.name for p in Path(directory).iterdir()), ['source.json'])
            Path(directory).chmod(0o755)
            self.assertEqual(invoke(module, args + ['--confirm-private-export'])[0], 2)
            Path(directory).chmod(0o700)
            for output_path in [str(source), 'relative.json', directory + '/../escape.json', str(ROOT / 'tools/findmy/forbidden.json')]:
                self.assertEqual(invoke(module, ['prepare', '--input', str(source), '--output', output_path, '--confirm-private-export'])[0], 2)


class UpstreamImport(unittest.TestCase):
    def test_explicit_owned_upstream_conversion_without_auth_imports(self):
        module = load()
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'tracker.private.json'
            target = Path(directory) / 'crosscub-findmy.json'
            upstream = {'type': 'accessory', 'master_key': synthetic()['master_private_key_hex'],
                        'skn': synthetic()['primary_ratchet_seed_hex'], 'sks': 'cd' * 32,
                        'paired_at': '2026-01-01T00:00:00+00:00', 'alignment_date': '2026-01-01T00:00:00+00:00',
                        'alignment_index': 0, 'name': 'synthetic-canary', 'model': 'ESP32',
                        'identifier': None, 'group_identifier': None, 'serial_number': None}
            source.write_text(json.dumps(upstream))
            source.chmod(0o600)
            args = ['import-upstream', '--input', str(source), '--output', str(target), '--confirm-private-export', '--confirm-own-tracker']
            self.assertEqual(invoke(module, args[:-1])[0], 2)
            with patch.object(socket, 'socket', side_effect=AssertionError('network')), patch.object(subprocess, 'Popen', side_effect=AssertionError('process')):
                code, output = invoke(module, args)
            self.assertEqual(code, 0, output)
            self.assertNotIn('synthetic-canary', output)
            self.assertEqual(json.loads(target.read_text()), synthetic())
            self.assertNotIn('findmy', __import__('sys').modules)
            target.unlink()
            for field, bad in [('alignment_index', 1), ('alignment_index', False), ('sks', 'bad'), ('alignment_date', '2026-01-02T00:00:00+00:00'), ('type', 'account')]:
                source.write_text(json.dumps(dict(upstream, **{field: bad})))
                self.assertEqual(invoke(module, args)[0], 2)
                self.assertFalse(target.exists())


class HostilePaths(unittest.TestCase):
    def test_input_must_be_outside_git_and_non_special(self):
        module = load()
        # Even a 0600 private key must not be accepted from a Git checkout.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / '.git').write_text('gitdir: synthetic')
            source = root / 'private.json'
            source.write_text(json.dumps(synthetic()))
            source.chmod(0o600)
            self.assertEqual(invoke(module, ['validate', '--input', str(source)])[0], 2)
            (root / '.git').unlink()
            for content in [' ' * 4097, '[]', 'null']:
                source.write_text(content)
                self.assertEqual(invoke(module, ['validate', '--input', str(source)])[0], 2)
            source.unlink()
            os.mkfifo(source, 0o600)
            self.assertEqual(invoke(module, ['validate', '--input', str(source)])[0], 2)
            for args in [['--password', 'secret-canary'], ['register', '--confirm', 'secret-canary'], ['validate', '--input', '/missing-secret-canary']]:
                code, output = invoke(module, args)
                self.assertEqual(code, 2)
                self.assertNotIn('secret-canary', output)


if __name__ == '__main__':
    unittest.main()
