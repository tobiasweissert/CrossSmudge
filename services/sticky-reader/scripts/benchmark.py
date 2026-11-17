#!/usr/bin/env python3
"""Private capture -> actual release worker. Output sizes/hashes/coverage only, never titles or body snippets."""
import argparse,base64,hashlib,html.parser,json,pathlib,resource,subprocess,time
class Paragraphs(html.parser.HTMLParser):
 def __init__(self):super().__init__();self.stack=[];self.current=None;self.paragraphs=[];self.article_paragraphs=[];self.article=0;self.links=[];self.article_links=[];self.anchor=None
 def handle_starttag(self,tag,attrs):
  a=dict(attrs);hidden=tag in ('script','style','head','template','noscript','iframe') or 'hidden' in a or a.get('aria-hidden')=='true' or 'display:none' in a.get('style','').replace(' ','')
  if tag not in ('area','base','br','col','embed','hr','img','input','link','meta','param','source','track','wbr'):self.stack.append((tag,hidden))
  if tag=='article':self.article+=1
  if tag=='p':self.current=[]
  if tag=='a':self.anchor=[]
 def handle_data(self,data):
  if self.anchor is not None and not any(h for _,h in self.stack):self.anchor.append(data)
  if self.current is not None and not any(h for _,h in self.stack):self.current.append(data)
 def handle_endtag(self,tag):
  if tag=='a' and self.anchor is not None:
   text=' '.join(''.join(self.anchor).split());self.anchor=None
   if len(text)>=20:
    self.links.append(text)
    if self.article:self.article_links.append(text)
  if tag=='p' and self.current is not None:
   text=' '.join(''.join(self.current).split());self.current=None
   if len(text)>=80:
    self.paragraphs.append(text)
    if self.article:self.article_paragraphs.append(text)
  if tag=='article':self.article=max(0,self.article-1)
  for i in range(len(self.stack)-1,-1,-1):
   if self.stack[i][0]==tag:del self.stack[i:];break
p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--capture',required=True);p.add_argument('--output',required=True);a=p.parse_args()
data=pathlib.Path(a.capture).read_bytes();parser=Paragraphs();parser.feed(data.decode('utf8',errors='replace'));paragraphs=parser.article_paragraphs or parser.paragraphs or parser.article_links or parser.links
report={'input_bytes':len(data),'input_sha256':hashlib.sha256(data).hexdigest(),'source_candidate_paragraphs':len(paragraphs),'candidate_scope':'article p' if parser.article_paragraphs else ('visible p' if parser.paragraphs else ('article a' if parser.article_links else 'visible a')),'network':'none; pre-captured bytes only','modes':[]}
for mode in ('reader','simplified'):
 task={'kind':'html','data':base64.b64encode(data).decode(),'url':'https://www.20min.ch/story/captured-fixture','mode':mode};start=time.monotonic();before=resource.getrusage(resource.RUSAGE_CHILDREN);r=subprocess.run([a.binary,'--worker'],input=json.dumps(task).encode(),stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=16);after=resource.getrusage(resource.RUSAGE_CHILDREN)
 if r.returncode:raise SystemExit('worker failed; private output suppressed')
 result=json.loads(r.stdout);row={'mode':mode,'wall_seconds':time.monotonic()-start,'cpu_seconds':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime}
 if 'error' in result:row['error']=result['error'];report['modes'].append(row);continue
 d=result['document'];chunks=d['chunks'];text=' '.join(' '.join(run['text'] for c in chunks for b in c['blocks'] for run in b.get('runs',[])).split());encoded=[json.dumps(c,ensure_ascii=False,separators=(',',':')).encode() for c in chunks]
 row.update(chunks=len(chunks),first_chunk_bytes=len(encoded[0]),all_chunks_bytes=sum(map(len,encoded)),max_chunk_bytes=max(map(len,encoded)),retained_text_bytes=len(text.encode()),retained_text_sha256=hashlib.sha256(text.encode()).hexdigest(),truncated=chunks[0]['truncated'],blocks=sum(len(c['blocks']) for c in chunks),images=len(d['assets']))
 if paragraphs:
  probes=[paragraphs[0],paragraphs[len(paragraphs)//2],paragraphs[-1]];normalized=''.join(text.split());row['coverage_normalization']='whitespace removed; exact Unicode/case';row['candidate_begin_middle_end_covered']=[''.join(x.split())[:70] in normalized for x in probes];row['covered_candidate_paragraphs']=sum(''.join(x.split())[:70] in normalized for x in paragraphs)
 report['modes'].append(row)
pathlib.Path(a.output).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
