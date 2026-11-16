#!/usr/bin/env python3
"""Bounded production-method pixel and callback regressions, never a catalogue parity claim."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib

from compare_battleship import compare,read_png

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
REGISTRY = {
    'battleship.lua': 'cp-battleship',
    'battleship_report.lua': 'cp-battleship',
    'battleship_footer.lua': 'cp-battleship',
    'battleship_nonsticky.lua': 'cp-battleship',
    'battleship_placement.lua': 'cp-battleship',
    'battleship_capture_placement.lua': 'cp-battleship',
    'battleship_partial.lua': 'cp-battleship',
    'battleship_buttons.lua': 'cp-battleship',
    'battleship_focus.lua': 'cp-battleship',
    'battleship_menu.lua': 'cp-battleship',
    'battleship_records.lua': 'cp-battleship',
    'battleship_cycles.lua': 'cp-battleship',
    'battleship_new_game.lua': 'cp-battleship',
    'battleship_capture_results.lua': 'cp-battleship',
    'battleship_targets.lua': 'cp-battleship',
    'battleship_capture_aim.lua': 'cp-battleship',
    'battleship_keyboard_aim.lua': 'cp-battleship',
    'battleship_import_own_view.lua': 'cp-battleship',
    'solitaire.lua': 'solitaire',
    'solitaire_capture.lua': 'solitaire',
    'solitaire_selection.lua': 'solitaire',
    'solitaire_fans.lua': 'solitaire',
    'solitaire_menu.lua': 'solitaire',
    'solitaire_records.lua': 'solitaire',
    'solitaire_mapped.lua': 'solitaire',
    'sudoku.lua': 'sudoku',
    'wordle.lua': 'wordle',
    'wordle_capture.lua': 'wordle',
}
TRACE = r'''
assert(smudge.get_device()=='sticky','compiled production Sticky identity failed')
assert(math.maxinteger==2147483647 and io==nil and package==nil and debug==nil,'unexpected production library/VM configuration')
local originalWrite=smudge.write_file
local trace={}
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do
 local original=smudge[name]
 smudge[name]=function(...)
  if name=='clear'then trace={}end -- Trace the captured production frame, not every discarded repaint.
  local args={...};local out={name}
  for i=1,#args do out[#out+1]=name=='bitmap' and i==5 and ('bytes:'..#args[i])or tostring(args[i])end
  trace[#trace+1]=table.concat(out,'\t')
  return original(...)
 end
end
local snapshot=capture
capture=function(path)
 assert(originalWrite(path..'.trace',table.concat(trace,'\n')..'\n',false))
 snapshot(path);trace={}
end
'''
BATTLE_STATE = r'''
local M=dofile('core.lua');local g=M.new(123456)
for row=1,5 do assert(M.action(g,1,row))end
for _,i in ipairs({14,46,78,21,88,35})do
 if g.turn==1 then assert(M.fire(g,i));assert(M.fire(g,(i+10)%100+1))end
end
assert(smudge.write_file('state-map.txt',table.concat({g.a,g.b,g.sa,g.sb,tostring(g.turn),tostring(g.last)},'\n'),false))
assert(dofile('storage.lua').save('1|'..M.encode(g)))
dofile('main.lua');on_init();on_draw();on_button('confirm',true);on_draw();capture('gameplay.pgm');on_exit()
'''


def png(source, destination):
    _, dimensions, maximum, pixels = source.read_bytes().split(b'\n', 3)
    width, height = map(int, dimensions.split())
    assert maximum == b'255' and len(pixels) == width * height

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))

    destination.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0)) +
                            chunk(b'IDAT', zlib.compress(b''.join(b'\0' + pixels[y*width:(y+1)*width]
                                                                 for y in range(height)))) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--skip-build', action='store_true', help='Reuse this output directory tooling only')
    parser.add_argument('--tooling', type=Path, help='Reuse shared compiled host tools with --skip-build')
    parser.add_argument('--apps-root', type=Path, default=ROOT/'apps', help='Extracted SD applications directory')
    parser.add_argument('--only-app', action='append', choices=sorted(set(REGISTRY.values())))
    args = parser.parse_args()
    apps_root = args.apps_root.resolve()
    selected = set(args.only_app or REGISTRY.values())
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    tooling = (args.tooling or output/'tooling').resolve()
    if not args.skip_build:
        subprocess.run(['python3', str(HERE/'build_capture.py'), '--output', str(tooling)], check=True)
        subprocess.run(['python3', str(HERE/'build_native.py'), '--reference', str(args.reference),
                        '--output', str(tooling)], check=True)
        subprocess.run(['python3', str(HERE/'build_solitaire_reference.py'), '--reference', str(args.reference),
                        '--output', str(tooling)], check=True)
    results = []

    def execute(work, fixture, label, width, height):
        copied_inputs = {str(path.relative_to(work)): hashlib.sha256(path.read_bytes()).hexdigest()
                         for path in sorted(work.rglob('*')) if path.is_file()}
        body = TRACE + fixture + "\ncapture('final.pgm')\n"
        (work/'test-fixture.lua').write_text(body)
        result = subprocess.run([str(tooling/'capture-runner'), 'test-fixture.lua', '2097152', str(width), str(height)],
                                cwd=work, text=True, capture_output=True, timeout=180)
        destination = output/'captures'/label/f'{width}x{height}'
        destination.mkdir(parents=True, exist_ok=True)
        record = dict(test=label, width=width, height=height, returncode=result.returncode,
                      stdout=result.stdout, stderr=result.stderr, process_returncode=result.returncode, app_inputs=copied_inputs)
        if result.stdout.strip():
            try:
                record['metrics'] = json.loads(result.stdout.strip().splitlines()[-1])
                if record['metrics'].get('after_close') != 0 or record['metrics'].get('peak_bytes', 2097153) > 2097152:
                    record['returncode'] = 1
            except json.JSONDecodeError:
                pass
        results.append(record)
        for path in sorted(work.iterdir()):
            if path.suffix == '.pgm':
                png(path, destination/(path.stem+'.png'))
            elif path.suffix in ('.trace', '.txt', '.fleet', '.state'):
                shutil.copyfile(path, destination/path.name)
        print(label, width, height, 'PASS' if result.returncode == 0 else 'EXPECTED FAILURE' if label == 'unsupported-font-guard' else result.stderr, flush=True)
        return destination, record

    with tempfile.TemporaryDirectory(prefix='fixture-', dir=output) as temporary:
        scratch = Path(temporary)
        for name, app in REGISTRY.items():
            if app not in selected:
                continue
            for width, height in ((480, 800), (800, 480)):
                work = scratch/(name+str(width))
                shutil.copytree(apps_root/app, work)
                destination, record = execute(work, (HERE/name).read_text(), name, width, height)
                if record['returncode'] == 0 and width == 800 and name == 'solitaire_mapped.lua':
                    for state in sorted(work.glob('solitaire-*.state')):
                        kind = 'menu' if state.stem == 'solitaire-menu' else 'win' if state.stem == 'solitaire-won' else 'board'
                        native = destination/(state.stem+'-native.pgm')
                        reference = subprocess.run([str(tooling/'solitaire-reference'), kind, str(width), str(height), str(state), str(native)], capture_output=True, text=True)
                        item = dict(test=name+':'+state.stem+'-native', returncode=reference.returncode, stderr=reference.stderr)
                        if reference.returncode == 0:
                            png(native, native.with_suffix('.png'))
                            w,h,a = read_png(native.with_suffix('.png'))
                            ww,hh,b = read_png(destination/(state.stem+'.png'))
                            assert (w,h) == (ww,hh)
                            item['differences'] = {'whole_frame': sum(x != y for x,y in zip(a,b))}
                            if item['differences']['whole_frame'] != 0:
                                item['returncode'] = 1
                        results.append(item)
                if record['returncode'] == 0 and width == 480 and name in ('battleship_capture_results.lua', 'battleship_capture_aim.lua'):
                    for state in sorted(work.glob('battle-*.state')):
                        native = destination/(state.stem+'-native.pgm')
                        reference = subprocess.run([str(tooling/'native-reference'), str(width), str(height), str(state), str(native)], capture_output=True, text=True)
                        item = dict(test=name+':'+state.stem+'-native', returncode=reference.returncode, stderr=reference.stderr)
                        if reference.returncode == 0:
                            png(native, native.with_suffix('.png'))
                            item['differences'] = compare(native.with_suffix('.png'), destination/(state.stem+'.png'))
                            if item['differences']['whole_frame'] != 0:
                                item['returncode'] = 1
                        results.append(item)
        for label, revision in ([('battleship-before', '621719e4'), ('battleship-after', None)] if 'cp-battleship' in selected else []):
            for width, height in ((480, 800), (800, 480)):
                work = scratch/(label+str(width))
                if revision:
                    work.mkdir()
                    archive = subprocess.check_output(['git', 'archive', revision, 'apps/cp-battleship'], cwd=ROOT)
                    subprocess.run(['tar', '-x', '--strip-components=2', '-C', str(work)], input=archive, check=True)
                else:
                    shutil.copytree(apps_root/'cp-battleship', work)
                destination, record = execute(work, BATTLE_STATE, label, width, height)
                if record['returncode'] == 0 and label == 'battleship-after':
                    native = destination/'native.pgm'
                    native_result = subprocess.run([str(tooling/'native-reference'), str(width), str(height),
                                                    str(work/'state-map.txt'), str(native)],
                                                   capture_output=True, text=True)
                    results.append(dict(test='native-battleship', width=width, height=height,
                                        returncode=native_result.returncode, stdout=native_result.stdout,
                                        stderr=native_result.stderr))
                    if native_result.returncode == 0:
                        png(native, destination/'native.png')
                        if width == 480:
                            differences = compare(destination/'native.png', destination/'gameplay.png')
                            (destination/'comparison.json').write_text(json.dumps(differences, indent=2)+'\n')
                            results.append(dict(test='native-battleship-portrait-pixels', differences=differences,
                                                returncode=int(any(differences.values()))))
        # A fallback text no-op would silently approve missing font pixels; require an actual error.
        work = scratch/'fail-closed'
        work.mkdir()
        _, guard = execute(work, "smudge.text(0,0,'unmodelled font must fail')", 'unsupported-font-guard', 480, 800)
        guard['expected_failure'] = True
        guard['returncode'] = 0 if guard['returncode'] != 0 and 'unmodeled stock API called by test' in guard['stderr'] else 1
    payload = {'scope': 'Actual production shape/bitmap methods and compiled Sticky identity, actual app callbacks/fonts. '
                        'Named mapped Battleship portrait and Solitaire landscape frames require full zero pixel differences. '
                        'Other orientations are captured bounded adaptations. No all-catalogue or physical acceptance.',
               'results': results,
               'app_hashes': {app: {str(path.relative_to(apps_root/app)): hashlib.sha256(path.read_bytes()).hexdigest()
                                    for path in sorted((apps_root/app).rglob('*')) if path.is_file()}
                              for app in sorted(selected)},
               'source_hashes': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in HERE.iterdir() if path.suffix in ('.py', '.lua', '.md')}}
    (output/'results.json').write_text(json.dumps(payload, indent=2)+'\n')
    assert all(item['returncode'] == 0 for item in results), 'See results.json for exact failures'


if __name__ == '__main__':
    main()
