local host=dofile;local app='apps/cp-notes/';local files={};local writes=0
smudge={};setmetatable(smudge,{__index=function()return function()end end})
smudge.get_bounds=function()return 480,800 end;smudge.get_orientation=function()return 'portrait' end
smudge.get_memory=function()return {lua_max_kb=2048} end;smudge.has_touch=function()return true end
smudge.read_file=function(path,n)if files[path] then return files[path]:sub(1,n) end;local f=io.open(app..path,'rb');if f then local s=f:read(n);f:close();return s end end
smudge.write_file=function(path,s)files[path]=s;writes=writes+1;return true end
smudge.dofile=function(path)return host(app..path) end;dofile=smudge.dofile
local S=dofile('storage.lua');S.scope(1);assert(S.save('- [x] IMPORTANT SAVED EDIT'));local saved=files['note-1-a.dat'];writes=0
local api=dofile('main.lua');on_init();on_draw()
local function tap(action,value)for _,hit in ipairs(api.hits()) do if hit.action==action and (value==nil or hit.value==value) then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return true end end;return false end
assert(tap('help'));assert(tap('menu'))
tap('option',1);on_button('down',true);on_draw();on_exit();assert(writes==0);print('PASS unloaded help navigation')