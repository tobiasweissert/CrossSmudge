-- All literals below are SYNTHETIC adversarial fixtures, not captured responses.
local J = dofile('apps/cp-hackernews/json.lua')
local function bad(s) assert(J.decode(s)==nil, s) end
assert(J.decode('{"a":[1,true,null,"\\uD83D\\uDE00"]}').a[4]=='😀')
assert(J.decode('2147483647')==2147483647)
for _,s in ipairs({'01','1.','1e','[1,]','{"a":1,"a":2}','"\\uD800"','"\\uDC00"','"\255"','"\0"','2147483648',string.rep('[',14)..string.rep(']',14)}) do bad(s) end
print('PASS strict bounded JSON / int32 / UTF8')
local hostDofile=dofile
local app='apps/cp-hackernews/'
dofile=function(path) return hostDofile(app..path) end
local files,requests,cancels,draws={},{},{},{}
local nextId,inside,failWrite,exited=0,false,false,false
smudge={
  read_file=function(path,n) assert(not path:find('%.%.') and not path:find('/')); return files[path] and files[path]:sub(1,n) end,
  write_file=function(path,body) files[path]=failWrite and body:sub(1,20) or body; return not failWrite end,
  get_date=function() return {year=2026,month=11,day=15,hour=12,min=0} end,
  has_touch=function() return true end,
  has_feature=function() return true end,
  http_request=function(opts) assert(not inside,'request during completion'); nextId=nextId+1; requests[#requests+1]=opts; return nextId end,
  http_cancel=function(id) cancels[#cancels+1]=id; return true end,
  request_update=function() end, exit=function() exited=true end,
  get_bounds=function() return 480,800 end,
  get_metrics=function() return {content_side_padding=20,header_height=40,top_padding=10,button_hints_height=40} end,
  text_width=function(s) assert(utf8.len(s)); return utf8.len(s)*8 end,
  line_height=function() return 20 end,
  clear=function() draws={} end,header=function() end,button_hints=function() end,
  text=function(x,y,s) assert(x>=0 and y>=50 and y<800); assert(utf8.len(s)*8<=440); draws[#draws+1]=s end
}
local C=dofile('cache.lua')
local c=C.open('test',function(s) return s=='old' or s=='new' end)
assert(c:save('old')); failWrite=true; assert(not c:save('new')); failWrite=false
assert(C.open('test',function() return true end).body=='old')
assert(c:save('new')); assert(C.open('test',function() return true end).body=='new')
local latest=c.name..c.slot..'.cache'
files[latest]=files[latest]:gsub('2026%-11','2027-11')
assert(C.open('test',function() return true end).body=='old')
print('PASS cache checked readback / interrupted writes / recovery')
local M=dofile('model.lua')
assert(M.ids('[11,22]')[2]==22)
assert(not M.ids('[11,11]')); assert(not M.ids('{"1":11}'))
assert(not M.item('{"id":1,"title":true,"type":"story"}'))
assert(not M.article_url('https://127.0.0.1/a'))
assert(not M.article_url('https://example.com@evil.test/a'))
assert(M.plain('<p>A &amp; &#x1F600;</p>')=='\nA & 😀')
dofile('main.lua'); on_init(); on_draw()
-- Up wraps from Read to Refresh. Completion must defer the item request.
on_button('up'); on_button('confirm'); assert(#requests==0); on_update(); assert(#requests==1)
local function complete(body,id,status,err)
  inside=true; on_http({id=id or nextId,status=status or 200,body=body,error=err}); inside=false
end
complete('[11,22]'); assert(#requests==1); on_update(); assert(#requests==2)
complete('{"id":99,"type":"story","title":"Wrong story"}')
on_draw(); assert(table.concat(draws):find('No story loaded'))
-- Refresh and load correct item.
on_button('confirm'); on_update(); complete('[11,22]'); on_update()
local item='{"id":11,"type":"story","title":"Real-shaped synthetic story","url":"https://example.com/","text":"<p>Hello &amp; goodbye</p>"}'
complete(item); on_draw(); assert(table.concat(draws):find('Real%-shaped'))
-- Delayed unrelated completion cannot replace the current story.
complete('{"id":22}',1); on_draw(); assert(table.concat(draws):find('Real%-shaped'))
on_button('down'); on_button('confirm'); on_draw(); assert(table.concat(draws):find('Hello & goodbye'))
on_button('back'); on_button('down'); on_button('confirm'); on_update()
assert(requests[#requests].url=='https://r.jina.ai/https://example.com/')
complete(string.rep('Read this 😀 long article. ',100)); on_draw()
local first=table.concat(draws); on_touch(479,500,'move'); on_draw(); assert(table.concat(draws)==first)
on_touch(479,500,'tap'); on_draw(); assert(table.concat(draws)~=first)
on_button('back'); on_button('down'); on_button('down'); on_button('confirm'); on_update()
assert(requests[#requests].url:find('/22.json',1,true))
local cancelled=nextId; on_button('back'); assert(cancels[#cancels]==cancelled)
complete('{"id":22,"type":"story","title":"Late"}',cancelled)
on_exit(); local count=#requests; on_update(); on_draw(); on_touch(10,780,'tap'); assert(#requests==count)
-- Reopen shows the saved story; failed refresh preserves it.
dofile('main.lua'); on_init(); on_draw(); assert(table.concat(draws):find('Real%-shaped'))
on_button('up'); on_button('confirm'); on_update(); complete(nil,nil,0,'time_untrusted'); on_draw()
assert(table.concat(draws):find('Trust verified'))
on_button('back'); assert(exited)
print('PASS HN lifecycle / next-update sequencing / identity / offline / buttons / touch / paging')

-- Menu must retain visible action even when a valid provider title has controls.
exited=false;dofile('main.lua');on_init();on_button('up');on_button('confirm');on_update();complete('[11]');on_update()
complete('{"id":11,"type":"story","title":"'..string.rep('\\n',80)..'Title"}')
on_draw();assert(table.concat(draws):find('Refresh feed'))
on_exit()
print('PASS menu newline title / cache metadata corruption')
assert(J.decode('-2147483648')==-2147483648)
for _,s in ipairs({'"\237\160\128"','"\244\144\128\128"',string.rep(' ',8193)}) do bad(s) end
smudge.http_request=function() return nil,'permission_denied' end
dofile('main.lua');on_init();on_button('up');on_button('confirm');on_update();on_draw()
assert(table.concat(draws):find('permission_denied'));on_exit()
print('PASS immediate submission rejection / UTF8 scalar bounds')
