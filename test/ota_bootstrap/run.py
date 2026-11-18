"""The one-time SD bootstrap must be visibly newer and use the owner OTA feed."""
from pathlib import Path
import configparser
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BootstrapVersion(unittest.TestCase):
    def test_distinct_numeric_release_identity(self):
        config = configparser.ConfigParser(interpolation=None)
        config.read(ROOT / 'platformio.ini')
        version = config['crosssmudge']['version']
        self.assertEqual(config['crossink']['version'], version)
        self.assertRegex(version, r'^[0-9]{1,9}(?:\.[0-9]{1,9}){0,3}$')
        self.assertLessEqual(len(version.encode('ascii')), 31)
        parts = tuple(int(part) for part in version.split('.'))
        self.assertGreaterEqual(parts + (0,) * (4 - len(parts)), (1, 6, 5, 4))
        self.assertIn('CROSSCUB_APP_VERSION=\\"${crosssmudge.version}\\"',
                      config['base']['build_flags'])
        producer = (ROOT / 'src/CrosscubAppDesc.c').read_text()
        self.assertIn('.version = CROSSCUB_APP_VERSION', producer)
        self.assertIn('.project_name = "Crosscub"', producer)

    def test_owner_feed_is_fixed_not_upstream(self):
        policy = (ROOT / 'src/network/CrosscubOtaPolicy.h').read_text()
        self.assertIn('https://api.github.com/repos/tobiasweissert/Crosscub/releases/latest', policy)
        self.assertIn('https://github.com/tobiasweissert/Crosscub/releases/download/', policy)
        self.assertIn('crosscub-v', policy)
        self.assertNotIn('api.github.com/repos/Mumfee/', policy)


if __name__ == '__main__':
    unittest.main()
