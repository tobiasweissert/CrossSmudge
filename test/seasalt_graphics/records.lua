local files={};local fail=false
smudge.read_file=function(path,cap)local s=files[path];return s and s:sub(1,cap)end
smudge.write_file=function(path,data)files[path]=fail and data:sub(1,20)or data;return not fail end
local M=dofile('core.lua');local R=dofile('records.lua');R.load();assert(R.played==0)
local g=M.new(789);g.phase=3;g.score={40,30}
R.observe(M,g);assert(R.save());assert(R.played==1 and R.won==1)
for i=1,12 do R.observe(M,g)end;assert(R.played==1)
local old=files['ui-record-a.dat'];R=dofile('records.lua');R.load();R.observe(M,g);assert(R.played==1)
g.seed=999;g.score={20,40};R.observe(M,g);fail=true;assert(not R.save())
local recovery=dofile('records.lua');recovery.load();assert(recovery.played==1 and recovery.won==1)
fail=false;assert(R.save());recovery=dofile('records.lua');recovery.load();assert(recovery.played==2 and recovery.won==1)
local latest=files['ui-record-b.dat'];files['ui-record-b.dat']=latest:sub(1,-3)
recovery=dofile('records.lua');recovery.load();assert(recovery.played==1,'torn record discarded previous valid slot')
assert(files['ui-record-a.dat']==old);print('Sea Salt records deduplicate / interrupted write / recovery passed')
