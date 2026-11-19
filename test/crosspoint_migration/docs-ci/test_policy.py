"""Portable source-policy checks; not actionlint or a firmware build."""
from pathlib import Path
import re
import subprocess
import shutil
import unittest

ROOT = Path(__file__).resolve().parents[3]


class Policy(unittest.TestCase):
    def test_actual_web_scripts(self):
        self.assertIsNotNone(shutil.which('node'), 'Node is required for the actual web-script contract probe')
        subprocess.run(['node', str(Path(__file__).with_name('web_contract.js'))], cwd=ROOT, check=True, timeout=20)

    def test_standard_build_archives_before_stats(self):
        text = (ROOT / '.github/workflows/ci.yml').read_text()
        self.assertIn('workflow_dispatch:', text)
        self.assertIn('pioarduino==6.1.19', text)
        self.assertIn('634a3f8c067a3714a6a7c622015138af096ab5d2', text)
        self.assertIn('55.03.311', text)
        self.assertIn('firmware.elf', text)
        self.assertIn('image-validation.json', text)
        self.assertIn('toolchain-manifest.json', text)
        self.assertLess(text.index('name: Seal exact Sticky evidence'), text.index('name: Retain encrypted Sticky evidence'))
        self.assertLess(text.index('name: Retain encrypted Sticky evidence'), text.index('name: Extract firmware stats'))
        self.assertLess(text.index('name: Retain encrypted Sticky evidence'),
                        text.index('name: Check portable docs and workflow policy'))
        self.assertNotIn('github.event.repository.private', text)
        self.assertIn('vars.CROSSCUB_ARTIFACT_RECIPIENT_CERT', text)
        self.assertIn("if: always() && steps.seal.outcome == 'success'", text)
        uploads = text[text.index('uses: actions/upload-artifact@'):text.index('      - name: Extract firmware stats')]
        upload_paths = re.search(r'(?m)^          path: \|\n((?:            .+\n)+)', uploads)
        self.assertIsNotNone(upload_paths)
        self.assertEqual([p.strip() for p in upload_paths[1].splitlines()],
                         ['sticky-sealed/sealed.envelope', 'sticky-sealed/receipt.json'])
        self.assertIn("if: always() && steps.recipient.outcome == 'success' && steps.firmware.outcome != 'skipped'", text)
        self.assertIn('retention-days: 30', uploads)
        self.assertNotIn('tee pio-sticky.log', text)
        self.assertNotIn('sticky-evidence/', uploads)
        self.assertNotIn('*.elf', uploads)
        self.assertNotIn('*.bin', uploads)
        self.assertNotIn('contents: write', text)
        self.assertNotIn('gh release', text)
        self.assertNotIn('secrets.RELEASE_PAT', text)

    def test_release_is_owner_sticky_and_guarded(self):
        text = (ROOT / '.github/workflows/release.yml').read_text()
        self.assertNotRegex(text, r'(?m)^[<=>]{7}')
        self.assertIn("github.repository == 'tobiasweissert/Crosscub'", text)
        self.assertIn('scripts/prepare_ota_release.py --ci-check', text)
        self.assertIn('scripts/validate_ota_release.py', text)
        self.assertIn('default: false', text)
        self.assertNotIn('device: x3-x4', text)
        self.assertNotIn('git push', text)
        self.assertNotIn('--clobber', text)

    def test_candidate_cannot_publish(self):
        text = (ROOT / '.github/workflows/release_candidate.yml').read_text()
        self.assertNotIn('contents: write', text)
        self.assertNotIn('action-gh-release', text)
        self.assertNotIn('publish-release-candidate:', text)


if __name__ == '__main__':
    unittest.main()
