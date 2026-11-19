#!/usr/bin/env python3
"""Run production Wordle callbacks, capture real fonts, and compare pinned native models when supplied."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT/'test/crossplay_ui_parity'))
from run import png, TRACE
from compare_battleship import read_png

MODELS = {
    'callbacks.lua': {'menu': 'wordle-menu-oct2', 'help': 'wordle-help', 'archive': 'wordle-archive-oct2'},
    'records.lua': {'menu-record': 'wordle-menu-record', 'archive-record': 'wordle-archive-record-oct2'},
    'gameplay.lua': {'wordle-'+state: 'wordle-oct2-'+state for state in ('blank', 'typed', 'alley', 'win')},
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--app-dir', type=Path, default=ROOT/'apps/wordle', help='Extracted SD app root for package qualification')
    parser.add_argument('--tooling', type=Path, help='Reuse canonical compiled production pixel runner')
    parser.add_argument('--native', type=Path, help='Original native catalogue renderer with Wordle model fixtures')
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    tooling = args.tooling.resolve() if args.tooling else args.output/'tooling'
    if not args.tooling:
        subprocess.run(['python3', str(ROOT/'test/crossplay_ui_parity/build_capture.py'), '--output', str(tooling)], check=True)
    results, comparisons = [], []
    fixtures = [(name, HERE/name) for name in ('callbacks.lua', 'records.lua', 'missing_bank.lua', 'failed_write.lua', 'long_history.lua')]
    fixtures.append(('gameplay.lua', ROOT/'test/crossplay_ui_parity/wordle_capture.lua'))
    with tempfile.TemporaryDirectory(prefix='wordle-ui-', dir=args.output) as temporary:
        for name, fixture in fixtures:
            for width, height in ((480, 800), (800, 480)):
                app = Path(temporary)/(name+str(width))
                shutil.copytree(args.app_dir, app)
                body = fixture.read_text()
                # Historical root gameplay fixture redundantly mocks the same identity.
                # This runner compiles the real binding, so execute the fixture with that binding intact.
                body = body.replace("smudge.get_device=function()return 'sticky'end\n", '')
                (app/'fixture.lua').write_text(TRACE+body)
                process = subprocess.run([str(tooling/'capture-runner'), 'fixture.lua', '2097152', str(width), str(height)],
                                         cwd=app, text=True, capture_output=True, timeout=180)
                row = dict(test=name, width=width, height=height, returncode=process.returncode,
                           stdout=process.stdout, stderr=process.stderr,
                           fixture_sha256=hashlib.sha256((TRACE+body).encode()).hexdigest())
                if process.stdout.strip():
                    row['metrics'] = json.loads(process.stdout.strip().splitlines()[-1])
                    if row['metrics'].get('after_close') != 0 or row['metrics'].get('peak_bytes', 2097153) > 2097152:
                        row['returncode'] = 1
                results.append(row)
                print(name, width, height, 'PASS' if row['returncode'] == 0 else row['stderr'], flush=True)
                output = args.output/name/f'{width}x{height}'
                output.mkdir(parents=True, exist_ok=True)
                for image in app.glob('*.pgm'):
                    png(image, output/(image.stem+'.png'))
                for suffix in ('.trace', '.state'):
                    for evidence in app.glob('*'+suffix):
                        shutil.copyfile(evidence, output/evidence.name)
                if args.native and row['returncode'] == 0:
                    for capture, native_model in MODELS.get(name, {}).items():
                        reference = output/(capture+'-native.pgm')
                        result = subprocess.run([str(args.native.resolve()), native_model, str(width), str(height), str(reference)],
                                                text=True, capture_output=True, timeout=30)
                        assert result.returncode == 0, result.stderr
                        png(reference, reference.with_suffix('.png'))
                        w, h, actual = read_png(output/(capture+'.png'))
                        nw, nh, expected = read_png(reference.with_suffix('.png'))
                        assert (w, h) == (nw, nh)
                        differences = sum(a != b for a, b in zip(actual, expected))
                        exact = width == 480 or capture == 'help'
                        comparisons.append(dict(capture=capture, native_model=native_model, width=width, height=height,
                                                whole_frame=differences, exact_required=exact,
                                                candidate_sha256=hashlib.sha256(actual).hexdigest(),
                                                reference_sha256=hashlib.sha256(expected).hexdigest()))
    payload = dict(scope='Original portrait Wordle menu/help/archive/real statistics and retained gameplay; '
                         'landscape menu/calendar are bounded layout adaptations; SD import is an explicit service deviation.',
                   tests=results, native_comparisons=comparisons,
                   native_status='executed' if args.native else 'not run: supply --native; callback passes do not establish pixel parity',
                   native_binary_sha256=hashlib.sha256(args.native.read_bytes()).hexdigest() if args.native else None,
                   app_hashes={str(path.relative_to(args.app_dir)): hashlib.sha256(path.read_bytes()).hexdigest()
                               for path in sorted(args.app_dir.rglob('*')) if path.is_file()})
    (args.output/'results.json').write_text(json.dumps(payload, indent=2)+'\n')
    assert all(row['returncode'] == 0 for row in results), 'See results.json for callback failures'
    assert all(not row['whole_frame'] for row in comparisons if row['exact_required']), 'Original native pixels differ'


if __name__ == '__main__':
    main()
