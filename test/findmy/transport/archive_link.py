#!/usr/bin/env python3
"""Ensure static archive pulls strong retention override despite Arduino weak one."""
from pathlib import Path
import os
import subprocess
ROOT=Path(__file__).resolve().parents[3]
HERE=Path(__file__).resolve().parent
OUT=Path('/root/.hermes/task-state/crosscub-findmy-implementation/transport')
OUT.mkdir(parents=True,exist_ok=True)
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-O0',
 '-DCROSSCUB_FINDMY_EXPERIMENTAL=1','-DARDUINO_ARCH_ESP32=1','-DCONFIG_IDF_TARGET_ESP32S3=1',
 '-DCROSSCUB_FINDMY_TEST_WEAK_BT_BOOT=1','-I'+str(HERE/'stubs'),'-I'+str(ROOT/'lib/HalFindMyBle')]
if os.environ.get('SANITIZE') == '1':
 flags += ['-fsanitize=undefined','-fno-sanitize-recover=all']
for name in ['HalFindMyBle','EarlyBtRetention']:
 subprocess.run(flags+['-c',str(ROOT/f'lib/HalFindMyBle/{name}.cpp'),'-o',str(OUT/f'{name}.o')],check=True)
subprocess.run(['ar','rcs',str(OUT/'libtransport.a'),str(OUT/'HalFindMyBle.o'),str(OUT/'EarlyBtRetention.o')],check=True)
subprocess.run(flags+[str(HERE/'transport_test.cpp'),str(OUT/'libtransport.a'),'-o',str(OUT/'test-archive')],check=True)
subprocess.run([str(OUT/'test-archive'),'sync'],check=True)
print('PASS archive retention override survives existing weak Arduino symbol')
