"""Respect the parent build's PID plus start-ticks qualification receipt."""
import pathlib,json,hashlib,datetime,os

def check():
 declared=os.environ.get('ASSISTED_PARENT_BUILD_GUARD')
 p=pathlib.Path(declared or '/root/github/.host-data/crosssmudge/main-deploy/parent-normal-dio/parent-qualification.json')
 if not p.exists():
  if declared:raise SystemExit('Declared parent build qualification receipt missing; obtain completion metadata before compiling')
  return {'state':'no coordinated parent build declared in this environment'}
 q=json.loads(p.read_text())
 completion=p.parent/'result.json'
 if not completion.exists():raise SystemExit('Parent completion receipt missing; absence from a PID namespace does not prove the build exited')
 done=json.loads(completion.read_text())
 if done.get('pid')!=q['pid']or not isinstance(done.get('exit'),int)or not done.get('lock_released_on_return'):raise SystemExit('Parent build has no matching completed/lock-released receipt')
 stat=pathlib.Path('/proc')/str(q['pid'])/'stat'
 if stat.exists():
  fields=stat.read_text().rsplit(')',1)[1].split()
  if fields[19]==q['start_ticks'] and fields[0]!='Z':raise SystemExit('Parent qualified build remains active; defer heavy host compilation')
 return {'pid':q['pid'],'start_ticks':q['start_ticks'],'exit':done['exit'],'lock_released_on_return':True,'completed_utc':datetime.datetime.fromtimestamp(completion.stat().st_mtime,datetime.timezone.utc).isoformat(),'completion_sha256':hashlib.sha256(completion.read_bytes()).hexdigest(),'state':'matching parent completion receipt; no matching live process visible'}
if __name__=='__main__':print(json.dumps(check()))
