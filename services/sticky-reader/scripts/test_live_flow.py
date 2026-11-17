import contextlib,io,json,pathlib,runpy,sys,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parent.parent
class LiveFlow(unittest.TestCase):
 def test_public_cli_uses_no_private_file_or_auth(self):
  seen=[]
  class Reply:
   status=200
   body={}
   def __enter__(self):return self
   def __exit__(self,*args):pass
   def read(self,n):return json.dumps(self.body).encode()
  class Opener:
   def open(self,r,timeout):
    selftest.assertNotIn('Authorization',r.headers);seen.append(r.full_url)
    q=json.loads(r.data);p=r.full_url.rsplit('/',1)[-1];v=Reply()
    if p=='open' and len(seen)==1:v.body={'v':1,'state':'ready','doc':'a'*32,'chunks':1,'blocks':[],'truncated':False}
    elif p=='open':v.status=410;v.body={'v':1,'error':'cancelled'}
    elif p=='cancel':v.body={'v':1,'state':'cancelled'}
    else:v.body={'v':1,'found':False}
    return v
  selftest=self
  with patch.object(sys,'argv',['live_flow.py','--url','https://example.org/']),patch('urllib.request.build_opener',return_value=Opener()),contextlib.redirect_stdout(io.StringIO()):runpy.run_path(str(ROOT/'scripts/live_flow.py'),run_name='__main__')
  self.assertEqual(len(seen),5)
 def run_fixture(self,overrides=None,image=False):
  seen=[];overrides=overrides or {}
  class Reply:
   def __init__(self,status,body):self.status=status;self.body=body
   def __enter__(self):return self
   def __exit__(self,*args):pass
   def read(self,n):return (self.body if isinstance(self.body,bytes) else json.dumps(self.body).encode())[:n]
  class Opener:
   def open(opener,r,timeout):
    self.assertNotIn('Authorization',r.headers)
    endpoint=r.full_url.rsplit('/',1)[-1];seen.append(endpoint)
    if endpoint in overrides:return Reply(*overrides[endpoint])
    if endpoint=='open' and len(seen)==1:
     blocks=[{'kind':'image','image':{'asset':'b'*32}}] if image else []
     return Reply(200,{'v':1,'state':'ready','doc':'a'*32,'chunks':1,'blocks':blocks,'truncated':False})
    if endpoint=='open':return Reply(410,{'v':1,'error':'cancelled'})
    if endpoint=='cancel':return Reply(200,{'v':1,'state':'cancelled'})
    return Reply(200,{'v':1,'found':False})
  output=io.StringIO()
  with patch.object(sys,'argv',['live_flow.py','--url','https://example.org/']),patch('urllib.request.build_opener',return_value=Opener()),contextlib.redirect_stdout(output):
   namespace=runpy.run_path(str(ROOT/'scripts/live_flow.py'),run_name='__main__')
  return json.loads(output.getvalue()),namespace,seen
 def test_attempted_operations_require_http200(self):
  for endpoint in ('locate','find','image'):
   for status in (429,403,500):
    with self.subTest(endpoint=endpoint,status=status):
     with self.assertRaisesRegex(RuntimeError,endpoint+' failed'):
      self.run_fixture({endpoint:(status,{'v':1,'error':'busy','retryable':True})},image=endpoint=='image')
 def test_found_results_reject_malformed_types_and_error_envelopes(self):
  invalid=[{}, {'found':None}, {'found':0}, {'found':'false'}, {'found':[]},
           {'error':'busy'}, {'found':False,'error':'busy'},
           {'found':True}, {'found':True,'chunk':None}, {'found':True,'chunk':True},
           {'found':True,'chunk':-1}, {'found':True,'chunk':1}]
  for field,upper in (('block',1024),('run',256),('byte',2049)):
   for value in (None,True,'1',1.5,-1,upper+1):invalid.append({'found':True,'chunk':0,field:value})
  for endpoint in ('locate','find'):
   for fields in invalid:
    with self.subTest(endpoint=endpoint,fields=fields):
     with self.assertRaisesRegex(RuntimeError,endpoint+' result'):
      self.run_fixture({endpoint:(200,{'v':1,**fields})})
 def test_found_false_and_go_omitted_zero_positions_are_valid(self):
  for result in ({'found':False},{'found':True,'chunk':0},{'found':True,'chunk':0,'block':1,'run':1,'byte':1}):
   with self.subTest(result=result):
    report,_,seen=self.run_fixture({p:(200,{'v':1,**result}) for p in ('locate','find')})
    self.assertEqual(seen,['open','locate','find','cancel','open'])
    self.assertFalse(report['content_output'])
 def test_image200_json_error_is_not_success(self):
  with self.assertRaisesRegex(RuntimeError,'image format'):
   self.run_fixture({'image':(200,{'v':1,'error':'busy'})},image=True)
 def test_real_palette_one_bit_png_is_checked_and_hashed(self):
  import hashlib,struct,zlib
  def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
  png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,1,3,0,0,0))+chunk(b'PLTE',b'\x00\x00\x00\xff\xff\xff')+chunk(b'IDAT',zlib.compress(b'\x00\x00'))+chunk(b'IEND',b'')
  report,namespace,seen=self.run_fixture({'image':(200,png)},image=True)
  self.assertIn('image',seen)
  self.assertEqual(namespace['result'],{'sha256':hashlib.sha256(png).hexdigest()})
  self.assertIn({'endpoint':'image','status':200,'bytes':len(png)},report['operations'])
if __name__=='__main__':unittest.main()
