-- One owned coroutine. Native instruction/time slices cover all Lua stages,
-- including hostile long tags, CSS, checksums, find and pagination loops.
local W={}
local clock=function()return smudge.millis and smudge.millis()or math.floor(os.clock()*1000)end
local function elapsed(start)return (clock()-start)&0x7fffffff end
function W.start(fn,stage)
 assert(not W.job,'browser work busy')
 W.job={co=coroutine.create(fn),stage=stage or 'Preparing',start=clock(),instructions=0,steps=0}
end
function W.running()return W.job and coroutine.running()==W.job.co end
function W.busy()return W.job~=nil end
function W.stage(stage)if W.job then W.job.stage=stage end end
function W.check()
 if W.job and coroutine.running()==W.job.co then coroutine.yield()end
end
function W.atomic(fn)if smudge.browser_atomic then smudge.browser_atomic(fn)else fn()end end
function W.cancel()
 local job=W.job;W.job=nil
 if job and coroutine.running()~=job.co then coroutine.close(job.co)end
end
function W.step()
 local job=W.job;if not job then return true end
 if elapsed(job.start)>30000 or job.instructions>40000000 or job.steps>=20000 then W.cancel();return nil,'Work limit exceeded; page kept'end
 local ok,err,count=smudge.browser_resume(job.co)
 job.instructions=job.instructions+(count or 0);job.steps=job.steps+1
 if not ok then W.cancel();return nil,tostring(err)end
 if coroutine.status(job.co)=='dead'then W.job=nil;return true end
 return false
end
return W
