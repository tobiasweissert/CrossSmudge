import hashlib, importlib.util, io, json, pathlib, tarfile, tempfile, unittest
spec=importlib.util.spec_from_file_location('oci',pathlib.Path(__file__).with_name('package_oci.py'))
oci=importlib.util.module_from_spec(spec);spec.loader.exec_module(oci)
class OCI(unittest.TestCase):
 def test_exact_content_and_security(self):
  with tempfile.TemporaryDirectory() as d:
   d=pathlib.Path(d);binary=d/'arm64';binary.write_bytes(b'\x7fELF'+bytes([2,1,1])+bytes(11)+bytes([183,0])+b'original synthetic ELF test bytes')
   ca=d/'ca';ca.write_bytes(b'-----BEGIN CERTIFICATE-----\nsynthetic test only\n-----END CERTIFICATE-----\n');out=d/'image.tar'
   digest=oci.package(binary,ca,out);report=oci.validate(out,binary,ca)
   self.assertEqual(report['manifest'],digest);self.assertEqual(report['platform'],'linux/arm64')
   self.assertEqual(report['binary_sha256'],hashlib.sha256(binary.read_bytes()).hexdigest())
   with tarfile.open(out) as t:
    names=t.getnames();self.assertEqual(len(names),len(set(names)));self.assertTrue(all(not n.startswith('/') and '..' not in n.split('/') for n in names))
 def test_wrong_arch(self):
  with tempfile.TemporaryDirectory() as d:
   d=pathlib.Path(d);binary=d/'binary';binary.write_bytes(b'not arm64 ELF');ca=d/'ca';ca.write_bytes(b'ca')
   with self.assertRaises(ValueError):oci.package(binary,ca,d/'out')

class OCINegative(unittest.TestCase):
 def test_tampered_hash_traversal_platform_config(self):
  with tempfile.TemporaryDirectory() as d:
   d=pathlib.Path(d);binary=d/'arm64';binary.write_bytes(b'\x7fELF'+bytes([2,1,1])+bytes(11)+bytes([183,0])+b'original');ca=d/'ca';ca.write_bytes(b'-----BEGIN CERTIFICATE-----\nsynthetic\n');archive=d/'valid';oci.package(binary,ca,archive)
   with tarfile.open(archive) as t:original={m.name:t.extractfile(m).read() for m in t.getmembers()}
   for mode in ('hash','traversal','platform','config'):
    files=dict(original);index=json.loads(files['index.json'])
    if mode=='hash':
     key=next(k for k in files if k.startswith('blobs/'));files[key]+=b'corrupt'
    elif mode=='traversal':files['../escape']=b'x'
    elif mode=='platform':index['manifests'][0]['platform']['architecture']='amd64';files['index.json']=json.dumps(index).encode()
    else:
     desc=index['manifests'][0];manifest=json.loads(files['blobs/sha256/'+desc['digest'][7:]]);configdesc=manifest['config'];cfg=json.loads(files['blobs/sha256/'+configdesc['digest'][7:]]);cfg['config']['User']='0:0';raw=oci.encode(cfg);files['blobs/sha256/'+oci.digest(raw)]=raw;manifest['config']={'mediaType':configdesc['mediaType'],'size':len(raw),'digest':'sha256:'+oci.digest(raw)};raw=oci.encode(manifest);files['blobs/sha256/'+oci.digest(raw)]=raw;desc['digest']='sha256:'+oci.digest(raw);desc['size']=len(raw);files['index.json']=oci.encode(index)
    bad=d/mode
    with tarfile.open(bad,'w') as t:
     for name,data in files.items():
      info=tarfile.TarInfo(name);info.size=len(data);t.addfile(info,io.BytesIO(data))
    with self.assertRaises((AssertionError,ValueError)):oci.validate(bad,binary,ca)

if __name__=='__main__':unittest.main()
