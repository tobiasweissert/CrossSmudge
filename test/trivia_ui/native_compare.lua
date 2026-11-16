-- Same native ChoiceModel question/options as native-catalog-references; all
-- state arrives through the existing checked provider and actual callbacks.
local read,write=smudge.read_file,smudge.write_file
local files,id,now={},0,0
smudge.read_file=function(path,n)if path:match('^art/')then return read(path,n)end;return files[path]end
smudge.write_file=function(path,s)files[path]=s;return true end
smudge.get_date=function()return{year=2026,month=11,day=16,hour=12,min=0}end
smudge.millis=function()return now end
smudge.has_feature=function()return true end
smudge.random=function(a,b)return b==4 and 4 or 1 end
smudge.http_request=function()id=id+1;return id end
smudge.http_cancel=function()end
local traceFile
for _,name in ipairs({'clear','rect','rect_dither','rounded_rect','circle','line','bitmap'})do
 local original=smudge[name]
 smudge[name]=function(...)
  if traceFile then local args={...};local out={name};for i=1,#args do out[#out+1]=name=='bitmap'and i==5 and args[i]:gsub('.',function(c)return string.format('%02x',c:byte())end)or tostring(args[i])end;write(traceFile,table.concat(out,'\t')..'\n',true)end
  return original(...)
 end
end
local app=dofile('main.lua');on_init()
local function shot(name)traceFile=name..'.trace';write(traceFile,'',false);on_draw();traceFile=nil;capture(name..'.pgm')end
shot('native-notice')
local function tap(kind,value)
 on_draw();for _,hit in ipairs(app.view.hit)do if hit.kind==kind and (not value or hit.value==value)then local b=hit.box;on_touch(b.x+b.w//2,b.y+b.h//2,'tap');return end end
 error('Missing '..kind)
end
local body='{"response_code":0,"results":[{"type":"multiple","question":"Which%20planet%20is%20known%20as%20the%20Red%20Planet%3F","correct_answer":"Mars","incorrect_answers":["Mercury","Venus","Jupiter"]}]}'
tap('fetch');on_update();on_http{id=id,status=200,body=body};on_draw()
assert(table.concat(app.state().q.answers,'|')=='Mercury|Venus|Mars|Jupiter')
shot('native-choice');tap('choose',2);shot('native-wrong')
now=6000;on_update();tap('fetch');on_update();on_http{id=id,status=200,body=body};tap('choose',3);shot('native-correct')
on_exit()
print('PASS native comparison fixtures came through actual provider/callbacks')
