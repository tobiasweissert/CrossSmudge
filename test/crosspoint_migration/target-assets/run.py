#!/usr/bin/env python3
"""Bounded real-header regression: no downloads, target build or API doubles."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]
PNG = Path('/root/github/.host-data/crosssmudge/ota-libdeps/sticky/PNGdec/src')
SDK = ROOT / 'freeink-sdk/libs/ui/FreeInkUI/include'
ASSETS = [
    ('9ac3475733e677b7f61ba3f6e8ce41ce1b9b2967', 'src/components/icons/book.h', 'BookIcon', 128),
    ('818b0befac4d9249c7abded67fa980187c4b41af', 'src/components/icons/book24.h', 'Book24Icon', 72),
    ('818b0befac4d9249c7abded67fa980187c4b41af', 'src/images/LoadingIcon.h', 'LoadingIcon', 288),
]


def git_blob(ref, path):
    return subprocess.check_output(['git', '-C', str(ROOT), 'show', f'{ref}:{path}'])


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=['assets', 'png', 'png-preserve'], required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--phase', required=True)
    args = parser.parse_args()
    args.evidence.mkdir(parents=True, exist_ok=True)
    stem = args.evidence / f'{args.case}-{args.phase}'
    source = stem.with_suffix('.cpp')
    rows = []
    if args.case == 'assets':
        body = ''.join(f'#include "{p[4:]}"\n' for _, p, _, _ in ASSETS)
        body += ''.join(f'static_assert(sizeof({name}) == {size});\n' for _, _, name, size in ASSETS)
        body += 'static_assert(LOADINGICON_WIDTH == 48 && LOADINGICON_HEIGHT == 48);\n'
        for ref, path, name, size in ASSETS:
            original = git_blob(ref, path)
            array = re.search(r'\b' + name + r'\[\]\s*=\s*\{([^}]+)\}', original.decode()).group(1)
            pixels = bytes(int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', array))
            assert len(pixels) == size
            rows.append({'path': path, 'source_commit': ref,
                         'source_blob': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', f'{ref}:{path}'], text=True).strip(),
                         'header_sha256': sha(original), 'array': name,
                         'pixel_bytes': size, 'pixel_sha256': sha(pixels)})
    else:
        sleep = (ROOT / 'src/activities/boot_sleep/SleepActivity.cpp').read_text()
        # Compile precisely the application's PNG include and its local isolation,
        # not a separately maintained implementation of the proposed fix.
        lines = sleep.splitlines()
        index = lines.index('#include <PNGdec.h>')
        start = index
        if index > 1 and lines[index - 2] == '#pragma push_macro("local")':
            start = index - 2
        stop = index + 1
        if stop < len(lines) and lines[stop] == '#pragma pop_macro("local")':
            stop += 1
        boundary = '\n'.join(lines[start:stop])
        body = boundary + '\n#ifdef local\n#error PNGdec leaked local into application headers\n#endif\n'
        if args.case == 'png-preserve':
            body = '#define local 73\n' + boundary
            body += '\n#if local != 73\n#error PNGdec changed an existing application macro\n#endif\n#undef local\n'
        body += '#include "components/media/catalog.h"\n'
        body += 'static_assert(sizeof(PNG) > 0);\n'
        body += 'int main() { freeink::ui::CatalogWindow window; window.sync(5, 2); window.next(); return window.firstIndex != 2; }\n'
    source.write_text(body)
    command = ['g++', '-std=c++17', '-D__LINUX__', '-Wall', '-Wextra', '-Werror',
               '-I' + str(ROOT / 'src'), '-I' + str(PNG), '-I' + str(SDK),
               '-fsyntax-only', '-MMD', '-MF', str(stem.with_suffix('.d')), str(source)]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    stem.with_suffix('.log').write_text(result.stdout)
    provenance_ok = True
    if args.case == 'assets' and result.returncode == 0:
        provenance_ok = all((ROOT / p).read_bytes() == git_blob(ref, p) for ref, p, _, _ in ASSETS)
    deps = stem.with_suffix('.d')
    inputs = {}
    if deps.exists():
        for token in deps.read_text().replace('\\\n', '').split()[1:]:
            path = Path(token)
            if path.is_file():
                inputs[str(path)] = sha(path.read_bytes())
    report = {'command': command, 'compiler': shutil.which('g++'),
              'compiler_version': subprocess.check_output(['g++', '--version'], text=True).splitlines()[0],
              'exit': result.returncode, 'provenance_ok': provenance_ok,
              'assets': rows, 'inputs_sha256': inputs,
              'scope': 'host compiler, real PNGdec and pinned SDK UI headers; not target firmware compilation'}
    stem.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'case': args.case, 'phase': args.phase, 'exit': result.returncode, 'provenance_ok': provenance_ok}))
    print(result.stdout)
    return result.returncode or (0 if provenance_ok else 1)


if __name__ == '__main__':
    sys.exit(main())
