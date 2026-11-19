#!/usr/bin/env python3
"""Exact preservation guard for legacy method bodies (only SDK HalFile rename allowed)."""
from pathlib import Path
import hashlib, json, re
root=Path(__file__).resolve().parents[3]
s=(root/'src/activities/boot_sleep/SleepActivity.cpp').read_text()
pins={'renderCustomSleepScreen': 'fe916c1a1e93a13b5b2d79d157883ff5514dd8863b6a0b7cc2913d5c61440514', 'renderDefaultSleepScreen': 'c7cc1598cb93794eba7e19d799733cb766d99880929d53cf248a1438a7e6a64b', 'renderBitmapSleepScreen': '2e59b1a03067c015bc34af2581db16f70cf2e6b2bcf27cf35a374c0cfc49a494', 'renderReadingStatsSleepScreen': '9746de30ca1ea7c99c273e78ef9063853684733f4f1b945e9b292b18a6a0ae42', 'renderMinimalSleepScreen': '59e4b4c193b454f4faa50b7aa910984e6a8a874e588f730c0585477fa8abefb8', 'renderMinimalStatsSleepScreen': 'f693cf9df5c98eb95dc03bccfda37bc018971fe5b2bd45f412238028ed9cf507', 'renderDashboardSleepScreen': '9730102a96d706a7df89061ecd8ef03eab54db7004e1cad8322e0ec490a5944d', 'renderLastScreenSleepScreen': '1b6830c9c29438ac1abe1e7074652e4a033055270f381d6c2eb2361562330ac8', 'renderBlankSleepScreen': '04288e483143c0ab0b385129291e690ea7c9b529b5a8b14bb3c56b6f0f2b1c95', 'renderOverlaySleepScreen': '652cd0c8d55eebba46c122acdb8eea7a449b29e3826ce56aa9f78361b7896e2b'}
for name,want in pins.items():
    match=re.search(r'(?:void|bool) SleepActivity::'+name+r'\(',s)
    assert match, name+' missing'
    start=match.start()
    end=s.find('\n}\n',start)+3
    got=hashlib.sha256(s[start:end].replace('FsFile','HalFile').encode()).hexdigest()
    assert got==want, name+' legacy body changed'
print('preservation: '+str(len(pins))+' exact legacy method bodies PASS')
