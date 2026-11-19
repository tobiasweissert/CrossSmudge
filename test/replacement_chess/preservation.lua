-- APIs/traces are C-backed, like firmware, so instrumentation does not consume game heap.
math.randomseed(12345)
local function call(name,...)
 if _G[name] then print('callback',name,...);_G[name](...) end
end
dofile('main.lua');call('on_init');call('on_draw')
for _,button in ipairs({'down','up','confirm','down','up'}) do
 call('on_button',button,true);call('on_button',button,false);call('on_draw')
end
call('on_touch',1,1,'tap');call('on_tap',1,1);call('on_update');call('on_draw');call('on_exit')
for _,name in ipairs({'on_init','on_draw','on_button','on_touch','on_tap','on_exit','on_update','update'}) do _G[name]=nil end
dofile('main.lua');call('on_init');call('on_draw');call('on_exit')
