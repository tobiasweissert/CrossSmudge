"""Build unpublished owner-test app packages, including new UI assets, never saves."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('destination', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
destination = args.destination.resolve()
destination.mkdir(parents=True, exist_ok=True)
individual = destination / 'individual-apps'
individual.mkdir(exist_ok=True)
catalog = json.loads((root / 'apps/catalog.json').read_text())
source_files = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', 'apps'], cwd=root).decode().split('\0')
inventory = []
with zipfile.ZipFile(destination / 'apps-SD-overlay.zip', 'w', zipfile.ZIP_DEFLATED) as overlay:
    for app in catalog['apps']:
        if app['id']=='cp-trivia':continue  # Owner excluded untested Trivia from PR scope.
        app_id = app['id']
        prefix = f'apps/{app_id}/'
        files = set(app['files'])
        for name in source_files:
            if name.startswith(prefix):
                relative = name[len(prefix):]
                if Path(relative).suffix in {'.lua', '.bin', '.raw', '.json', '.txt'}:
                    files.add(relative)
        package = individual / f'{app_id}-owner-test.zip'
        with zipfile.ZipFile(package, 'w', zipfile.ZIP_DEFLATED) as archive:
            for relative in sorted(files):
                path = PurePosixPath(relative)
                assert not path.is_absolute() and '..' not in path.parts
                assert path.suffix not in {'.dat', '.sav', '.bak', '.cache'}
                source = root / prefix / relative
                assert source.is_file() and not source.is_symlink()
                data = source.read_bytes()
                archive.writestr(f'{app_id}/{relative}', data)
                overlay.writestr(f'.crosssmudge/applications/{app_id}/{relative}', data)
                inventory.append(dict(app=app_id, file=relative, version=app['version'], bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
        with zipfile.ZipFile(package) as archive:
            assert archive.testzip() is None
with zipfile.ZipFile(destination / 'apps-SD-overlay.zip') as archive:
    assert archive.testzip() is None and len(archive.namelist()) == len(inventory)
    for record in inventory:
        data = archive.read(f".crosssmudge/applications/{record['app']}/{record['file']}")
        assert hashlib.sha256(data).hexdigest() == record['sha256']
(destination / 'app-file-inventory.json').write_text(json.dumps(inventory, indent=2)+'\n')
print(f'Verified {len({row["app"] for row in inventory})} local test packages and {len(inventory)} files; catalog and manifest versions unchanged')
