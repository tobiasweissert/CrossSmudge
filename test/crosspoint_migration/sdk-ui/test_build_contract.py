#!/usr/bin/env python3
"""Bounded build/source invariants, separate from target compilation."""
import ast
import configparser
import os
from pathlib import Path
import re
import subprocess
ROOT=Path(__file__).resolve().parents[3]
CAN=os.environ.get('CROSSCUB_CANONICAL_REPO','/root/github/crosssmudge')
def blob(p,side):return subprocess.check_output(['git','-C',CAN,'show',f'refs/audit/crosspoint-rebase/{side}:{p}'])
def main():
    c=configparser.ConfigParser(interpolation=None);c.read(ROOT/'platformio.ini')
    assert c['platformio']['default_envs']=='sticky'
    assert '/55.03.311/' in c['base']['platform']
    assert c['base']['board_upload.maximum_size']=='6553600'
    assert set(s for s in c.sections() if s.startswith('env:'))=={'env:sticky','env:sticky-debug','env:sticky-owner-test','env:sticky-simulator','env:default','env:debug','env:simulator'}
    for section in ['base','simulator-base']:assert '-DCROSSPOINT_VERSION=CROSSINK_VERSION' in c[section]['build_flags']
    deps=c['base']['lib_deps'];flags=c['base']['build_flags'];scripts=c['base']['extra_scripts']
    for feature in ['BleKeyboardHost','FreeInkFont','ContentProtection','JsonSax','Opds','CatalogList','NearbyTransfer','AnimatedGIF','SdFat @ 2.3.1']:
        assert feature in deps or feature=='BleKeyboardHost' and 'BleKeyboardHost' in c['ble_page_turner']['lib_deps'],feature
    for flag in ['USE_SEPARATE_FAT_CACHE=1','FREEINK_CONTENT_WOLFSSL=1','FREEINK_FONT_ENABLE_AUTOHINT=1','HAVE_MAX_FRAGMENT','--wrap=esp_mbedtls_mem_calloc']:
        assert flag in flags,flag
    for script in ['patch_sdfat.py','patch_animatedgif.py','patch_pioarduino_cache.py','check_firmware_size.py','patch_websockets.py','build_html.py','build_web.py']:
        assert script in scripts,script
    patches=list((ROOT/'scripts/sdfat_patches').glob('*.patch'));assert len(patches)==3
    for p in patches:assert p.read_bytes()==blob(str(p.relative_to(ROOT)),'upstream')
    names=['build_html.py','convert_icon.py','debugging_monitor.py','gen_i18n.py','generate-font-manifest.py','generate_br_section_break_epub.py','git_branch.py','patch_wolfssl.py']
    for name in names:ast.parse((ROOT/'scripts'/name).read_text(),filename=name)
    header=(ROOT/'src/components/icons/listIcons.h').read_text()
    present=set(re.findall(r'static const freeink::Icon (\w+)\s*=',header))
    for side in ['owner','upstream']:
        expected=set(re.findall(r'static const freeink::Icon (\w+)\s*=',blob('src/components/icons/listIcons.h',side).decode()))
        assert expected<=present,(side,sorted(expected-present))
    for name,body in re.findall(r'static const uint8_t (icon_\w+)\[\]\s*=\s*\{(.*?)\};',header,re.S):
        match=re.search(r'_(\d+)_bits$',name); assert match,name
        width=int(match[1]);assert len(re.findall(r'0x[\da-fA-F]{2}',body))==((width+7)//8)*width,name
    # Owner wrappers retain error propagation and do not regress into upstream's bare-new allocation.
    storage=(ROOT/'lib/hal/HalStorage.cpp').read_text()
    for marker in ['std::malloc(sizeof(Impl))','iterationFailed_ = true','allocationFailed_ = true','bool HalFile::close()','bool HalFile::sync()']:
        assert marker in storage,marker
    print('PASS: Sticky-only profiles/slot gate, platform 55.03.311, BLE/font/plugin/content dependencies, three exact SdFat patches, script syntax, union icon inventory/planes, storage error interfaces')
if __name__=='__main__':main()
