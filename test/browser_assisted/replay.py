#!/usr/bin/env python3
"""Production HttpCore/resources/Lua callbacks over numeric loopback.
Default is explicitly a synthetic fixture. --endpoint relays to a parent's
actual local service with the SYNTHETIC a*64 test key; never production TLS proof.
Run run.py first to provision isolated Lua objects. No downloads/PIO involved.
"""
import os,pathlib,subprocess,json,http.server,threading,urllib.request,urllib.error,argparse,hashlib
import sys,tempfile
sys.dont_write_bytecode=True
from guard import check
check()
R=pathlib.Path(__file__).resolve().parents[2];os.chdir(R)
a=argparse.ArgumentParser();a.add_argument('--endpoint');a.add_argument('--in-process',action='store_true');a.add_argument('--response-http',type=pathlib.Path);a.add_argument('--url',default='https://example.org/synthetic');args=a.parse_args()
if args.endpoint:
 u=urllib.parse.urlsplit(args.endpoint)
 assert u.scheme=='http'and u.hostname=='127.0.0.1'and u.port and u.path in ('','/')and not u.query and not u.username,'parent service must be explicit numeric loopback'
O=pathlib.Path(os.environ.get('ASSISTED_TEST_OUT',str(pathlib.Path(tempfile.gettempdir())/'crosssmudge-browser-assisted-tests')));O.mkdir(exist_ok=True,parents=True)
flags=['-O1','-g','-DBOARD_HAS_PSRAM=1','-Ilib/Lua/src','-Ilib/LuaHttp','-Ilib/LuaBrowser','-Ilib/Memory','-Ilib/miniz/src','-Isrc/activities/apps/lua']
if os.environ.get('SANITIZE'):flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
objs=[O/(p.stem+'.o')for p in sorted(pathlib.Path('lib/Lua/src').glob('l*.c'))]
command=['c++','-std=c++20',*flags,'test/browser_assisted/replay_runner.cpp','lib/LuaHttp/LuaHttp.cpp','lib/LuaHttp/HttpCore.cpp','lib/LuaHttp/HttpGzip.cpp','lib/hal/HalLuaResources.cpp',O/'miniz.o',*objs,'-pthread','-lm','-ldl','-o',O/'replay']
subprocess.run(list(map(str,command)),check=True)
receipts=[]
class Handler(http.server.BaseHTTPRequestHandler):
 def log_message(self,*a):pass
 def do_POST(self):
  assert self.headers['Authorization']=='Bearer '+'a'*64
  body=self.rfile.read(int(self.headers['Content-Length']));assert len(body)<=2048
  if args.endpoint:
   req=urllib.request.Request(args.endpoint.rstrip('/')+self.path,data=body,headers={'Authorization':'Bearer '+'a'*64,'Content-Type':'application/json'},method='POST')
   try:res=urllib.request.urlopen(req,timeout=20)
   except urllib.error.HTTPError as e:res=e
   with res:status=res.status;raw=res.read(65537);kind=res.headers.get('Content-Type','')
  else:
   q=json.loads(body);assert self.path=='/v1/open'
   status=200;kind='application/json';raw=json.dumps(dict(v=1,state='ready',doc='a'*32,url=q['url'],title='Original loopback fixture',lang='en',dir='ltr',mode=q['mode'],chunks=1,chunk=0,truncated=False,blocks=[dict(id=1,kind='h1',runs=[dict(text='Original loopback fixture',size=2)])],anchors=[]),separators=(',',':')).encode()
  receipts.append({'endpoint':self.path,'status':status,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()})
  self.send_response(status);self.send_header('Content-Type',kind);self.send_header('Content-Length',str(len(raw)));self.end_headers();self.wfile.write(raw)
server=None;port=0
if args.in_process or args.response_http:
 if args.response_http:raw=args.response_http.read_bytes()
 else:
  body=json.dumps(dict(v=1,state='ready',doc='a'*32,url=args.url,title='Original byte fixture',lang='en',dir='ltr',mode='reader',chunks=1,chunk=0,truncated=False,blocks=[dict(id=1,kind='p',runs=[dict(text='Original native byte fixture')])],anchors=[]),separators=(',',':')).encode()
  raw=b'HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: '+str(len(body)).encode()+b'\r\n\r\n'+body
 (O/'replay-response.http').write_bytes(raw)
 receipts.append({'response_sha256':hashlib.sha256(raw).hexdigest(),'bytes':len(raw)})
else:
 server=http.server.HTTPServer(('127.0.0.1',0),Handler);port=server.server_port;thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
prefix=pathlib.Path('test/browser_assisted/client.lua').read_text().split("local J=dofile('json.lua')",1)[0]
driver='''
for k,v in pairs(native_api)do smudge[k]=v end
smudge.api_version=7;smudge.millis=native_millis
smudge.browser_request_id=native_api.browser_request_id
local originalUpdate
local function update()native_dispatch();originalUpdate()end
dofile('main.lua');originalUpdate=on_update;on_update=update;on_init();draw()
click('Address');edit(TARGET);on_update()
local loaded=false
for i=1,3500 do native_pause();on_update();on_draw();if text:find('Chunk ',1,true)then loaded=true;break end end
assert(loaded,'actual service/model flow failed: '..text)
on_exit();for i=1,10 do native_dispatch()end
print('PASS native POST -> HTTP bytes -> resource -> real Lua on_http -> strict SRP1 -> SD -> Browser render')
'''.replace('TARGET',json.dumps(args.url))
(O/'replay.lua').write_text(prefix+driver)
(O/'replay-data').mkdir(exist_ok=True)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1',REPLAY_HTTP_RESPONSE=str(O/'replay-response.http'))
try:
 p=subprocess.run([str(O/'replay'),str(O/'replay.lua'),str(port),str(O/'replay-data')],env=env,timeout=45)
finally:
 if server:server.shutdown();server.server_close();thread.join()
(O/'replay-receipt.json').write_text(json.dumps({'mode':'provided HTTP byte replay'if args.response_http else 'synthetic in-process native byte fixture'if args.in_process else 'actual parent service relay'if args.endpoint else 'synthetic loopback fixture','responses':receipts,'exit':p.returncode},indent=2)+'\n')
p.check_returncode()
