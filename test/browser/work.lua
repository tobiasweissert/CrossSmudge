-- Real browser instruction hook, nested yieldable pcall and native C boundaries.
smudge=browser_native
local host=dofile
local W=host('apps/browser/work.lua')
local completed=0
W.start(function()
 local ok=pcall(function()for i=1,400000 do completed=completed+1 end end);assert(ok)
 -- gsub calls Lua through a non-yieldable native C frame. The hook must defer
 -- its yield until it returns, without longjmp across native owners.
 local result=string.rep('a',2048):gsub('a',function()local n=0;for i=1,50 do n=n+i end;return 'b'end)
 assert(#result==2048)
end)
local steps=0
while W.busy()do steps=steps+1;local ok,err=W.step();assert(ok~=nil,err);assert(steps<1000)end
assert(steps>1 and completed==400000)
W.start(function()while true do completed=completed+1 end end)
assert(W.step()==false);W.cancel();local stopped=completed;assert(W.step()==true and completed==stopped)
W.start(function()error('recoverable job failure')end);local ok,err=W.step();assert(ok==nil and err:find('recoverable',1,true)and not W.busy())
W.start(function()error('must not execute')end);W.job.instructions=40000001;ok,err=W.step();assert(ok==nil and err:find('Work limit',1,true)and not W.busy())
W.start(function()error('must not execute')end);W.job.steps=20000;ok,err=W.step();assert(ok==nil and not W.busy())
print('PASS real browser hook nested pcall/native C boundary/cancel/error/hard budget',steps)
