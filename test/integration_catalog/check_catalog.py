#!/usr/bin/env python3
"""Frozen owner-catalog release gate; run only after final replacement import."""
import json
import pathlib
import re
import subprocess
import argparse
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--owner-overlay', type=pathlib.Path,
                    help='Verify unpublished owner UI dependencies in their actual SD overlay')
args = parser.parse_args()
overlay = zipfile.ZipFile(args.owner_overlay) if args.owner_overlay else None

root = pathlib.Path(__file__).resolve().parents[2]
catalog = json.loads((root / 'apps/catalog.json').read_text())
assert catalog['repo'] == 'tobiasweissert/CrossSmudge'
assert catalog['branch'] == 'integration/sticky-test-all'
expected = {'chess': '1.1.0', 'sudoku': '1.1.0', 'solitaire': '1.1.0',
            'wordle': '1.2.0', 'dashboard': '1.0.3', 'hourglass': '1.0.1',
            'watertracker': '1.0.1', '2048': '1.1.0'}
ids = set()
for app in catalog['apps']:
    app_id = app['id']
    assert re.fullmatch(r'[A-Za-z0-9_-]{1,64}', app_id), app_id
    assert app_id not in ids, app_id
    ids.add(app_id)
    folder = root / 'apps' / app_id
    manifest = json.loads((folder / 'manifest.json').read_text())
    assert manifest['id'] == app_id
    assert manifest['version'] == app['version'], app_id
    if app_id in expected:
        assert app['version'] == expected[app_id], (app_id, 'final input missing')
    files = app['files']
    assert len(files) == len(set(files)), app_id
    assert 'manifest.json' in files and manifest.get('entry', 'main.lua') in files
    for rel in files:
        assert re.fullmatch(r'[A-Za-z0-9_-][A-Za-z0-9_.-]*(/[A-Za-z0-9_-][A-Za-z0-9_.-]*)*', rel), rel
        path = folder / rel
        assert path.is_file() and not path.is_symlink(), path
    # Owner UI ports are unpublished: their extra modules/assets ship in the
    # owner-test overlay, without changing the published catalog or versions.
    packaged = set(files)
    if overlay is not None and app_id != 'cp-trivia':
        prefix = f'.crosssmudge/applications/{app_id}/'
        packaged = {name[len(prefix):] for name in overlay.namelist() if name.startswith(prefix)}
        assert set(files) <= packaged, (app_id, 'catalog member omitted from overlay')
        for rel in packaged:
            assert overlay.read(prefix + rel) == (folder / rel).read_bytes(), (app_id, rel)
            assert not rel.endswith(('.dat', '.sav', '.bak', '.cache')), rel
    # All code, sprites and data must ship, including dynamically selected files.
    for path in folder.rglob('*'):
        if not path.is_file():
            continue
        rel = path.relative_to(folder).as_posix()
        if path.suffix in {'.lua', '.raw', '.bin', '.json', '.txt'} or rel.startswith(('data/', 'sprites/', 'maps/')):
            assert rel in packaged, (app_id, 'missing dependency', rel)
        if path.suffix == '.lua':
            for dep in re.findall(r'''\bdofile\(['"]([^'"]+)['"]\)''', path.read_text()):
                assert dep in packaged, (app_id, 'missing module', dep)
    if app_id in {'cp-hackernews', 'cp-trivia'}:
        assert app['requires_psram'] is True and app['min_api'] == manifest['min_api'] == 3
assert len([i for i in ids if i.startswith('cp-') and i not in {'cp-hackernews', 'cp-trivia'}]) == 18
assert expected.keys() <= ids
print('Owner catalog versions, safe paths and complete runtime dependencies verified')

baseline = json.loads(subprocess.check_output(['git', 'show', '2b40bf110c4e1e01b48346eeb16c501699a629cf:apps/catalog.json'], cwd=root))
old = {a['id']: a for a in baseline['apps']}
new = {a['id']: a for a in catalog['apps']}
assert len(new) == 36 and len(set(new) - set(old)) == 20
assert set(old) <= set(new)
assert {i for i in old if old[i] != new[i]} == set(expected)
for app in catalog['apps']:
    if app['id'] in expected or app['id'].startswith('cp-'):
        assert not any(path.endswith(('.dat', '.cache', '.bak', '.sav')) for path in app['files'])
print('36 catalog entries: 20 new, 8 updated, 8 unchanged; no save payloads')
