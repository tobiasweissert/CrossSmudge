-- Real production Browser startup/work queue + production native one-shot bindings.
local host=dofile
http_api={};test_root='saved'
host('test/browser/activity.lua')
for key,value in pairs(native_launch)do smudge[key]=value end
local requested={}
smudge.http_request=function(q)requested[#requested+1]=q;return #requested end
local take,ack=smudge.take_launch_url,smudge.ack_launch_url
local takes,acks=0,0
smudge.take_launch_url=function()
 assert(test_idle(),'URL taken before cache startup finished')
 takes=takes+1;return take()
end
smudge.ack_launch_url=function(token)
 assert(test_idle(),'URL acked while startup work owned Browser')
 acks=acks+1;return ack(token)
end
local function drain()
 for _=1,2000 do on_update();if test_idle()and test_launch_state()~=2 then return end end
 error('launch failed to settle')
end
local target='https://example.com/a#fragment'
test_prepare_launch(target)
dofile('main.lua');assert(test_supports_launch(),'stale Browser lacks launch API')
on_init();assert(takes==0 and acks==0 and #requested==0 and test_launch_state()==2)
drain();assert(takes==1 and acks==1 and test_launch_state()==3)
-- ACK is after enqueue, before outgoing HTTPS completes. Fragment stays off wire.
on_update();assert(#requested==1 and requested[1].url=='https://example.com/a')
for _=1,30 do on_update()end
assert(takes==1 and acks==1 and #requested==1)
on_exit();dofile('main.lua');on_init();drain();on_update();assert(#requested==1,'acknowledged payload replayed on entry')
-- A failed cache startup retains native pending state for Retry/Cancel.
on_exit();test_prepare_launch(target)
local list=smudge.list_files;smudge.list_files=function()error('injected cache startup failure')end
dofile('main.lua');on_init();drain();assert(test_launch_state()==1,'startup error consumed pending URL')
smudge.list_files=list
-- Exit before startup completes cannot acknowledge or lose the pending URL.
on_exit();test_prepare_launch(target);dofile('main.lua');on_init();on_exit();assert(test_launch_state()==1)
-- Manual startup and every existing app see no launch request by default.
test_clear_launch();dofile('main.lua');on_init();for _=1,50 do on_update()end
assert(#requested==1)
-- Marker check is the same production helper invoked by LuaRunner::loadScript.
browser_launch_api=nil;assert(not test_supports_launch())
print('PASS production Browser async startup/one-shot URL queue/ACK/fragment/failed-cache/reentry/manual startup')
