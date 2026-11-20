#!/usr/bin/env python3
"""Offline registration preparation and source-pinned handoff. No account execution."""
import argparse
import json
import os
from pathlib import Path
import sys


class SafeError(Exception):
    """Only fixed, allowlisted codes cross the public boundary."""


def private_home(path):
    path = Path(path).absolute()
    for parent in (path, *path.parents):
        if parent.is_symlink() or (parent / '.git').exists():
            raise SafeError('E_PRIVATE_HOME')
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    if not path.is_dir() or path.stat().st_uid != os.getuid() or path.stat().st_mode & 0o777 != 0o700:
        raise SafeError('E_PRIVATE_HOME')
    return path


def signature(record):
    """Extend pinned host/cloud.py fingerprint with the complete zone/identifier binding."""
    import hashlib
    value = (record.record_identifier.value.name, record.type.name,
             sorted((field.identifier.name, field.value.SerializeToString())
                    for field in record.record_field),
             record.protection_info.protection_info,
             record.record_identifier.SerializeToString())
    return hashlib.sha256(repr(value).encode()).hexdigest()


class FindMyAdapter:
    """Injected pinned FindMy contract. Construction never imports auth/runtime code.

    snapshot must be the fully reviewed complete_records, never an incremental list.
    create_semantics must be pinned SaveSemantics.CREATE, not update/upsert.
    No deletion wire format is implemented here.
    """
    def __init__(self, store, snapshot, create_semantics, zone_tag):
        # Checked against pinned constants.py; never rely on record_save's UPDATE default.
        if (getattr(create_semantics, 'name', None) != 'CREATE'
                or getattr(create_semantics, 'value', None) != 2):
            raise SafeError('E_CREATE_ONLY')
        self.store = store
        self.snapshot = snapshot
        self.create_semantics = create_semantics
        self.zone_tag = zone_tag

    async def complete(self):
        try:
            items = await self.snapshot(self.store)
            if not isinstance(items, list) or len(items) > 10000:
                raise SafeError('E_SNAPSHOT')
            result = {}
            for item in items:
                if (not isinstance(item.name, str) or not item.name or item.name in result
                        or item.name != item.source.record_identifier.value.name):
                    raise SafeError('E_SNAPSHOT')
                result[item.name] = signature(item.source)
            return result
        except Exception:
            raise SafeError('E_SNAPSHOT') from None

    async def create(self, record):
        await self.store.client.record_save(
            record, record_protection_info_tag='',
            zone_protection_info_tag=self.zone_tag, semantics=self.create_semantics)


def locked(method):
    """Hold one installation lock across the entire async transaction."""
    import functools
    import fcntl
    @functools.wraps(method)
    async def wrapper(self, *args, **kwargs):
        fd = os.open(self.home / '.lock', os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
        try:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise SafeError('E_BUSY') from None
            return await method(self, *args, **kwargs)
        finally:
            os.close(fd)
    return wrapper


def pair_identity(records):
    import uuid
    try:
        if len(records) != 2 or [r.type.name for r in records] != ['MasterBeaconRecord', 'BeaconNamingRecord']:
            raise ValueError()
        ids = [r.record_identifier.value.name for r in records]
        if len(set(ids)) != 2:
            raise ValueError()
        for ident in ids:
            value = uuid.UUID(ident)
            if str(value) != ident.lower() or value.version != 4:
                raise ValueError()
        return ids, [signature(r) for r in records]
    except Exception:
        raise SafeError('E_PAIR') from None


class PairJournal:
    """One durable owned pair; no keys, credentials, payloads, overwrites or retries."""
    def __init__(self, home):
        self.home = private_home(home)
        self.path = self.home / 'pair.json'

    def save(self, data, *, initial=False):
        import tempfile
        if initial:
            fd = os.open(self.path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, 'w') as stream:
                json.dump(data, stream, sort_keys=True)
                stream.flush()
                os.fsync(stream.fileno())
        else:
            fd, name = tempfile.mkstemp(dir=self.home, prefix='.pair-')
            try:
                with os.fdopen(fd, 'w') as stream:
                    json.dump(data, stream, sort_keys=True)
                    stream.flush()
                    os.fsync(stream.fileno())
                os.replace(name, self.path)
            finally:
                Path(name).unlink(missing_ok=True)
        fd = os.open(self.home, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)

    @locked
    async def prepare(self, records, adapter):
        ids, digests = pair_identity(records)
        if self.path.exists() or self.path.is_symlink():
            raise SafeError('E_EXISTS')
        data = {'ids': ids, 'digests': digests,
                'baseline': await adapter.complete(), 'creation_attempted': [], 'status': 'prepared'}
        if set(data['ids']) & data['baseline'].keys():
            raise SafeError('E_COLLISION')
        self.save(data, initial=True)

    @locked
    async def apply(self, records, adapter, *, execute=False):
        if not execute:
            return 'dry-run'
        ids, digests = pair_identity(records)
        try:
            fd = os.open(self.path, os.O_RDONLY | os.O_NOFOLLOW)
            with os.fdopen(fd, 'r') as stream:
                stat = os.fstat(stream.fileno())
                if stat.st_mode & 0o077 or stat.st_uid != os.getuid():
                    raise ValueError()
                raw = stream.read(2 * 1024 * 1024 + 1)
            if len(raw) > 2 * 1024 * 1024:
                raise ValueError()
            data = json.loads(raw)
            if (set(data) != {'ids', 'digests', 'baseline', 'creation_attempted', 'status'}
                    or data['ids'] != ids or data['digests'] != digests
                    or not isinstance(data['baseline'], dict)
                    or not isinstance(data['creation_attempted'], list)
                    or any(x not in ids for x in data['creation_attempted'])
                    or set(ids) & data['baseline'].keys()):
                raise ValueError()
        except Exception:
            raise SafeError('E_JOURNAL') from None
        if data['creation_attempted'] or data['status'] != 'prepared':
            raise SafeError('E_ALREADY_ATTEMPTED')
        before = await adapter.complete()
        if set(data['ids']) & before.keys():
            raise SafeError('E_COLLISION')
        if before != data['baseline']:
            raise SafeError('E_BASELINE')
        expected = dict(before)
        for record, ident, digest in zip(records, ids, digests):
            data['creation_attempted'].append(ident)
            data['status'] = 'uncertain'
            self.save(data)
            try:
                await adapter.create(record)
                after = await adapter.complete()
            except Exception:
                raise SafeError('E_UNCERTAIN') from None
            expected[ident] = digest
            if after != expected:
                raise SafeError('E_READBACK')
        data['status'] = 'created_verified'
        self.save(data)
        return data['status']


def source_lock():
    import re
    from pathlib import PurePosixPath
    lock = json.loads(Path(__file__).with_name('source.lock.json').read_text())
    if lock['schema'] != 1 or set(lock['repositories']) != {'candidate', 'os', 'findmy'}:
        raise SafeError('E_SOURCE_LOCK')
    seen = set()
    for entry in lock['files']:
        path = PurePosixPath(entry['path'])
        repo = lock['repositories'][entry['repository']]
        key = (entry['repository'], entry['path'])
        if (path.is_absolute() or '..' in path.parts or not path.parts
                or str(path) != entry['path'] or key in seen
                or not re.fullmatch('[0-9a-f]{64}', entry['sha256'])
                or not re.fullmatch('[0-9a-f]{40}', repo['commit'])
                or not re.fullmatch('[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo['repository'])):
            raise SafeError('E_SOURCE_LOCK')
        seen.add(key)
    return lock


def verify_blob(entry, data):
    import hashlib
    if len(data) > 500000 or hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise SafeError('E_SOURCE_HASH')


def source_files(home, *, fetch=False):
    """Only fixed public text URLs. No executing/importing downloaded source."""
    lock = source_lock()
    count = 0
    total = 0
    opener = None
    if fetch:
        import urllib.request
        class NoRedirect(urllib.request.HTTPRedirectHandler):
            def redirect_request(self, *args, **kwargs):
                raise SafeError('E_SOURCE_NETWORK')
        opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())
    for entry in lock['files']:
        repo = lock['repositories'][entry['repository']]
        path = home / 'sources' / entry['repository'] / entry['path']
        for parent in (path, *path.parents):
            if parent.is_symlink():
                raise SafeError('E_SOURCE_PATH')
            if parent == home:
                break
        if path.exists():
            with path.open('rb') as stream:
                data = stream.read(500001)
        elif fetch:
            url = 'https://raw.githubusercontent.com/' + repo['repository'] + '/' + repo['commit'] + '/' + entry['path']
            try:
                with opener.open(url, timeout=20) as response:
                    data = response.read(500001)
                count += 1
            except Exception:
                raise SafeError('E_SOURCE_NETWORK') from None
            verify_blob(entry, data)
            path.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_WRONLY | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, 'wb') as stream:
                stream.write(data)
        else:
            raise SafeError('E_SOURCE_MISSING')
        verify_blob(entry, data)
        total += len(data)
        if total > 4 * 1024 * 1024:
            raise SafeError('E_SOURCE_SIZE')
    return {'status': 'source-verified', 'files': len(lock['files']),
            'public_source_requests': count, 'account_requests': 0, 'processes': 0,
            'executable_closure_approved': False}


class SafeParser(argparse.ArgumentParser):
    def error(self, message):
        # argparse's default diagnostic echoes argv, which might contain a secret.
        raise SafeError('E_ARGUMENTS')


def run(argv=None):
    parser = SafeParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare', 'fetch-sources', 'verify-sources', 'login', 'recover', 'apply', 'remove'))
    parser.add_argument('--private-home')
    parser.add_argument('--allow-public-source-network', action='store_true')
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--allow-account-network', action='store_true')
    parser.add_argument('--confirm-own-tracker', action='store_true')
    args = parser.parse_args(argv)
    if args.operation == 'prepare':
        if not args.private_home:
            raise SafeError('E_PRIVATE_HOME')
        private_home(args.private_home)
        return {'status': 'prepared', 'account_requests': 0, 'processes': 0}
    if args.operation == 'fetch-sources' and not args.execute:
        return {'status': 'dry-run', 'public_source_requests': 0, 'account_requests': 0}
    if args.operation in ('fetch-sources', 'verify-sources'):
        if not args.private_home:
            raise SafeError('E_PRIVATE_HOME')
        if args.operation == 'fetch-sources' and not args.allow_public_source_network:
            raise SafeError('E_NETWORK_DENIED')
        home = private_home(args.private_home)
        return source_files(home, fetch=args.operation == 'fetch-sources')
    if args.execute:
        raise SafeError('E_REVIEW_REQUIRED')
    return {'status': 'dry-run', 'operation': args.operation, 'account_requests': 0}


def main():
    try:
        result = run()
    except SafeError as error:
        allowed = {'E_REVIEW_REQUIRED', 'E_PRIVATE_HOME', 'E_NETWORK_DENIED', 'E_SOURCE_HASH',
                   'E_SOURCE_LOCK', 'E_SOURCE_NETWORK', 'E_SOURCE_MISSING', 'E_SOURCE_PATH', 'E_SOURCE_SIZE', 'E_ARGUMENTS'}
        code = str(error)
        print(json.dumps({'error': code if code in allowed else 'E_STOPPED'}))
        return 1
    except Exception:
        # Never print exception classes, messages, paths, remote bodies or tracebacks.
        print(json.dumps({'error': 'E_STOPPED'}))
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
