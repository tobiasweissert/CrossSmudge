#!/usr/bin/env python3
"""Measure an owned native qualification process and all its actual child PIDs. No private bodies are written."""
import argparse,json,os,pathlib,resource,subprocess,threading,time
p=argparse.ArgumentParser();p.add_argument('--binary',required=True);p.add_argument('--output',required=True);p.add_argument('--capture');a=p.parse_args()
env=os.environ.copy();env.update(GOMAXPROCS='1',GOMEMLIMIT='48MiB')
if a.capture:env['SRP_CAPTURE']=a.capture
start=time.monotonic();before=resource.getrusage(resource.RUSAGE_CHILDREN)
proc=subprocess.Popen([a.binary],stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env)
peak={'aggregate_rss_kib':0,'parent_rss_kib':0,'children_rss_kib':0,'max_children':0};stop=threading.Event();seen=set()
def sample():
 while not stop.is_set():
  infos={}
  for path in pathlib.Path('/proc').glob('[0-9]*/status'):
   try:
    values={k:v.strip() for k,v in (line.split(':',1) for line in path.read_text().splitlines() if ':' in line)}
    infos[int(path.parent.name)]=(int(values['PPid']),int(values.get('VmRSS','0 kB').split()[0]))
   except (OSError,KeyError,ValueError):pass
  owned={proc.pid};changed=True
  while changed:
   changed=False
   for pid,(parent,rss) in infos.items():
    if parent in owned and pid not in owned:owned.add(pid);changed=True
  parent=infos.get(proc.pid,(0,0))[1];children=sum(infos.get(pid,(0,0))[1] for pid in owned if pid!=proc.pid)
  peak['aggregate_rss_kib']=max(peak['aggregate_rss_kib'],parent+children);peak['parent_rss_kib']=max(peak['parent_rss_kib'],parent);peak['children_rss_kib']=max(peak['children_rss_kib'],children);peak['max_children']=max(peak['max_children'],len(owned)-1);seen.update(owned-{proc.pid});stop.wait(.002)
thread=threading.Thread(target=sample);thread.start()
try:
 stdout,stderr=proc.communicate(timeout=65)
except subprocess.TimeoutExpired:
 proc.kill();stdout,stderr=proc.communicate();raise
finally:stop.set();thread.join()
after=resource.getrusage(resource.RUSAGE_CHILDREN)
report={'exit':proc.returncode,'wall_seconds':time.monotonic()-start,'child_cpu_seconds':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,'peaks':peak,'observed_worker_pids':sorted(seen),'workers_reaped':all(not pathlib.Path('/proc',str(pid)).exists() for pid in seen),'budget_kib':192*1024,'cgroup_or_500m_enforced':False,'boundary':'host amd64 synthetic HTTP/TLS streams; fake DNS/dial; real extractor/codec/owned subprocesses'}
if proc.returncode==0:report['flow']=json.loads(stdout)
else:report['failure_stderr']=stderr.decode(errors='replace')[:1024]
report['rss_below_budget']=peak['aggregate_rss_kib']<192*1024
pathlib.Path(a.output).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if proc.returncode or not report['rss_below_budget'] or not report['workers_reaped']:raise SystemExit(1)
