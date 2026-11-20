#!/usr/bin/env python3
"""Configuration admission checks, not firmware compilation or power evidence."""
import configparser
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[3]

class ExperimentalProfileTest(unittest.TestCase):
    def test_tracking_profile_is_separate_and_requests_real_sdk_configuration(self):
        config = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(';',))
        config.read(ROOT / 'platformio.ini')
        self.assertIn('env:sticky-findmy', config, 'experimental target is not implemented')
        profile = config['env:sticky-findmy']
        self.assertEqual(profile['extends'], 'env:sticky')
        self.assertIn('-DCROSSCUB_FINDMY_EXPERIMENTAL=1', profile['build_flags'])
        self.assertIn('-DFINDMY_CRYPTO_MBEDTLS=1', profile['build_flags'])
        settings = profile['custom_sdkconfig'].splitlines()
        for value in ['CONFIG_BT_ENABLED=y', 'CONFIG_BT_NIMBLE_ENABLED=y',
                      'CONFIG_BT_CONTROLLER_ENABLED=y', 'CONFIG_PM_ENABLE=y',
                      'CONFIG_FREERTOS_USE_TICKLESS_IDLE=y',
                      'CONFIG_BT_CTRL_MODEM_SLEEP=y',
                      'CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y',
                      'CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP=y']:
            self.assertIn(value, settings)
        self.assertNotIn('CROSSCUB_FINDMY_EXPERIMENTAL', config['env:sticky']['build_flags'])
        self.assertEqual(config['env:default']['extends'], 'env:sticky')

if __name__ == '__main__':
    unittest.main()
