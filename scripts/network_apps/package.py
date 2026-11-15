#!/usr/bin/env python3
"""Build reviewed local previews. Does not publish, edit catalog, or bump versions."""
import hashlib, json, pathlib, sys, zipfile
root=pathlib.Path(__file__).resolve().parents[2]
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
apps=('cp-hackernews','cp-trivia')
for shared in ('json.lua','cache.lua','ui.lua'):
    assert (root/'apps'/apps[0]/shared).read_bytes()==(root/'apps'/apps[1]/shared).read_bytes(),shared
inventory=[]
for app in apps:
    folder=root/'apps'/app
    files=sorted(p for p in folder.rglob('*') if p.is_file())
    manifest=json.loads((folder/'manifest.json').read_text())
    assert manifest['id']==app and manifest['min_api']==3
    assert len(manifest['network']['domains'])<=4
    assert not any(p.suffix in ('.cache','.pyc') for p in files)
    archive=out/(app+'.zip')
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_STORED) as z:
        for p in files:
            info=zipfile.ZipInfo(app+'/'+p.relative_to(folder).as_posix(),date_time=(2026,11,15,0,0,0))
            info.external_attr=0o100644<<16
            z.writestr(info,p.read_bytes())
    rows=[]
    with zipfile.ZipFile(archive) as z:
        assert z.testzip() is None
        assert len(z.namelist())==len(files)
        for p in files:
            rel=p.relative_to(folder).as_posix();data=z.read(app+'/'+rel)
            assert data==p.read_bytes()
            rows.append({'path':rel,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()})
    data=archive.read_bytes()
    inventory.append({'id':app,'zip_bytes':len(data),'zip_sha256':hashlib.sha256(data).hexdigest(),
                      'unpacked_bytes':sum(r['bytes'] for r in rows),'files':rows})
(out/'inventory.json').write_text(json.dumps(inventory,indent=2)+'\n')
print(json.dumps(inventory,indent=2))
