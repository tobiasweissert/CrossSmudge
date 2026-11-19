local host=dofile
local app='apps/cp-trivia/'
dofile=function(p) return host(app..p) end
local M=dofile('model.lua')
local raw='{"response_code":0,"results":[{"type":"multiple","question":"What%20is%20%CF%80%3F","correct_answer":"Number","incorrect_answers":["A","B","C"]}]}'
assert(M.question(raw).question=='What is π?')
assert(not M.question(raw:gsub('%%CF','%%FF')))
assert(not M.question(raw:gsub('"B"','"A"')))
assert(not M.question('{"response_code":5,"results":[]}'))
assert(not M.question(raw:gsub('%%20','%%Q0')))
print('PASS Trivia strict provider shape / percent UTF8 / duplicates / rate refusal')
local files,requests,draws={},{},{}
local id,clock,cancel,exited=0,0,nil,false
smudge={read_file=function(p,n) return files[p] and files[p]:sub(1,n) end,
 write_file=function(p,s) files[p]=s; return true end,
 get_date=function() return {year=2026,month=11,day=15,hour=12,min=0} end,
 random=function(a,b) return b end,millis=function() return clock end,
 has_feature=function() return true end,has_touch=function() return true end,
 http_request=function(o) id=id+1; requests[#requests+1]=o; return id end,
 http_cancel=function(i) cancel=i end,exit=function() exited=true end,request_update=function() end,
 get_bounds=function() return 800,480 end,
 get_metrics=function() return {header_height=40,top_padding=10,button_hints_height=40,content_side_padding=20} end,
 line_height=function() return 20 end,text_width=function(s) return assert(utf8.len(s))*8 end,
 clear=function() draws={} end,header=function() end,button_hints=function() end,
 text=function(x,y,s) assert(y>=50 and y<480);draws[#draws+1]=s end}
dofile('main.lua');on_init();on_draw();on_button('confirm');assert(#requests==0);on_update();assert(#requests==1)
on_http({id=id+1,status=200,body=raw});on_draw();assert(table.concat(draws):find('Fetching'))
on_http({id=id,status=200,body=raw});on_draw();assert(table.concat(draws):find('Number'))
on_button('confirm');on_draw();assert(table.concat(draws):find('What is π?'))
on_button('confirm');on_draw();assert(table.concat(draws):find('Correct!'))
on_button('confirm');on_update();assert(#requests==1)
clock=6000;on_update();on_button('confirm');on_update();assert(#requests==2)
on_http({id=id,status=200,body='{"response_code":5,"results":[]}'});on_draw();assert(table.concat(draws):find('Provider refused'))
clock=12000;on_update();on_button('confirm');on_update();on_exit();assert(cancel==id)
on_http({id=id,status=200,body=raw});on_update();local n=#requests;on_touch(200,470,'tap');assert(#requests==n)
dofile('main.lua');on_init();on_draw();assert(table.concat(draws):find('Offline cache'))
on_touch(200,470,'move');on_draw();assert(not table.concat(draws):find('What is π?'))
on_touch(200,470,'tap');on_draw();assert(table.concat(draws):find('What is π?'))
on_button('back');on_button('back');assert(exited)
print('PASS Trivia lifecycle / cooldown / retry / answer / cached replay / landscape touch')
-- Signed millis rollover, exact cooldown boundary, and long-answer pagination.
clock=2147483640;dofile('main.lua');on_init();on_button('up');on_button('confirm');on_update()
local long=raw:gsub('What%%20is%%20%%CF%%80%%3F',string.rep('Q',1300))
on_http({id=id,status=200,body=long});on_button('confirm');on_draw()
local first=table.concat(draws);on_touch(799,300,'tap');on_draw();assert(table.concat(draws)~=first)
on_button('confirm');clock=-2147477657;on_update();local count=#requests
on_button('confirm');on_update();assert(#requests==count)
clock=-2147477656;on_update();on_button('confirm');on_update();assert(#requests==count+1)
on_exit()
print('PASS Trivia signed timer rollover / cooldown boundary / text pagination')
