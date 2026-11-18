#!/usr/bin/env python3
"""Parent-only public HTTPS acceptance. No key/private file; no content output."""
import argparse,hashlib,json,pathlib,re,secrets,ssl,time,urllib.error,urllib.parse,urllib.request
ORIGIN='https://sticky.tobiasweissert.ch'
PATHS={'open','poll','chunk','locate','find','image','cancel'}
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):raise RuntimeError('backend redirect rejected')
p=argparse.ArgumentParser();p.add_argument('--url',required=True);a=p.parse_args()
host=urllib.parse.urlsplit(a.url).hostname or ''
if any(host==x or host.endswith('.'+x) for x in ('nytimes.com','nyt.com','nyt.net','nytimg.com','nyti.ms')):raise SystemExit('host denied')
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}),urllib.request.HTTPSHandler(context=ssl.create_default_context()),NoRedirect())
metrics=[]
def request(path,body,image=False):
 if path not in PATHS:raise ValueError('path denied')
 body={'v':1,**body};encoded=json.dumps(body,separators=(',',':')).encode()
 if len(encoded)>2048:raise ValueError('request too large')
 r=urllib.request.Request(ORIGIN+'/v1/'+path,data=encoded,headers={'Content-Type':'application/json'},method='POST')
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
for endpoint,body in (('locate',{'doc':doc,'fragment':'top'}),('find',{'doc':doc,'text':'example','after':None})):
 status,result=request(endpoint,body)
 if status!=200:raise RuntimeError(endpoint+' failed')
 if type(result.get('found')) is not bool or 'error' in result:raise RuntimeError(endpoint+' result')
 if result['found']:
  # Match Browser P.position's explicit typed envelope, plus known chunk count.
  # Membership/UTF-8 boundaries in an unrequested chunk are not qualified here.
  for field,lower,upper in (('chunk',0,min(chunks-1,63)),('block',1,1024),('run',1,512),('byte',1,2049)):
   value=result.get(field)
   if type(value) is not int or not lower<=value<=upper:raise RuntimeError(endpoint+' result position')
if asset:
 status,result=request('image',{'doc':doc,'asset':asset,'width':160,'height':160},True)
 if status!=200:raise RuntimeError('image failed')
# Exercise cancel-before-open without relying on an internet publisher being slow.
cancelled=secrets.token_hex(16);status,_=request('cancel',{'rid':cancelled})
if status!=200:raise RuntimeError('cancel failed')
status,error=request('open',{'rid':cancelled,'url':a.url,'mode':'reader','images':'on-demand'})
if status!=410 or error.get('error')!='cancelled':raise RuntimeError('tombstone failed')
print(json.dumps({'chunks':chunks,'truncated':model['truncated'],'operations':metrics,'content_output':False},indent=2))
