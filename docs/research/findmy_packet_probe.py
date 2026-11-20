#!/usr/bin/env python3
"""Offline Find My beacon-format feasibility probe. No BLE or network access.

Synthetic byte vectors only: this checks framing/reconstruction, not elliptic-curve
validity, Apple report acceptance, key security, or target firmware compatibility.
The wire layout was inspected in OpenHaystack ea05dadb9e6e71075ef08afb5c2087302fbb7210.
No upstream firmware is imported; this is not a deployable beacon.
"""
import json
import unittest


def frame(public_advertisement_key):
    if not isinstance(public_advertisement_key, bytes) or len(public_advertisement_key) != 28:
        raise ValueError('Expected exactly28 advertisement-key bytes')
    key = public_advertisement_key
    address = bytes([key[0] | 0xC0]) + key[1:6]
    advertisement = bytes.fromhex('1e ff 4c 00 12 19 00') + key[6:] + bytes([key[0] >> 6, 0])
    return address, advertisement


def recover(address, advertisement):
    if len(address) != 6 or len(advertisement) != 31:
        raise ValueError('Invalid frame lengths')
    if advertisement[:7] != bytes.fromhex('1e ff 4c 00 12 19 00'):
        raise ValueError('Unexpected frame header')
    if address[0] & 0xC0 != 0xC0 or advertisement[29] > 3:
        raise ValueError('Invalid address/key high bits')
    return bytes([(address[0] & 0x3F) | advertisement[29] << 6]) + address[1:] + advertisement[7:29]


class WireFormat(unittest.TestCase):
    def test_legacy_advertising_budget(self):
        address, payload = frame(bytes(range(28)))
        self.assertEqual(len(address), 6)
        self.assertEqual(len(payload), 31)
        self.assertEqual(payload[0], len(payload) - 1)
        self.assertEqual(payload[2:4], bytes.fromhex('4c00'))
        # There is no room to blindly append a standard three-byte Flags AD.
        self.assertGreater(len(payload) + 3, 31)

    def test_complete_first_byte_space_round_trips(self):
        for first in range(256):
            key = bytes([first]) + bytes(range(1, 28))
            with self.subTest(first=first):
                address, payload = frame(key)
                self.assertEqual(recover(address, payload), key)

    def test_key_length_is_literal(self):
        for bad in [b'', bytes(27), bytes(29), '0' * 28]:
            with self.assertRaises(ValueError):
                frame(bad)

    def test_truncated_or_wrong_frames_rejected(self):
        address, payload = frame(bytes(range(28)))
        for wrong_address, wrong_payload in [(address[:5], payload), (address, payload[:30]),
                                             (address, bytes(31)), (bytes(6), payload)]:
            with self.assertRaises(ValueError):
                recover(wrong_address, wrong_payload)


if __name__ == '__main__':
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(WireFormat))
    print(json.dumps({'passed': result.wasSuccessful(), 'tests': result.testsRun,
                      'scope': 'synthetic offline packet-format checks only',
                      'radio_transmissions': 0, 'apple_requests': 0, 'real_keys_used': False}))
    raise SystemExit(not result.wasSuccessful())
