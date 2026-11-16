local host=dofile;local app='apps/cp-forehead/';local labels={};local bars={}
dofile=function(p)local m=host(app..p);if p=='graphics.lua' then local text=m.text; m.text=function(label,...)labels[#labels+1]=tostring(label);return text(label,...)end;local rect=m.rect;m.rect=function(b,fill,paper,stroke)if fill and b.w==18 then bars[#bars+1]=b.h end;return rect(b,fill,paper,stroke)end end;return m end
smudge={};setmetatable(smudge,{__index=function()return function()end end})
smudge.get_bounds=function()return 480,800 end;smudge.get_orientation=function()return 'portrait' end
smudge.read_file=function(p,n)local f=io.open(app..p,'rb');if f then local s=f:read(n);f:close();return s end end
local M=dofile('core.lua');local V=dofile('presentation.lua');local g=M.new(17);V.init(g)
local scores={}
for _,n in ipairs({0,9,10,16,100}) do
 g.got=n;g.phase=1;M.finish(g);assert(tonumber(g.bestIn:sub(1,3))==n);g.phase=0;scores[#scores+1]=n;labels={};bars={};V.draw(g,M,'',1)
 assert(table.concat(labels,'|'):find((n==0 and 'NEW' or 'BEST '..n),1,true),'decimal BEST '..n..' absent: '..table.concat(labels,'|'))
 assert(#bars==#scores);for i,value in ipairs(scores) do assert(bars[i]==6+value*194//math.max(1,n),'decimal recent graph value') end
 V.screen='picker';labels={};V.draw(g,M,'',1);assert(table.concat(labels,'|'):find((n==0 and 'NEW' or 'BEST '..n),1,true));V.screen='menu'
end
print('PASS decimal score labels')

-- Real round callbacks and journals retain their existing decimal encoding.
local files={};local now=10000
local read=smudge.read_file;smudge.read_file=function(p,n)return files[p] and files[p]:sub(1,n) or read(p,n)end
smudge.write_file=function(p,s)files[p]=s;return true end
smudge.get_memory=function()return {lua_max_kb=2048}end;smudge.millis=function()return now end
local app=dofile('main.lua');on_init();on_draw()
local function tap(action)
 for _,hit in ipairs(app.view.hit) do if hit.action==action then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('missing Forehead action '..tostring(action))
end
tap(4);tap(1);for i=1,10 do on_button('up',true) end
assert(app.state().got==10);now=now+60001;on_update();on_draw();tap(3)
labels={};on_draw();assert(table.concat(labels,'|'):find('BEST 10',1,true));on_exit()
local encoded=app.core.encode(app.state());app=dofile('main.lua');on_init()
assert(app.core.encode(app.state())==encoded);assert(app.state().recent=='010' and app.state().bestIn:sub(1,3)=='010');on_exit()
print('PASS real ten-answer round, decimal graph values, unchanged journal reload')
