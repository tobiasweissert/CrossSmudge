#!/usr/bin/env python3
"""Compile real capability/backend guards, including an SDK-defined zero board macro."""
from pathlib import Path
import os,subprocess,tempfile,json
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='airdrop-capability-',dir=os.environ.get('TMPDIR')) as tmp:
 p=Path(tmp);(p/'BoardConfig.h').write_text('#pragma once\n#define FREEINK_CAP_TOUCH CROSSSMUDGE_APP_CAP_TOUCH\n#define FREEINK_CAP_USB_MSC CROSSINK_APP_CAP_USB_DRIVE\n#ifndef FREEINK_DEVICE_STICKY\n#define FREEINK_DEVICE_STICKY 0\n#endif\n')
 for sticky in (None,0,1):
  for simulator in (False,True):
   for include_first in (False,True):
    expected=int(sticky==1 and not simulator)
    source=('#include <BoardConfig.h>\n' if include_first else '')+'#include <AppCapabilities.h>\nstatic_assert(CROSSINK_APP_CAP_AIRDROP=='+str(expected)+');\n'
    (p/'test.cpp').write_text(source)
    flags=['-DCROSSSMUDGE_APP_CAP_TOUCH=0','-DCROSSINK_APP_CAP_USB_DRIVE=0']
    if sticky is not None:flags+=['-DFREEINK_DEVICE_STICKY='+str(sticky)]
    if simulator:flags+=['-DSIMULATOR']
    cmd=['c++','-std=c++20',*flags,'-I'+str(p),'-I'+str(root/'include'),'-I'+str(root/'lib/AirDrop'),'-I'+str(root/'lib/miniz/src')]
    subprocess.run([*cmd,'-fsyntax-only',str(p/'test.cpp')],check=True)
    if not expected:
     for file in ('lib/AirDrop/port/ReceiverEsp32.cpp','lib/AirDrop/port/Discovery.cpp','lib/AirDrop/vendor/ESP32Drop/src/awdl/port/awdl_port_esp32.cpp','lib/AirDrop/storage/HalStorageSink.cpp'):
      obj=p/'guard.o';subprocess.run([*cmd,'-c',str(root/file),'-o',str(obj)],check=True)
      symbols=subprocess.check_output(['nm','-C',str(obj)],text=True)
      assert not any(x in symbols for x in ('awdl_begin','tcp_listen_task','mbedtls_ssl','esp_wifi','psa_crypto','HalStorageSink')),(file,symbols)
 print('PASS12 capability/include-order profiles; nonSticky/simulator backend objects retain no radio/TLS/SD backend symbols')
