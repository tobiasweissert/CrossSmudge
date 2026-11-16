-- Actual touch/exit callbacks, production journal, in-memory save overlay.
local host=dofile;local app='apps/cp-notes/';local files={};local writes=0
smudge={};setmetatable(smudge,{__index=function()return function()end end})
smudge.get_bounds=function()return 480,800 end;smudge.get_orientation=function()return 'portrait' end
smudge.get_memory=function()return {lua_max_kb=2048} end;smudge.has_touch=function()return true end
smudge.read_file=function(path,n)if files[path] then return files[path]:sub(1,n) end;local f=io.open(app..path,'rb');if f then local s=f:read(n);f:close();return s end end
smudge.write_file=function(path,s)files[path]=s;writes=writes+1;return true end
smudge.dofile=function(path)return host(app..path) end;dofile=smudge.dofile
local N=dofile('core.lua');local S=dofile('storage.lua');S.scope(1)
local important='- [x] IMPORTANT SAVED EDIT';assert(S.save(important));local saved=files['note-1-a.dat'];writes=0
local api
local function launch()api=dofile('main.lua');on_init();on_draw()end
local function screen()local _,_,s=api.state();return s end
local function tap(action,value)
 for _,hit in ipairs(api.hits()) do if hit.action==action and (value==nil or hit.value==value) then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return true end end
 return false
end
for _,route in ipairs({'menu','back','button','exit'}) do
 launch();assert(tap('help'));assert(screen()=='help')
 if route=='exit' then on_exit() else
  if route=='button' then on_button('back',true);on_draw() else assert(tap(route)) end
  assert(screen()=='library');assert(not tap('option',7) and not tap('option',1),'unloaded note actions exposed')
  on_button('down',true);on_swipe('up');on_exit()
 end
 assert(files['note-1-a.dat']==saved and files['note-1-b.dat']==nil and writes==0,'library help changed saved note')
end
-- Loaded cards retain normal help/menu/save behavior and their journal sequence.
launch();assert(tap('library',1));assert(api.state()==important)
assert(tap('menu'));assert(tap('option',6));assert(screen()=='help')
assert(tap('back'));assert(screen()=='menu');assert(tap('option',1));assert(screen()=='note')
assert(tap('line',1));local edited=api.state();assert(edited~=important)
assert(tap('menu'));assert(tap('option',7));assert(screen()=='library')
assert(files['note-1-a.dat']==saved);assert(files['note-1-b.dat']:match('^CP1:2:'));on_exit()
S.scope(1);assert(S.load(N.validate)==edited and S.seq==2)
launch();assert(tap('library',1));assert(api.state()==edited);on_exit();assert(files['note-1-a.dat']==saved)
-- An empty loaded card still returns from Help to the note menu.
launch();assert(tap('library',2));assert(tap('menu'));assert(tap('option',6));assert(tap('back'));assert(screen()=='menu');on_exit()
print('PASS library help menu/back/exit writes nothing; loaded help, edits and journal reload preserved')
