"""Execute release workflow scripts with synthetic bytes, never target firmware."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_release import ROOT, synthetic_image


def steps():
    text = (ROOT / '.github/workflows/release.yml').read_text()
    return {m[0]: m[1] for m in re.findall(
        r'^      - name: ([^\n]+)\n(.*?)(?=^      - |^  [a-z].*:\n|\Z)', text, re.M | re.S)}


def script(step):
    body = step.split('        run: |\n', 1)[1]
    return '\n'.join(line[10:] if line.startswith('          ') else line
                     for line in body.splitlines()) + '\n'


class ArtifactPrivacyTests(unittest.TestCase):
    def run_step(self, name, cwd, **env):
        return subprocess.run(['bash', '-euo', 'pipefail', '-c', script(steps()[name])],
                              cwd=cwd, env={**os.environ, **env}, capture_output=True)

    def test_public_stage_contains_only_validated_bin_and_report(self):
        with tempfile.TemporaryDirectory(prefix='synthetic-public-stage-') as tmp:
            cwd = Path(tmp)
            (cwd / 'scripts').symlink_to(ROOT / 'scripts', target_is_directory=True)
            build = cwd / '.pio/build/sticky'
            build.mkdir(parents=True)
            image = build / 'crosscub-sticky-v1.6.5.4.bin'
            image.write_bytes(synthetic_image())
            (build / 'firmware.elf').write_bytes(b'PRIVATE_ELF_CANARY')
            (cwd / 'release-build.log').write_bytes(b'PRIVATE_LOG_CANARY')
            result = self.run_step('Validate exact OTA image before artifact upload', cwd,
                                   RELEASE_VERSION='1.6.5.4')
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual({p.name for p in (cwd / 'release-qualified').iterdir()},
                             {image.name, 'ota-validation.json'})
            image.write_bytes(b'SYNTHETIC INVALID IMAGE - NOT FIRMWARE')
            result = self.run_step('Validate exact OTA image before artifact upload', cwd,
                                   RELEASE_VERSION='1.6.5.4')
            self.assertNotEqual(result.returncode, 0)

    def test_failed_build_diagnostics_are_private_and_recipient_sealed(self):
        workflow = steps()
        self.assertIn('Require recipient-encrypted artifact retention', workflow)
        recipient = workflow['Require recipient-encrypted artifact retention']
        self.assertIn('vars.CROSSCUB_ARTIFACT_RECIPIENT_CERT', recipient)
        self.assertIn('umask 077', recipient)
        build = workflow['Build Crosscub Sticky']
        self.assertIn('id: firmware', build)
        self.assertNotIn('tee', build)
        stage = workflow['Stage private release evidence']
        seal = workflow['Seal private release evidence']
        for step in [stage, seal]:
            self.assertIn("always() && steps.recipient.outcome == 'success' && steps.firmware.outcome != 'skipped'", step)
        upload = workflow['Retain encrypted release evidence']
        self.assertIn("always() && steps.seal.outcome == 'success'", upload)
        self.assertIn('release-sealed/sealed.envelope', upload)
        self.assertIn('release-sealed/receipt.json', upload)
        self.assertIn('retention-days: 31', upload)
        self.assertIn('compression-level: 0', upload)
        public = workflow['Upload qualified firmware artifact']
        self.assertNotIn('always()', public)
        self.assertIn('release-qualified/crosscub-sticky-v${{ needs.prepare-release.outputs.release_version }}.bin', public)
        self.assertIn('release-qualified/ota-validation.json', public)
        with tempfile.TemporaryDirectory(prefix='synthetic-sealed-release-') as tmp:
            cwd = Path(tmp)
            (cwd / 'scripts').symlink_to(ROOT / 'scripts', target_is_directory=True)
            tools = cwd / 'tools'; tools.mkdir()
            pio = tools / 'pio'
            pio.write_text('#!/bin/bash\nif [[ "$1" == run ]]; then echo PRIVATE_LOG_CANARY; exit 23; fi\necho SYNTHETIC_TOOL_METADATA\n')
            pio.chmod(0o700)
            env = dict(PATH=str(tools) + os.pathsep + os.environ['PATH'], RUNNER_TEMP=str(cwd),
                       GITHUB_REPOSITORY='tobiasweissert/Crosscub', GITHUB_SHA='a'*40,
                       GITHUB_RUN_ID='1', GITHUB_RUN_ATTEMPT='1')
            generated = subprocess.run(['openssl', 'req', '-x509', '-newkey', 'rsa:3072',
                '-nodes', '-keyout', str(cwd / 'synthetic.key'), '-out', str(cwd / 'synthetic.pem'),
                '-days', '32', '-subj', '/CN=SYNTHETIC-NOT-PRODUCTION'], capture_output=True)
            self.assertEqual(generated.returncode, 0)
            cert = (cwd / 'synthetic.pem').read_text()  # Public synthetic certificate only.
            result = self.run_step('Require recipient-encrypted artifact retention', cwd,
                                   **env, RECIPIENT_CERT='not a certificate')
            self.assertNotEqual(result.returncode, 0)
            result = self.run_step('Require recipient-encrypted artifact retention', cwd,
                                   **env, RECIPIENT_CERT=cert)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = self.run_step('Build Crosscub Sticky', cwd, **env)
            self.assertEqual(result.returncode, 23)
            self.assertNotIn(b'PRIVATE_LOG_CANARY', result.stdout + result.stderr)
            builddir = cwd / '.pio/build/sticky'; builddir.mkdir(parents=True)
            expected = {'firmware.elf': b'PRIVATE_ELF_CANARY', 'firmware.map': b'PRIVATE_MAP_CANARY',
                        'firmware.bin': b'SYNTHETIC NOT FIRMWARE', 'bootloader.bin': b'BOOT_CANARY',
                        'partitions.bin': b'PARTITION_CANARY'}
            for name, data in expected.items():
                (builddir / name).write_bytes(data)
            for name in ['Stage private release evidence', 'Seal private release evidence']:
                result = self.run_step(name, cwd, **env)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotIn(b'PRIVATE_', result.stdout + result.stderr)
            sealed = cwd / 'release-sealed'
            self.assertEqual({p.name for p in sealed.iterdir()}, {'sealed.envelope', 'receipt.json'})
            for path in sealed.iterdir():
                self.assertNotIn(b'PRIVATE_', path.read_bytes())
            result = subprocess.run(['python3', str(ROOT / 'scripts/seal_ci_artifacts.py'), 'unseal',
                '--envelope', str(sealed / 'sealed.envelope'), '--receipt', str(sealed / 'receipt.json'),
                '--cert', str(cwd / 'synthetic.pem'), '--key', str(cwd / 'synthetic.key'),
                '--output', str(cwd / 'recovered')], capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            for name, data in expected.items():
                self.assertEqual((cwd / 'recovered' / name).read_bytes(), data)
            self.assertIn(b'PRIVATE_LOG_CANARY', (cwd / 'recovered/release-build.log').read_bytes())
            # Metadata failure must not discard bytes already staged for failed builds.
            self.assertTrue((cwd / 'recovered/provenance.log').exists())


if __name__ == '__main__':
    unittest.main()
