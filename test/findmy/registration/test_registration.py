"""Credential-free tests; all service records/transports are synthetic."""
import importlib.util
import pathlib
import tempfile
import unittest
import asyncio
import json
import types
from enum import IntEnum


class SaveSemantics(IntEnum):
    CREATE = 2
    UPDATE = 3



def record(ident, kind, payload=b'SYNTHETIC'):
    class FieldValue:
        def SerializeToString(self):
            return payload
    return types.SimpleNamespace(
        record_identifier=types.SimpleNamespace(value=types.SimpleNamespace(name=ident), SerializeToString=lambda: b'SYNTHETIC-ADDRESS:' + ident.encode()),
        type=types.SimpleNamespace(name=kind),
        record_field=[types.SimpleNamespace(identifier=types.SimpleNamespace(name='synthetic'), value=FieldValue())],
        protection_info=types.SimpleNamespace(protection_info=b'SYNTHETIC-NOT-PCS'))


IDS = ['11111111-1111-4111-8111-111111111111', '22222222-2222-4222-8222-222222222222']


class FixtureClient:
    def __init__(self):
        self.rows = {}
        self.writes = []

    async def record_save(self, value, **kwargs):
        ident = value.record_identifier.value.name
        if ident in self.rows:
            raise RuntimeError('SYNTHETIC_COLLISION_CANARY')
        self.rows[ident] = value
        self.writes.append((ident, kwargs))


async def snapshot(store):
    return [types.SimpleNamespace(name=key, source=value) for key, value in store.client.rows.items()]

ROOT = pathlib.Path(__file__).resolve().parents[3]
MODULE = ROOT / 'tools/findmy_registration/registration.py'


def load():
    if not MODULE.exists():
        raise AssertionError('registration entrypoint not implemented')
    spec = importlib.util.spec_from_file_location('registration', MODULE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class RegistrationTests(unittest.TestCase):
    def test_prepare_is_credential_free_and_live_operations_fail_closed(self):
        app = load()
        with tempfile.TemporaryDirectory() as directory:
            home = pathlib.Path(directory) / 'owner'
            result = app.run(['prepare', '--private-home', str(home)])
            self.assertEqual(result, {'status': 'prepared', 'account_requests': 0, 'processes': 0})
            self.assertEqual(home.stat().st_mode & 0o777, 0o700)
            for action in ('login', 'recover', 'apply', 'remove'):
                self.assertEqual(app.run([action]), {'status': 'dry-run', 'operation': action, 'account_requests': 0})
                with self.assertRaisesRegex(app.SafeError, '^E_REVIEW_REQUIRED$'):
                    app.run([action, '--execute', '--allow-account-network', '--confirm-own-tracker'])
    def test_exact_pair_journal_and_actual_record_save_contract(self):
        app = load()
        self.assertTrue(hasattr(app, 'PairJournal'), 'two-record adapter not implemented')
        with tempfile.TemporaryDirectory() as directory:
            home = pathlib.Path(directory) / 'owner'
            store = types.SimpleNamespace(client=FixtureClient())
            adapter = app.FindMyAdapter(store, snapshot, SaveSemantics.CREATE, 'SYNTHETIC_ZONE_TAG')
            pair = [record(IDS[0], 'MasterBeaconRecord'), record(IDS[1], 'BeaconNamingRecord')]
            journal = app.PairJournal(home)
            asyncio.run(journal.prepare(pair, adapter))
            self.assertEqual(json.loads((home / 'pair.json').read_text())['ids'], IDS)
            self.assertEqual(asyncio.run(journal.apply(pair, adapter)), 'dry-run')
            self.assertEqual(store.client.writes, [])
            self.assertEqual(asyncio.run(journal.apply(pair, adapter, execute=True)), 'created_verified')
            self.assertEqual([ident for ident, _ in store.client.writes], IDS)
            for _, kwargs in store.client.writes:
                self.assertEqual(kwargs, {'record_protection_info_tag': '', 'zone_protection_info_tag': 'SYNTHETIC_ZONE_TAG', 'semantics': SaveSemantics.CREATE})
            saved = json.loads((home / 'pair.json').read_text())
            self.assertEqual(saved['creation_attempted'], IDS)
            self.assertEqual(saved['status'], 'created_verified')
            self.assertNotIn('SYNTHETIC-NOT-PCS', (home / 'pair.json').read_text())
    def test_collision_replay_and_uncertain_write_never_overwrite(self):
        app = load()
        with tempfile.TemporaryDirectory() as directory:
            home = pathlib.Path(directory) / 'owner'
            store = types.SimpleNamespace(client=FixtureClient())
            adapter = app.FindMyAdapter(store, snapshot, SaveSemantics.CREATE, '')
            pair = [record(IDS[0], 'MasterBeaconRecord'), record(IDS[1], 'BeaconNamingRecord')]
            store.client.rows[IDS[0]] = pair[0]
            journal = app.PairJournal(home)
            with self.assertRaisesRegex(app.SafeError, '^E_COLLISION$'):
                asyncio.run(journal.prepare(pair, adapter))
            self.assertFalse((home / 'pair.json').exists())
            store.client.rows.clear()
            asyncio.run(journal.prepare(pair, adapter))
            original = (home / 'pair.json').read_bytes()
            with self.assertRaisesRegex(app.SafeError, '^E_EXISTS$'):
                asyncio.run(journal.prepare(pair, adapter))
            self.assertEqual(original, (home / 'pair.json').read_bytes())
            save = store.client.record_save
            async def lost_reply(value, **kwargs):
                await save(value, **kwargs)
                raise RuntimeError('CREDENTIAL_CANARY_DO_NOT_PRINT')
            store.client.record_save = lost_reply
            with self.assertRaisesRegex(app.SafeError, '^E_UNCERTAIN$'):
                asyncio.run(journal.apply(pair, adapter, execute=True))
            self.assertEqual(json.loads((home / 'pair.json').read_text())['creation_attempted'], IDS[:1])
            with self.assertRaisesRegex(app.SafeError, '^E_ALREADY_ATTEMPTED$'):
                asyncio.run(journal.apply(pair, adapter, execute=True))
            self.assertEqual(len(store.client.writes), 1)
            self.assertNotIn('CANARY', (home / 'pair.json').read_text())
    def test_pair_validation_readback_and_changed_baseline(self):
        app = load()
        with tempfile.TemporaryDirectory() as directory:
            store = types.SimpleNamespace(client=FixtureClient())
            adapter = app.FindMyAdapter(store, snapshot, SaveSemantics.CREATE, '')
            pair = [record(IDS[0], 'MasterBeaconRecord'), record(IDS[1], 'BeaconNamingRecord')]
            journal = app.PairJournal(pathlib.Path(directory) / 'owner')
            for bad in (pair[:1], pair + pair[:1], [pair[0], pair[0]], pair[::-1]):
                with self.assertRaisesRegex(app.SafeError, '^E_PAIR$'):
                    asyncio.run(journal.prepare(bad, adapter))
            asyncio.run(journal.prepare(pair, adapter))
            with self.assertRaisesRegex(app.SafeError, '^E_JOURNAL$'):
                asyncio.run(journal.apply([pair[0], record(IDS[1], 'BeaconNamingRecord', b'changed')], adapter, execute=True))
            self.assertEqual(store.client.writes, [])
            save = store.client.record_save
            async def wrong_readback(value, **kwargs):
                state = json.loads(journal.path.read_text())
                self.assertEqual(state['status'], 'uncertain')
                self.assertEqual(state['creation_attempted'], IDS[:1])
                await save(value, **kwargs)
                store.client.rows[IDS[0]] = record(IDS[0], 'MasterBeaconRecord', b'wrong')
            store.client.record_save = wrong_readback
            with self.assertRaisesRegex(app.SafeError, '^E_READBACK$'):
                asyncio.run(journal.apply(pair, adapter, execute=True))
            self.assertEqual(len(store.client.writes), 1)

    def test_ambiguous_snapshot_and_baseline_change_refuse_writes(self):
        app = load()
        with tempfile.TemporaryDirectory() as directory:
            store = types.SimpleNamespace(client=FixtureClient())
            pair = [record(IDS[0], 'MasterBeaconRecord'), record(IDS[1], 'BeaconNamingRecord')]
            async def duplicate(store):
                item = types.SimpleNamespace(name=IDS[0], source=pair[0])
                return [item, item]
            adapter = app.FindMyAdapter(store, duplicate, SaveSemantics.CREATE, '')
            journal = app.PairJournal(pathlib.Path(directory) / 'owner')
            with self.assertRaisesRegex(app.SafeError, '^E_SNAPSHOT$'):
                asyncio.run(journal.prepare(pair, adapter))
            adapter.snapshot = snapshot
            store.client.rows['old-owner-record'] = record('old-owner-record', 'OtherRecord')
            asyncio.run(journal.prepare(pair, adapter))
            store.client.rows['old-owner-record'] = record('old-owner-record', 'OtherRecord', b'changed')
            with self.assertRaisesRegex(app.SafeError, '^E_BASELINE$'):
                asyncio.run(journal.apply(pair, adapter, execute=True))
            self.assertEqual(store.client.writes, [])

    def test_private_home_symlinks_git_permissions_and_lock(self):
        app = load()
        import fcntl
        import os
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / '.git').mkdir()
            with self.assertRaisesRegex(app.SafeError, '^E_PRIVATE_HOME$'):
                app.PairJournal(root / 'private')
            (root / '.git').rmdir()
            (root / 'target').mkdir(mode=0o700)
            (root / 'link').symlink_to(root / 'target')
            with self.assertRaisesRegex(app.SafeError, '^E_PRIVATE_HOME$'):
                app.PairJournal(root / 'link')
            (root / 'public').mkdir(mode=0o755)
            with self.assertRaisesRegex(app.SafeError, '^E_PRIVATE_HOME$'):
                app.PairJournal(root / 'public')
            (root / 'owner').mkdir(mode=0o500)
            with self.assertRaisesRegex(app.SafeError, '^E_PRIVATE_HOME$'):
                app.PairJournal(root / 'owner')
            (root / 'owner').chmod(0o700)
            journal = app.PairJournal(root / 'owner')
            fd = os.open(journal.home / '.lock', os.O_CREAT | os.O_RDWR, 0o600)
            try:
                fcntl.flock(fd, fcntl.LOCK_EX)
                store = types.SimpleNamespace(client=FixtureClient())
                adapter = app.FindMyAdapter(store, snapshot, SaveSemantics.CREATE, '')
                with self.assertRaisesRegex(app.SafeError, '^E_BUSY$'):
                    asyncio.run(journal.prepare([record(IDS[0], 'MasterBeaconRecord'), record(IDS[1], 'BeaconNamingRecord')], adapter))
            finally:
                os.close(fd)
    def test_source_hash_pins_and_public_network_default_deny(self):
        app = load()
        self.assertTrue(hasattr(app, 'source_lock'), 'pinned source handoff not implemented')
        lock = app.source_lock()
        expected = {
            'candidate': '1c0ede3989ee11bd97e9bed169a7fbc6a99db460',
            'findmy': '7969003ca785658369b650f75d9e7ca519566bf6',
            'os': '374d83854f657a7fdca45f813387d99d491db0e2'}
        self.assertEqual({key: value['commit'] for key, value in lock['repositories'].items()}, expected)
        blob = b'{\n  "findmy": {\n    "commit": "7969003ca785658369b650f75d9e7ca519566bf6",\n    "url": "https://github.com/parawanderer/FindMy.py/archive/7969003ca785658369b650f75d9e7ca519566bf6.zip",\n    "sha256": "efb082865de10feeab1896ceeb6a93a4dfd5b7810769b8a25e7f2940fc2ea575"\n  }\n}\n'
        entry = next(item for item in lock['files'] if item['repository'] == 'candidate' and item['path'] == 'sources.lock.json')
        app.verify_blob(entry, blob)
        with self.assertRaisesRegex(app.SafeError, '^E_SOURCE_HASH$'):
            app.verify_blob(entry, blob + b'changed')
        with tempfile.TemporaryDirectory() as directory:
            result = app.run(['fetch-sources', '--private-home', str(pathlib.Path(directory) / 'owner')])
            self.assertEqual(result['public_source_requests'], 0)
            self.assertEqual(result['status'], 'dry-run')
    def test_create_semantics_cannot_be_update_and_cli_errors_are_fixed(self):
        app = load()
        store = types.SimpleNamespace(client=FixtureClient())
        with self.assertRaisesRegex(app.SafeError, '^E_CREATE_ONLY$'):
            app.FindMyAdapter(store, snapshot, SaveSemantics.UPDATE, '')
        import subprocess
        import sys
        result = subprocess.run([sys.executable, str(MODULE), 'apply', '--execute', '--private-home', 'CREDENTIAL_CANARY'], capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(json.loads(result.stdout), {'error': 'E_REVIEW_REQUIRED'})
        self.assertEqual(result.stderr, '')
        self.assertNotIn('CANARY', result.stdout)
    def test_pinned_record_save_function_runs_with_synthetic_transport(self):
        app = load()
        import ast
        import hashlib
        import logging
        file = ROOT / 'test/findmy/registration/fixtures/cloudkit_client.py'
        self.assertTrue(file.exists(), 'actual pinned client source fixture not present')
        raw = file.read_bytes()
        expected = next(item['sha256'] for item in app.source_lock()['files'] if item['repository'] == 'findmy' and item['path'] == 'findmy/cloudkit/client.py')
        self.assertEqual(hashlib.sha256(raw).hexdigest(), expected)
        tree = ast.parse(raw)
        cls = next(node for node in tree.body if isinstance(node, ast.ClassDef) and node.name == 'AsyncCloudKitClient')
        method = next(node for node in cls.body if isinstance(node, ast.AsyncFunctionDef) and node.name == 'record_save')
        future = ast.ImportFrom(module='__future__', names=[ast.alias(name='annotations')], level=0)
        selected = ast.fix_missing_locations(ast.Module(body=[future, method], type_ignores=[]))
        scope = {'SaveSemantics': SaveSemantics, 'OperationType': types.SimpleNamespace(RECORD_SAVE_TYPE=210),
                 'PATH_RECORD_SAVE': '/api/client/record/save', 'logger': logging.getLogger('synthetic'), 'UnhandledProtocolError': RuntimeError}
        exec(compile(selected, '<pinned-record-save-only>', 'exec'), scope)
        class Client(FixtureClient):
            async def open_container(self):
                pass
            def _build_request(self, operation):
                self.operation = operation
                class Copy:
                    def CopyFrom(inner, value):
                        inner.value = value
                return types.SimpleNamespace(record_save_request=types.SimpleNamespace(record=Copy()))
            async def _post_operation(self, path, request):
                self.last = request.record_save_request
                self.last_path = path
                await FixtureClient.record_save(self, self.last.record.value, semantics=self.last.save_semantics)
                return types.SimpleNamespace(record_save_response=types.SimpleNamespace(server_fields=self.last.record.value))
        client = Client()
        client.record_save = types.MethodType(scope['record_save'], client)
        store = types.SimpleNamespace(client=client)
        adapter = app.FindMyAdapter(store, snapshot, SaveSemantics.CREATE, 'SYNTHETIC_TAG')
        with tempfile.TemporaryDirectory() as directory:
            journal = app.PairJournal(pathlib.Path(directory) / 'owner')
            pair = [record(IDS[0], 'MasterBeaconRecord'), record(IDS[1], 'BeaconNamingRecord')]
            asyncio.run(journal.prepare(pair, adapter))
            self.assertEqual(asyncio.run(journal.apply(pair, adapter, execute=True)), 'created_verified')
        self.assertEqual(client.last_path, '/api/client/record/save')
        self.assertEqual(client.operation, 210)
        self.assertEqual(client.last.save_semantics, 2)
        self.assertTrue(client.last.merge)
        self.assertEqual(client.last.zone_protection_info_tag, 'SYNTHETIC_TAG')
        self.assertEqual(client.last.record_protection_info_tag, '')
    def test_unknown_arguments_do_not_echo_credential_canaries(self):
        import subprocess
        import sys
        result = subprocess.run([sys.executable, str(MODULE), 'login', '--password', 'CREDENTIAL_CANARY'], capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stderr, '')
        self.assertEqual(json.loads(result.stdout), {'error': 'E_ARGUMENTS'})

    def test_default_commands_under_process_and_network_denial(self):
        import subprocess
        import sys
        code = '''import importlib.util,json,pathlib,sys,tempfile
spec=importlib.util.spec_from_file_location('registration',sys.argv[1])
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
def audit(event,args):
    if event.startswith(('socket.', 'subprocess.', 'os.exec', 'os.spawn', 'os.system')):
        raise RuntimeError('DENIED')
sys.addaudithook(audit)
with tempfile.TemporaryDirectory() as d:
    m.run(['prepare','--private-home',str(pathlib.Path(d)/'owner')])
    m.run(['fetch-sources'])
    for action in ('login','recover','apply','remove'):
        m.run([action])
        try:m.run([action,'--execute','--allow-account-network','--confirm-own-tracker'])
        except m.SafeError:pass
        else:raise AssertionError('live gate missing')
assert not any(name.split('.')[0] in ('findmy','anisette') for name in sys.modules)
print(json.dumps({'denial_enforced':True,'upstream_imports':False}))
'''
        result = subprocess.run([sys.executable, '-I', '-B', '-c', code, str(MODULE)], capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), {'denial_enforced': True, 'upstream_imports': False})
    def test_full_identifier_zone_is_bound_not_only_record_name(self):
        app = load()
        first = record(IDS[0], 'MasterBeaconRecord')
        other_zone = record(IDS[0], 'MasterBeaconRecord')
        other_zone.record_identifier.SerializeToString = lambda: b'SYNTHETIC-OTHER-ZONE'
        self.assertNotEqual(app.signature(first), app.signature(other_zone))


if __name__ == '__main__':
    unittest.main()
