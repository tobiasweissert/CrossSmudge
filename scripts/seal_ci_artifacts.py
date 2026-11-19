#!/usr/bin/env python3
"""Recipient-only CMS AuthEnvelopedData evidence; stdlib + OpenSSL, no CI secret.

seal archives every regular top-level evidence file plus a member-hash manifest.
unseal publishes plaintext only after GCM authentication and exact member checks.
Public receipt is an integrity/identity aid, NOT a signature or retention proof.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile


def openssl(*args, data=None):
    p = subprocess.run(['openssl', *map(str, args)], input=data, capture_output=True)
    if p.returncode:
        raise ValueError('OpenSSL operation failed (details suppressed)')
    return p.stdout


def sha(data):
    return hashlib.sha256(data).hexdigest()


def certificate(path):
    data = Path(path).read_bytes()
    if not re.fullmatch(rb'\s*-----BEGIN CERTIFICATE-----\s+[A-Za-z0-9+/=\r\n]+-----END CERTIFICATE-----\s*', data):
        raise ValueError('Exactly one public PEM certificate required')
    openssl('x509', '-in', path, '-checkend', '2592000', '-noout')
    # Recipient PEM is directly trusted repository configuration, not a CA chain.
    # Check notBefore separately; checkend above checks the retention-time horizon.
    from datetime import datetime, timezone
    start = openssl('x509', '-in', path, '-startdate', '-noout').decode().strip().split('=', 1)[1]
    if datetime.strptime(start, '%b %d %H:%M:%S %Y %Z').replace(tzinfo=timezone.utc) > datetime.now(timezone.utc):
        raise ValueError('Recipient certificate not yet valid')
    text = openssl('x509', '-in', path, '-text', '-noout').decode()
    bits = re.search(r'Public-Key: \((\d+) bit\)', text)
    if 'Public Key Algorithm: rsaEncryption' not in text or not bits or int(bits[1]) < 3072:
        raise ValueError('RSA recipient of at least 3072 bits required')
    if 'X509v3 Key Usage' in text and 'Key Encipherment' not in text:
        raise ValueError('Certificate must permit key encipherment')
    return sha(openssl('x509', '-in', path, '-outform', 'DER'))


def identity():
    fields = {'repository': ('GITHUB_REPOSITORY', r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+'),
              'source_sha': ('GITHUB_SHA', r'[0-9a-f]{40}'),
              'run_id': ('GITHUB_RUN_ID', r'[1-9][0-9]*'),
              'run_attempt': ('GITHUB_RUN_ATTEMPT', r'[1-9][0-9]*')}
    result = {}
    for name, (env, pattern) in fields.items():
        value = os.environ.get(env, '')
        if not re.fullmatch(pattern, value):
            raise ValueError('Missing or invalid run identity: ' + env)
        result[name] = value
    return result


def safe_name(name):
    return bool(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,199}', name))


def seal(evidence, cert, output):
    fingerprint = certificate(cert)
    run = identity()
    source = Path(evidence)
    files = sorted(source.iterdir())
    if not files or any(p.is_symlink() or not p.is_file() or not safe_name(p.name) or p.name == 'sealed-manifest.json' for p in files):
        raise ValueError('Evidence must contain only safe regular top-level files')
    output = Path(output)
    output.mkdir(mode=0o700)  # Never reuse a stale output directory.
    try:
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / 'evidence.tar'
            manifest = {'schema': 1, 'identity': run, 'members': {}}
            with tarfile.open(archive, 'w') as tar:
                for path in files:
                    data = path.read_bytes()
                    manifest['members'][path.name] = {'sha256': sha(data), 'size': len(data)}
                    info = tarfile.TarInfo(path.name)
                    info.size, info.mode = len(data), 0o600
                    tar.addfile(info, io.BytesIO(data))
                data = (json.dumps(manifest, sort_keys=True) + '\n').encode()
                info = tarfile.TarInfo('sealed-manifest.json')
                info.size, info.mode = len(data), 0o600
                tar.addfile(info, io.BytesIO(data))
            envelope = output / 'sealed.envelope'
            openssl('cms', '-encrypt', '-binary', '-aes-256-gcm', '-in', archive,
                    '-outform', 'DER', '-out', envelope, '-recip', cert,
                    '-keyopt', 'rsa_padding_mode:oaep', '-keyopt', 'rsa_oaep_md:sha256')
            receipt = {'schema': 1, 'algorithm': 'CMS AuthEnvelopedData AES-256-GCM RSA-OAEP-SHA256',
                       'identity': run, 'recipient_cert_sha256': fingerprint,
                       'envelope_sha256': sha(envelope.read_bytes())}
            (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    except BaseException:
        for p in output.iterdir():
            p.unlink()
        output.rmdir()
        raise


def unseal(envelope, cert, key, receipt_path, output):
    receipt = json.loads(Path(receipt_path).read_text())
    if receipt['recipient_cert_sha256'] != certificate(cert) or receipt['envelope_sha256'] != sha(Path(envelope).read_bytes()):
        raise ValueError('Receipt fingerprint/digest mismatch')
    target = Path(output)
    if target.exists():
        raise ValueError('Plaintext output must not exist')
    with tempfile.TemporaryDirectory() as tmp:
        archive = Path(tmp) / 'authenticated.tar'
        # OpenSSL can emit unauthenticated partial plaintext: keep it private here.
        openssl('cms', '-decrypt', '-binary', '-inform', 'DER', '-in', envelope,
                '-recip', cert, '-inkey', key, '-out', archive)
        with tarfile.open(archive, 'r:') as tar:
            members = tar.getmembers()
            names = [m.name for m in members]
            if len(names) != len(set(names)) or any(not m.isfile() or not safe_name(m.name) for m in members):
                raise ValueError('Unsafe archive members')
            manifest = json.load(tar.extractfile('sealed-manifest.json'))
            if manifest['schema'] != 1 or manifest['identity'] != receipt['identity'] or set(names) != set(manifest['members']) | {'sealed-manifest.json'}:
                raise ValueError('Archive identity/member mismatch')
            for m in members:
                if m.name != 'sealed-manifest.json':
                    data = tar.extractfile(m).read()
                    if manifest['members'][m.name] != {'sha256': sha(data), 'size': len(data)}:
                        raise ValueError('Member hash/size mismatch')
            target.mkdir(mode=0o700)
            for m in members:
                with (target / m.name).open('xb') as out:
                    os.chmod(out.name, 0o600)
                    out.write(tar.extractfile(m).read())


def main():
    os.umask(0o077)
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    check = sub.add_parser('validate-cert')
    check.add_argument('--cert', required=True)
    enc = sub.add_parser('seal')
    for arg in ['evidence', 'cert', 'output']:
        enc.add_argument('--' + arg, required=True)
    dec = sub.add_parser('unseal')
    for arg in ['envelope', 'cert', 'key', 'receipt', 'output']:
        dec.add_argument('--' + arg, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'validate-cert':
            print(certificate(args.cert))
        elif args.command == 'seal':
            seal(args.evidence, args.cert, args.output)
        else:
            unseal(args.envelope, args.cert, args.key, args.receipt, args.output)
    except (ValueError, OSError, KeyError, tarfile.TarError):
        parser.exit(1, 'Artifact sealing/verification failed; no upload authorized.\n')


if __name__ == '__main__':
    main()
