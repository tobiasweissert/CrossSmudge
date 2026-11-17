#!/usr/bin/env python3
"""Parent-only real HTTPS acceptance. Reads an existing private native config; never prints credentials/content."""
import argparse,hashlib,json,pathlib,re,secrets,ssl,time,urllib.error,urllib.parse,urllib.request
ORIGIN='https://sticky-browser.tobiasweissert.ch'
PATHS={'open','poll','chunk','locate','find','image','cancel'}
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):raise RuntimeError('backend redirect rejected')
p=argparse.ArgumentParser();p.add_argument('--device-config',required=True,type=pathlib.Path);p.add_argument('--url',required=True);a=p.parse_args()
raw=a.device_config.read_bytes()
if len(raw)>512:raise SystemExit('invalid configuration')
def unique(pairs):
 d={}
 for k,v in pairs:
  if k in d:raise ValueError('duplicate configuration field')
  d[k]=v
 return d
cfg=json.loads(raw,object_pairs_hook=unique)
if set(cfg)!={'v','origin','token'} or type(cfg['v']) is not int or cfg['v']!=1 or cfg['origin']!=ORIGIN or not re.fullmatch('[0-9a-f]{64}',cfg['token']):raise SystemExit('invalid configuration')
host=urllib.parse.urlsplit(a.url).hostname or ''
if any(host==x or host.endswith('.'+x) for x in ('nytimes.com','nyt.com','nyt.net','nytimg.com','nyti.ms')):raise SystemExit('host denied')
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),urllib.request.HTTPSHandler(context=ssl.create_default_context()),NoRedirect())
metrics=[]
def request(path,body,image=False):
 if path not in PATHS:raise ValueError('path denied')
 body={'v':1,**body};encoded=json.dumps(body,separators=(',',':')).encode()
 if len(encoded)>2048:raise ValueError('request too large')
 r=urllib.request.Request(ORIGIN+'/v1/'+path,data=encoded,headers={'Content-Type':'application/json','Authorization':'Bearer '+cfg['token']},method='POST')
 try:response=opener.open(r,timeout=20)
 except urllib.error.HTTPError as e:response=e
 with response:
  data=response.read(65537 if image else 49153);status=response.status
  if len(data)>(65536 if image else 49152):raise RuntimeError('response cap')
  if status in range(300,400):raise RuntimeError('redirect rejected')
  metrics.append({'endpoint':path,'status':status,'bytes':len(data)})
  if image and status==200:
   if data[:8]!=b'\x89PNG\r\n\x1a\n' or data[24:26]!=b'\x01\x03':raise RuntimeError('image format')
   return status,{'sha256':hashlib.sha256(data).hexdigest()}
  result=json.loads(data)
  if result.get('v')!=1:raise RuntimeError('schema')
  return status,result
rid=secrets.token_hex(16);status,model=request('open',{'rid':rid,'url':a.url,'mode':'reader','images':'on-demand'})
deadline=time.monotonic()+30
while status==202:
 delay=model.get('retry_ms');job=model.get('job')
 if type(delay) is not int or not 500<=delay<=2000 or not re.fullmatch('[0-9a-f]{32}',job or '') or time.monotonic()>deadline:raise RuntimeError('pending bounds')
 time.sleep(delay/1000);status,model=request('poll',{'job':job})
if status!=200 or model.get('state')!='ready':raise SystemExit('open failed with stable code '+str(model.get('error')))
doc=model['doc'];chunks=model['chunks'];asset=next((b['image']['asset'] for b in model['blocks'] if b['kind']=='image'),None)
if chunks>1:
 status,last=request('chunk',{'doc':doc,'chunk':chunks-1})
 if status!=200:raise RuntimeError('last chunk failed')
 if asset is None:asset=next((b['image']['asset'] for b in last['blocks'] if b['kind']=='image'),None)
request('locate',{'doc':doc,'fragment':'top'});request('find',{'doc':doc,'text':'example','after':None})
if asset:request('image',{'doc':doc,'asset':asset,'width':160,'height':160},True)
# Exercise cancel-before-open without relying on an internet publisher being slow.
cancelled=secrets.token_hex(16);status,_=request('cancel',{'rid':cancelled})
if status!=200:raise RuntimeError('cancel failed')
status,error=request('open',{'rid':cancelled,'url':a.url,'mode':'reader','images':'on-demand'})
if status!=410 or error.get('error')!='cancelled':raise RuntimeError('tombstone failed')
print(json.dumps({'chunks':chunks,'truncated':model['truncated'],'operations':metrics,'content_output':False},indent=2))
