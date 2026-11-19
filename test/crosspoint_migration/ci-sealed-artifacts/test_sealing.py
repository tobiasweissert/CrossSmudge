"""Offline qualification on tiny SYNTHETIC bytes, never live GitHub retention."""
import importlib.util
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('seal_ci_artifacts', ROOT / 'scripts/seal_ci_artifacts.py')
s = importlib.util.module_from_spec(spec)
spec.loader.exec_module(s)


class Sealing(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        cls.base = Path(cls.keys.name)
        for name in ['recipient', 'wrong']:
            s.openssl('req', '-x509', '-newkey', 'rsa:3072', '-noenc',
                      '-keyout', cls.base / (name + '.key'), '-out', cls.base / (name + '.pem'),
                      '-days', '365', '-subj', '/CN=SYNTHETIC offline test',
                      '-addext', 'basicConstraints=critical,CA:FALSE',
                      '-addext', 'keyUsage=critical,keyEncipherment')

    @classmethod
    def tearDownClass(cls):
        cls.keys.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.evidence = self.root / 'sticky-evidence'
        self.evidence.mkdir()
        self.contents = {'firmware.bin': b'\xe9SYNTHETIC BIN\x00\xff',
                         'firmware.elf': b'\x7fELF SYNTHETIC NOT FIRMWARE',
                         'build.log': b'private /runner/path diagnostics',
                         'image-validation.json': b'{"valid":false,"publication_authorized":false}'}
        for name, data in self.contents.items():
            (self.evidence / name).write_bytes(data)
        self.cert, self.key = self.base / 'recipient.pem', self.base / 'recipient.key'
        self.out = self.root / 'sealed'
        self.env = patch.dict(os.environ, {'GITHUB_REPOSITORY': 'tobiasweissert/Crosscub',
                                          'GITHUB_SHA': 'a' * 40, 'GITHUB_RUN_ID': '1', 'GITHUB_RUN_ATTEMPT': '1'})
        self.env.start()
        self.addCleanup(self.env.stop)

    def seal(self):
        s.seal(self.evidence, self.cert, self.out)
        self.envelope, self.receipt = self.out / 'sealed.envelope', self.out / 'receipt.json'

    def decrypt(self, cert=None, key=None):
        s.unseal(self.envelope, cert or self.cert, key or self.key, self.receipt, self.root / 'plaintext')

    def refresh_digest(self):
        receipt = json.loads(self.receipt.read_text())
        receipt['envelope_sha256'] = s.sha(self.envelope.read_bytes())
        self.receipt.write_text(json.dumps(receipt))

    def test_bin_elf_provenance_roundtrip_and_cms_algorithm(self):
        self.seal()
        structure = s.openssl('cms', '-cmsout', '-print', '-inform', 'DER', '-in', self.envelope).decode()
        self.assertIn('id-smime-ct-authEnvelopedData', structure)
        self.assertIn('aes-256-gcm', structure)
        self.assertIn('rsaesOaep', structure)
        self.decrypt()
        for name, data in self.contents.items():
            self.assertEqual((self.root / 'plaintext' / name).read_bytes(), data)
        manifest = json.loads((self.root / 'plaintext/sealed-manifest.json').read_text())
        self.assertEqual(set(manifest['members']), set(self.contents))
        for name, data in self.contents.items():
            self.assertEqual(manifest['members'][name], {'sha256': s.sha(data), 'size': len(data)})
        self.assertEqual({p.name for p in self.out.iterdir()}, {'sealed.envelope', 'receipt.json'})
        self.assertNotIn('/runner/path', self.receipt.read_text())
        self.assertNotIn('private', self.receipt.read_text())

    def test_tamper_authentication_even_with_updated_public_digest(self):
        self.seal()
        data = bytearray(self.envelope.read_bytes())
        data[-1] ^= 1  # GCM tag
        self.envelope.write_bytes(data)
        self.refresh_digest()
        with self.assertRaises(ValueError):
            self.decrypt()
        self.assertFalse((self.root / 'plaintext').exists())

    def test_truncation_even_with_updated_public_digest(self):
        self.seal()
        self.envelope.write_bytes(self.envelope.read_bytes()[:-8])
        self.refresh_digest()
        with self.assertRaises(ValueError):
            self.decrypt()
        self.assertFalse((self.root / 'plaintext').exists())

    def test_wrong_private_key(self):
        self.seal()
        with self.assertRaises(ValueError):
            self.decrypt(key=self.base / 'wrong.key')
        self.assertFalse((self.root / 'plaintext').exists())

    def test_wrong_recipient_even_with_updated_public_fingerprint(self):
        self.seal()
        receipt = json.loads(self.receipt.read_text())
        receipt['recipient_cert_sha256'] = s.certificate(self.base / 'wrong.pem')
        self.receipt.write_text(json.dumps(receipt))
        with self.assertRaises(ValueError):
            self.decrypt(cert=self.base / 'wrong.pem', key=self.base / 'wrong.key')

    def test_missing_empty_or_private_key_certificate_fails_closed(self):
        for value in [b'', self.key.read_bytes(), b'not a certificate']:
            cert = self.root / 'bad.pem'
            cert.write_bytes(value)
            with self.assertRaises(ValueError):
                s.seal(self.evidence, cert, self.out)
            self.assertFalse(self.out.exists())
        with self.assertRaises(OSError):
            s.seal(self.evidence, self.root / 'absent.pem', self.out)

    def test_unsafe_member_and_stale_output_rejected(self):
        (self.evidence / 'link').symlink_to(self.evidence / 'firmware.elf')
        with self.assertRaises(ValueError):
            self.seal()
        (self.evidence / 'link').unlink()
        self.seal()
        with self.assertRaises(FileExistsError):
            self.seal()

    def test_authenticated_archive_with_wrong_member_hash_rejected(self):
        self.seal()
        archive = self.root / 'crafted.tar'
        receipt = json.loads(self.receipt.read_text())
        manifest = {'schema': 1, 'identity': receipt['identity'],
                    'members': {'firmware.bin': {'sha256': '0' * 64, 'size': 3}}}
        with tarfile.open(archive, 'w') as tar:
            for name, data in [('firmware.bin', b'bad'), ('sealed-manifest.json', json.dumps(manifest).encode())]:
                info = tarfile.TarInfo(name)
                info.size = len(data)
                tar.addfile(info, io.BytesIO(data))
        s.openssl('cms', '-encrypt', '-binary', '-aes-256-gcm', '-in', archive, '-outform', 'DER',
                  '-out', self.envelope, '-recip', self.cert)
        self.refresh_digest()
        with self.assertRaisesRegex(ValueError, 'Member hash'):
            self.decrypt()
        self.assertFalse((self.root / 'plaintext').exists())

    def test_short_lived_certificate_rejected(self):
        cert = self.root / 'short.pem'
        s.openssl('req', '-x509', '-new', '-key', self.key, '-out', cert,
                  '-days', '1', '-subj', '/CN=Short lifetime synthetic')
        with self.assertRaises(ValueError):
            s.seal(self.evidence, cert, self.out)
        self.assertFalse(self.out.exists())

    def test_invalid_run_identity_rejected(self):
        with patch.dict(os.environ, {'GITHUB_SHA': '/sensitive/path'}):
            with self.assertRaises(ValueError):
                self.seal()
        self.assertFalse(self.out.exists())


if __name__ == '__main__':
    unittest.main()
