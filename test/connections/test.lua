collectgarbage('incremental',105,250,0)
local path='apps/cp-connections/'
local J,A=dofile(path..'json.lua'),dofile(path..'archive.lua')
local fixture=os.getenv('CONNECTIONS_FIXTURE') or '/root/github/.host-data/crosssmudge/lua-network-apps/public-fixtures-parent/connections.body'
local f=assert(io.open(fixture,'rb'));local total=assert(f:seek('end'));assert(total==1460742)
local reads,maxread=0,0
local function read(offset,n)
  assert(n<=1024);reads=reads+1;maxread=math.max(maxread,n)
  assert(f:seek('set',offset));return f:read(n) or '',total
end
local function scan(day)
  local s=A.new(read,J.decode,day);local done,p,e
  repeat done,p,e=s:step() until done
  assert(s.records==1248,e);return p,e
end
local latest=assert(scan());assert(latest.date=='2026-11-15')
local daily=assert(scan('2024-01-01'));assert(daily.date=='2024-01-01')
assert(not scan('2025-04-01')) -- actual captured record has sixteen empty members
assert(not scan('2026-11-16'));f:close()
assert(J.decode('"\\ud83d\\ude00"')=='\240\159\152\128')
for _,raw in ipairs({'"\\ud800"','"\192\128"','{"a":1,"a":2}','[1,]','['..string.rep('[',10)..'0'..string.rep(']',10)..']'}) do assert(J.decode(raw)==nil) end
local function invalid(raw)
  local s=A.new(function(o,n)return raw:sub(o+1,o+n),#raw end,J.decode)
  local done,p,e;repeat done,p,e=s:step() until done;assert(not p and e)
end
for _,raw in ipairs({'[','[]junk','[{},]','["oops"]','['..string.rep('{',10),'[{"a":"'..string.rep('x',5000)..'"}]'}) do invalid(raw) end
J,A=nil,nil;collectgarbage('collect')
local G,S=dofile(path..'game.lua'),dofile(path..'save.lua')
local g=G.new(latest,123)
local oracle=assert(io.open('/root/github/.host-data/crosssmudge/http-followup/oracle.txt'))
assert(table.concat(g.order,',')==oracle:read('*l'))
local names={invalid=0,wrong=1,['one away']=2,solved=3,['already guessed']=4}
local result={playing=0,won=1,lost=2}
for _,mask in ipairs({23,23,15,240,3840,61440}) do
  g.chosen={};for _,t in ipairs(g.order) do if mask&(1<<(t-1))~=0 then g.chosen[t]=true end end
  local r=g:submit()
  assert(table.concat({names[r],g.mistakes,#g.solved,result[g.result]},',')==oracle:read('*l'))
  local reopened=assert(G.restore(latest,g:encode()));assert(reopened:encode()==g:encode());g=reopened
end
oracle:close();assert(g.result=='won')
local encoded=g:encode();local name=latest.answers[1].group;latest.answers[1].group='corrected group';assert(not G.restore(latest,encoded));latest.answers[1].group=name
local lost=G.new(latest,4)
for _,mask in ipairs({23,39,71,135}) do
  lost.chosen={};for _,t in ipairs(lost.order) do if mask&(1<<(t-1))~=0 then lost.chosen[t]=true end end
  assert(lost:submit()=='one away')
end
assert(lost.result=='lost' and G.restore(latest,lost:encode()).mistakes==4)
local files={['unrelated-user-save']='KEEP'};local short=false
smudge={read_file=function(p,n)assert(n<=1024);return files[p] end,write_file=function(p,s) files[p]=short and s:sub(1,10) or s;return not short end}
local validate=function(raw)return G.restore(latest,raw)~=nil end
local save=S.open(validate);assert(save:write(g:encode()));local old=save.body
short=true;assert(not save:write(lost:encode()));assert(S.open(validate).body==old)
short=false;assert(save:write(lost:encode()));assert(S.open(validate).body==lost:encode());assert(files['unrelated-user-save']=='KEEP')
-- Actual main.lua lifecycle with bounded native-like reads, stale callbacks, explicit download only.
A,J,G,S,g,lost,save,latest,daily,validate=nil,nil,nil,nil,nil,nil,nil,nil,nil,nil
collectgarbage('collect')
local rawread=read
f=assert(io.open(fixture,'rb'))
smudge.download_read=rawread
smudge.random=function(a,b)assert(a==0 and b==65535);return 123 end
smudge.request_update=function()end
smudge.has_feature=function()return true end
smudge.get_device=function()return 'sticky' end
smudge.get_date=function()return {year=2026,month=11,day=15} end
smudge.get_bounds=function()return 480,800 end
smudge.get_metrics=function()return {header_height=40,top_padding=8,button_hints_height=30,content_side_padding=10} end
smudge.line_height=function()return 20 end
smudge.clear=function()end;smudge.header=function()end
smudge.text=function(x,y,s)assert(type(x)=='number' and type(y)=='number' and type(s)=='string')end
smudge.wrapped_text=function(s,width,font,style,max)assert(type(s)=='string' and width>0 and max<=2);return {s}end
smudge.has_touch=function()return true end;smudge.button_hints=function()end
local cancelled=0;smudge.http_cancel=function()cancelled=cancelled+1 end;smudge.exit=function()end
local orig=dofile;dofile=function(p)return orig(path..p)end
assert(loadfile(path..'main.lua'))();on_init()
for i=1,1500 do on_update() end
on_draw();on_button('confirm');on_touch(20,100,'tap');on_button('down');on_draw()
on_http{id=1234,kind='download',status=200,size=total};on_exit();on_http{id=1234,error='cancelled'};on_update();on_draw()
assert(cancelled==0);f:close();dofile=orig
print('PASS captured archive latest/daily/missing/unplayable, UTF8/depth/size rejection, original C++ rules oracle, repeat/lose/win/restart, interrupted saves, app lifecycle; reads '..reads..' max '..maxread)
