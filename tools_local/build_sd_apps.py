#!/usr/bin/env python3
"""Build independent S3 ELF app packages; never builds or flashes host firmware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ALLOWED_IMPORTS = {'memcpy', 'memmove', 'memset', 'memcmp', 'strlen', 'strcmp', 'strncmp'}

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--toolchain', type=Path, default=Path(os.environ.get('PLATFORMIO_CORE_DIR', str(Path.home()/'.platformio')))/'packages/toolchain-xtensa-esp-elf/bin')
    args = parser.parse_args()
    compiler = args.toolchain/'xtensa-esp32s3-elf-gcc'
    stripper = args.toolchain/'xtensa-esp32s3-elf-strip'
    readelf = args.toolchain/'xtensa-esp32s3-elf-readelf'
    receipt = {'compiler': subprocess.check_output([str(compiler),'--version'], text=True).splitlines()[0], 'apps': {}}
    for app, title in [('browser','Browser'),('chatgpt','ChatGPT')]:
        folder = args.output/'apps'/app
        folder.mkdir(parents=True, exist_ok=True)
        image = folder/'app.elf'
        command = [str(compiler), '-std=c11', '-Os', '-Wall', '-Wextra', '-Werror', '-fPIC', '-mlongcalls', '-mtext-section-literals',
                   '-fvisibility=hidden', '-ffunction-sections', '-fdata-sections', '-nostdlib', '-shared', '-Wl,-e,app_main,--gc-sections',
                   '-I'+str(ROOT/'src/apps_local/sdapps'), str(ROOT/'sd-apps'/app/'app.c'), '-o', str(image)]
        subprocess.run(command, check=True)
        subprocess.run([str(stripper),'--strip-unneeded','--remove-section=.comment','--remove-section=.dynamic',str(image)], check=True)
        symbols = subprocess.check_output([str(readelf),'-Ws',str(image)],text=True)
        imports = set()
        for line in symbols.splitlines():
            fields = line.split()
            if len(fields)>=8 and fields[6]=='UND':
                imports.add(fields[7])
        if imports-ALLOWED_IMPORTS:
            raise SystemExit('Unsupported imports: '+', '.join(sorted(imports-ALLOWED_IMPORTS)))
        digest = hashlib.sha256(image.read_bytes()).hexdigest()
        (folder/'app.cfg').write_text(f'CROSSPLAY-APP 1\nid={app}\ntitle={title}\ntarget=esp32s3\nabi=2\nimage=app.elf\nsha256={digest}\n')
        (folder/'symbols.txt').write_text(symbols)
        inputs = [ROOT/'sd-apps'/app/'app.c', ROOT/'sd-apps/common/Text.h', ROOT/'src/apps_local/sdapps/AppAbi.h', Path(__file__).resolve()]
        receipt['apps'][app] = {'image_bytes':image.stat().st_size, 'sha256':digest, 'imports':sorted(imports), 'command':command,
                                'inputs':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}}
    (args.output/'module-build.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':
    main()
