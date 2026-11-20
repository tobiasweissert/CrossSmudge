#!/usr/bin/env python3
"""Offline-only Crosscub provisioning preparation; never connects to Apple."""
import json
import sys
import os
import re
import stat
import secrets
from datetime import datetime

UPSTREAM_COMMIT = '1c0ede3989ee11bd97e9bed169a7fbc6a99db460'


ORDER = int('FFFFFFFFFFFFFFFFFFFFFFFFFFFF16A2E0B8F03E13DD29455C5C2A3D', 16)
FIELDS = {'schema', 'master_private_key_hex', 'primary_ratchet_seed_hex', 'paired_at'}


def validate(value):
    if not isinstance(value, dict) or set(value) != FIELDS or value['schema'] != 'crosscub-findmy-v1':
        raise ValueError()
    for field, size in (('master_private_key_hex', 56), ('primary_ratchet_seed_hex', 64)):
        if not isinstance(value[field], str) or not re.fullmatch('[0-9a-f]{' + str(size) + '}', value[field]):
            raise ValueError()
    if not 0 < int(value['master_private_key_hex'], 16) < ORDER:
        raise ValueError()
    date = value['paired_at']
    if not isinstance(date, str) or not re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z', date):
        raise ValueError()
    datetime.strptime(date, '%Y-%m-%dT%H:%M:%SZ')
    return value


def parent_fd(path):
    # Walk descriptor-relative, never follow a symlink in any component.
    if not isinstance(path, str) or not path.startswith('/') or '\x00' in path:
        raise ValueError()
    parts = path.split('/')[1:]
    if not parts or any(part in ('', '.', '..') for part in parts):
        raise ValueError()
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in parts[:-1]:
            next_fd = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = next_fd
            try:
                os.stat('.git', dir_fd=fd, follow_symlinks=False)
            except FileNotFoundError:
                pass
            else:
                raise ValueError()
        return fd, parts[-1]
    except BaseException:
        os.close(fd)
        raise


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError()
        value[key] = item
    return value


def read_private(path):
    directory, name = parent_fd(path)
    try:
        fd = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
        with os.fdopen(fd, 'rb') as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode) or stat.S_IMODE(info.st_mode) != 0o600 or info.st_uid != os.getuid() or info.st_nlink != 1:
                raise ValueError()
            raw = stream.read(4097)
            if len(raw) > 4096:
                raise ValueError()
            return json.loads(raw, object_pairs_hook=unique_object)
    finally:
        os.close(directory)


def write_private(path, value):
    # parent_fd rejects symlinks and every Git checkout ancestor.
    directory, name = parent_fd(path)
    pending = '.crosscub-' + secrets.token_hex(16)
    created = False
    try:
        info = os.fstat(directory)
        if info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) != 0o700:
            raise ValueError()
        fd = os.open(pending, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=directory)
        created = True
        try:
            os.fchmod(fd, 0o600)
            data = (json.dumps(value, sort_keys=True) + '\n').encode('ascii')
            while data:
                count = os.write(fd, data)
                if count <= 0:
                    raise OSError()
                data = data[count:]
            os.fsync(fd)
        finally:
            os.close(fd)
        # link, unlike rename/replace, refuses an existing target atomically.
        os.link(pending, name, src_dir_fd=directory, dst_dir_fd=directory, follow_symlinks=False)
        os.fsync(directory)
    finally:
        try:
            if created:
                os.unlink(pending, dir_fd=directory)
                os.fsync(directory)
        finally:
            os.close(directory)


def convert_upstream(value):
    # Parse data only; never import upstream account/authentication code.
    required = {'type', 'master_key', 'skn', 'sks', 'paired_at', 'name', 'model',
                'identifier', 'alignment_date', 'alignment_index'}
    if not isinstance(value, dict) or not required <= set(value) or set(value) - required - {'group_identifier', 'serial_number'}:
        raise ValueError()
    if value['type'] != 'accessory' or type(value['alignment_index']) is not int or value['alignment_index'] != 0:
        raise ValueError()
    if value['alignment_date'] not in (None, value['paired_at']):
        raise ValueError()
    if not isinstance(value['sks'], str) or not re.fullmatch('[0-9a-f]{64}', value['sks']):
        raise ValueError()
    date = value['paired_at']
    if not isinstance(date, str) or not re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\+00:00', date):
        raise ValueError()
    return validate({'schema': 'crosscub-findmy-v1', 'master_private_key_hex': value['master_key'],
                     'primary_ratchet_seed_hex': value['skn'], 'paired_at': date[:-6] + 'Z'})


def main(argv=None):
    args = sys.argv[1:] if argv is None else argv
    if not args or args in (['status'], ['dry-run']):
        print(json.dumps(dict(offline_preparation=True, cloud_registration=False,
            normal_ble_pairing=False, play_sound=False, key_generation=False,
            cloud_verified=False, upstream_commit=UPSTREAM_COMMIT)))
        return 0
    if len(args) == 3 and args[:2] == ['validate', '--input']:
        try:
            validate(read_private(args[2]))
            print('{"valid":true,"schema":"crosscub-findmy-v1","registered":false}')
            return 0
        except Exception:
            print('{"error":"operation_refused"}')
            return 2
    if len(args) == 6 and args[:2] == ['prepare', '--input'] and args[3] == '--output' and args[5] == '--confirm-private-export':
        try:
            value = validate(read_private(args[2]))
            write_private(args[4], value)
            print('{"prepared":true,"registered":false}')
            return 0
        except Exception:
            print('{"error":"operation_refused"}')
            return 2
    if len(args) == 7 and args[:2] == ['import-upstream', '--input'] and args[3] == '--output' and args[5:] == ['--confirm-private-export', '--confirm-own-tracker']:
        try:
            value = convert_upstream(read_private(args[2]))
            write_private(args[4], value)
            print('{"prepared":true,"registered":false}')
            return 0
        except Exception:
            print('{"error":"operation_refused"}')
            return 2
    print('{"error":"unsupported_operation"}')
    return 2


if __name__ == '__main__':
    sys.exit(main())
