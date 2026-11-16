assert(smudge.get_device()=='sticky','actual configured identity is not Sticky')
assert(utf8.len('Café')==4 and io==nil and package==nil and debug==nil)
local read,write=smudge.read_file,smudge.write_file
local files,requests,clock,cancelled,failWrite={}, {}, 0, {}, false
smudge.read_file=function(path,n)if path:match('^art/')then return read(path,n)end;return files[path]and files[path]:sub(1,n)end
smudge.write_file=function(path,s)if failWrite then return false end;files[path]=s;return true end
smudge.get_date=function()return{year=2026,month=11,day=16,hour=12,min=0}end
smudge.time=function()return 100000 end
smudge.millis=function()return clock end
smudge.has_feature=function()return true end
smudge.http_request=function(o)requests[#requests+1]=o;return #requests end
smudge.http_cancel=function(id)cancelled[#cancelled+1]=id end
local traceFile
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do
 local native=smudge[name]
 smudge[name]=function(...)
  if name=='bitmap'then local x,y,w,h=...;local vw,vh=smudge.get_bounds();assert(x>=0 and y>=0 and x+w<=vw and y+h<=vh,'glyph outside viewport')end
  if traceFile then local out={name};for i,v in ipairs({...})do out[#out+1]=name=='bitmap'and i==5 and v:gsub('.',function(c)return string.format('%02x',c:byte())end)or tostring(v)end;write(traceFile,table.concat(out,'\t')..'\n',true)end
  return native(...)
 end
end
local app
local T={}
function T.open()app=dofile('main.lua');on_init();on_draw();return app end
function T.state()return app.state()end
function T.view()return app.view end
function T.update()clock=clock+250;on_update()end
function T.requests()return requests end
function T.complete(body,status,error,id)on_http{id=id or #requests,status=status or 200,body=body,error=error};on_draw()end
function T.tap(kind,id)
 on_draw();for _,b in ipairs(app.view.hit)do if b.kind==kind and(id==nil or id==b.id)then on_touch(b.x+b.w//2,b.y+b.h//2,'tap');on_draw();return end end
 error('Missing actual drawn '..kind..':'..tostring(id))
end
function T.button(b)on_button(b,true);on_draw()end
function T.shot(name)
 traceFile=name..'.trace';write(traceFile,'',false);on_draw();traceFile=nil;capture(name..'.pgm')
 write(name..'.state.txt',app.state().screen..'\npage='..app.view.page..'/'..app.view.pages..'\nlist='..app.state().listPage..'\nerror='..app.state().error,false)
end
function T.reset()if on_exit then on_exit()end;files={};requests={};clock=0;return T.open()end
function T.story(id,title,count,kids)
 return '{"id":'..id..',"type":"story","title":"'..(title or'Synthetic story')..'","by":"writer","score":42,"time":99000,"descendants":'..(count or 0)..',"url":"https://example.com/","kids":'..(kids or'[]')..'}'
end

function T.failWrites(value)failWrite=value end
