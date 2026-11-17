#!/usr/bin/env python3
"""Deterministic, daemonless ARM64 OCI archive. No network or registry operations."""
import argparse, hashlib, io, json, pathlib, struct, tarfile

def encode(obj):return json.dumps(obj,sort_keys=True,separators=(',',':')).encode()
def digest(data):return hashlib.sha256(data).hexdigest()
def member(t,name,data=b'',mode=0o644,directory=False):
 p=pathlib.PurePosixPath(name)
 if p.is_absolute() or '..' in p.parts:raise ValueError('unsafe archive path')
 info=tarfile.TarInfo(str(p));info.uid=info.gid=65532;info.mode=mode;info.mtime=0
 if directory:info.type=tarfile.DIRTYPE;info.size=0;t.addfile(info)
 else:info.size=len(data);t.addfile(info,io.BytesIO(data))
def elf(data):
 if len(data)<20 or data[:7]!=b'\x7fELF\x02\x01\x01' or struct.unpack_from('<H',data,18)[0]!=183:raise ValueError('expected little-endian ELF64 ARM64')
def package(binary,ca,output):
 binary,ca=pathlib.Path(binary).read_bytes(),pathlib.Path(ca).read_bytes();elf(binary)
 if b'-----BEGIN CERTIFICATE-----' not in ca:raise ValueError('CA bundle missing')
 stream=io.BytesIO()
 with tarfile.open(fileobj=stream,mode='w',format=tarfile.USTAR_FORMAT) as t:
  for name in ('usr','usr/local','usr/local/bin','etc','etc/ssl','etc/ssl/certs','run','run/auth'):member(t,name,mode=0o755,directory=True)
  for name in ('cache','tmp'):member(t,name,mode=0o770,directory=True)
  member(t,'usr/local/bin/sticky-reader',binary,0o755);member(t,'etc/ssl/certs/ca-certificates.crt',ca)
 layer=stream.getvalue();config=encode({'architecture':'arm64','os':'linux','config':{'User':'65532:65532','WorkingDir':'/cache','Entrypoint':['/usr/local/bin/sticky-reader'],'Env':['GOMAXPROCS=1','GOMEMLIMIT=48MiB','SRP_LISTEN=:8080','SRP_CACHE=/cache/session','SRP_VERIFIERS=/run/auth/verifiers.json','SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt'],'ExposedPorts':{'8080/tcp':{}},'Volumes':{'/cache':{},'/tmp':{}}},'rootfs':{'type':'layers','diff_ids':['sha256:'+digest(layer)]},'history':[{'created_by':'sticky-reader stdlib packager'}]})
 def desc(data,media):return {'mediaType':media,'digest':'sha256:'+digest(data),'size':len(data)}
 manifest=encode({'schemaVersion':2,'mediaType':'application/vnd.oci.image.manifest.v1+json','config':desc(config,'application/vnd.oci.image.config.v1+json'),'layers':[desc(layer,'application/vnd.oci.image.layer.v1.tar')]})
 descriptor=desc(manifest,'application/vnd.oci.image.manifest.v1+json');descriptor.update(platform={'os':'linux','architecture':'arm64'},annotations={'org.opencontainers.image.ref.name':'localhost/sticky-reader:reviewed'})
 index=encode({'schemaVersion':2,'manifests':[descriptor]})
 with tarfile.open(output,'w',format=tarfile.USTAR_FORMAT) as t:
  member(t,'oci-layout',encode({'imageLayoutVersion':'1.0.0'}));member(t,'index.json',index)
  for data in (manifest,config,layer):member(t,'blobs/sha256/'+digest(data),data)
 return 'sha256:'+digest(manifest)
def validate(archive,binary,ca):
 with tarfile.open(archive) as t:
  names=t.getnames()
  if len(names)!=len(set(names)):raise ValueError('duplicate path')
  for m in t.getmembers():
   p=pathlib.PurePosixPath(m.name)
   if p.is_absolute() or '..' in p.parts or not m.isfile():raise ValueError('unsafe archive member')
  data={m.name:t.extractfile(m).read() for m in t.getmembers()}
 if json.loads(data['oci-layout'])!={'imageLayoutVersion':'1.0.0'}:raise ValueError('layout')
 index=json.loads(data['index.json']);assert index['schemaVersion']==2 and len(index['manifests'])==1
 descriptor=index['manifests'][0];assert descriptor['platform']=={'os':'linux','architecture':'arm64'}
 def blob(d):
  a,h=d['digest'].split(':');assert a=='sha256';b=data['blobs/sha256/'+h];assert len(b)==d['size'] and digest(b)==h;return b
 manifest=json.loads(blob(descriptor));assert manifest['schemaVersion']==2 and len(manifest['layers'])==1
 cfg=json.loads(blob(manifest['config']));layer=blob(manifest['layers'][0]);assert cfg['rootfs']['diff_ids']==['sha256:'+digest(layer)] and cfg['architecture']=='arm64' and cfg['os']=='linux'
 c=cfg['config'];assert c['User']=='65532:65532' and c['Entrypoint']==['/usr/local/bin/sticky-reader'] and c['WorkingDir']=='/cache';assert 'GOMEMLIMIT=48MiB' in c['Env'] and 'GOMAXPROCS=1' in c['Env'] and 'SRP_VERIFIERS=/run/auth/verifiers.json' in c['Env'];assert c['Volumes']=={'/cache':{},'/tmp':{}}
 with tarfile.open(fileobj=io.BytesIO(layer)) as t:
  seen=set()
  for m in t.getmembers():
   assert m.name not in seen;seen.add(m.name);p=pathlib.PurePosixPath(m.name);assert not p.is_absolute() and '..' not in p.parts and (m.isfile() or m.isdir()) and m.uid==65532 and m.gid==65532
  b=t.extractfile('usr/local/bin/sticky-reader').read();elf(b);assert b==pathlib.Path(binary).read_bytes();assert t.getmember('usr/local/bin/sticky-reader').mode==0o755
  assert t.extractfile('etc/ssl/certs/ca-certificates.crt').read()==pathlib.Path(ca).read_bytes()
  assert t.getmember('cache').mode==0o770 and t.getmember('tmp').mode==0o770
 return {'manifest':descriptor['digest'],'platform':'linux/arm64','binary_sha256':digest(b),'archive_sha256':digest(pathlib.Path(archive).read_bytes())}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--binary',required=True,type=pathlib.Path);p.add_argument('--ca',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args();package(a.binary,a.ca,a.output);print(json.dumps(validate(a.output,a.binary,a.ca),indent=2))
