#!/usr/bin/env python3
"""Local SD candidates from reconciled port inventory; never change publication metadata.

ZIP readback is an integrity check. Runtime qualification is a separate receipt.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BASE = '621719e4f68d8ddf719d470f84be20ad22c11d35'
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--inventory', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--app', action='append', help='Bounded subset; default is every reconciled port')
a = p.parse_args()
ledger = json.loads(a.inventory.read_text())
ports = {t['target_id']: t['source_id'] for t in ledger['titles'] if t['ported']}
assert len(ports) == ledger['counts']['ported_titles'] == 24
selected = sorted(a.app or ports)
assert len(selected) == len(set(selected)) and set(selected) <= ports.keys()
a.output.mkdir(parents=True, exist_ok=True)
reports = []
catalog = subprocess.check_output(['git', 'show', BASE+':apps/catalog.json'], cwd=ROOT)
assert catalog == (ROOT/'apps/catalog.json').read_bytes(), 'Publication catalog changed'
entries = json.loads(catalog)
entries = entries['apps'] if isinstance(entries, dict) else entries
entries = {entry['id']: entry for entry in entries}
for app in selected:
    base = ROOT/'apps'/app
    manifest = (base/'manifest.json').read_bytes()
    assert manifest == subprocess.check_output(['git', 'show', BASE+':apps/'+app+'/manifest.json'], cwd=ROOT)
    assert json.loads(manifest)['id'] == app
    members = {}
    for path in sorted(base.rglob('*')):
        assert not path.is_symlink(), 'Symlink in package: '+str(path)
        if not path.is_file():
            continue
        relative = path.relative_to(base).as_posix()
        parts = PurePosixPath(relative).parts
        assert '..' not in parts and not relative.startswith('/')
        assert not any(part in ('.git', '.pio', '__pycache__', 'storage', 'saved_data', 'network_data') for part in parts)
        assert path.suffix not in ('.pyc', '.pgm', '.log', '.dat'), 'Fixture/save in package: '+relative
        members[relative] = path.read_bytes()
    runtime = {name: {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()} for name, data in members.items()}
    tree = hashlib.sha256(json.dumps(runtime, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    archive = a.output/(app+'-'+tree[:12]+'-SD.zip')
    prefix = '.crosssmudge/applications/'+app+'/'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in members.items():
            info = zipfile.ZipInfo(prefix+name, (2026, 11, 16, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            z.writestr(info, data)
    with tempfile.TemporaryDirectory(prefix=app+'-readback-', dir=a.output) as temporary:
        with zipfile.ZipFile(archive) as z:
            assert z.testzip() is None
            assert sorted(z.namelist()) == sorted(prefix+name for name in members)
            z.extractall(temporary)
            extracted = Path(temporary)/prefix
            for name, data in members.items():
                assert z.read(prefix+name) == data == (extracted/name).read_bytes()
    reports.append({'id': app, 'source_id': ports[app], 'archive': archive.name,
                    'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(), 'bytes': archive.stat().st_size,
                    'source_tree_sha256': tree, 'members': runtime,
                    'catalog_omissions': sorted(set(members)-set(entries[app]['files'])),
                    'catalog_missing_from_disk': sorted(set(entries[app]['files'])-set(members)),
                    'manifest_unchanged': True, 'zip_and_extracted_readback': True,
                    'runtime_qualification': 'pending separate app-specific extracted-package tests'})
report = {'baseline': BASE, 'inventory_sha256': hashlib.sha256(a.inventory.read_bytes()).hexdigest(),
          'reconciled_port_count': 24, 'packaged_count': len(reports), 'complete_inventory': len(reports) == 24,
          'publication_metadata_unchanged': True, 'packages': reports}
(a.output/'runtime-members.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps({'packages': len(reports), 'complete_inventory': report['complete_inventory'], 'output': str(a.output)}))
