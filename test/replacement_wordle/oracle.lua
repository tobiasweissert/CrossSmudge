assert(math.maxinteger==2147483647,'test must use firmware signed int32 Lua')
assert(16777216.0==16777217.0,'test must use firmware float32 Lua')
local C=dofile('cp_core.lua')
local n=0
local function line(s)
 local a={};for x in s:gmatch('[^,]+') do a[#a+1]=x end
 if a[1]=='S' then local marks=C.score(a[2],a[3]);assert(table.concat(marks)==a[4],'duplicate scoring mismatch '..s)
 elseif a[1]=='T' then local bytes={};for pair in a[3]:gmatch('..') do bytes[#bytes+1]=string.char(tonumber(pair,16)) end;local stats=C.statsFor(table.concat(bytes),tonumber(a[2]));assert(stats.played==tonumber(a[4]) and stats.won==tonumber(a[5]) and stats.streak==tonumber(a[6]) and table.concat(stats.wins,'/')==a[7],'stats mismatch '..s)
 else local d,y,m,day=tonumber(a[2]),tonumber(a[3]),tonumber(a[4]),tonumber(a[5]);assert(C.dayIndex(y,m,day)==d);local yy,mm,dd=C.dateOfDay(d);assert(yy==y and mm==m and dd==day,'date mismatch '..s) end
 n=n+1
end
local pending,offset='',0
while true do local b=assert(smudge.read_file('oracle.txt',1024,offset));if #b==0 then break end;offset=offset+#b;pending=pending..b;while true do local at=pending:find('\n',1,true);if not at then break end;line(pending:sub(1,at-1));pending=pending:sub(at+1) end end
assert(n==2251 and pending=='');print('Wordle source differential cases '..n)
local g=C.start(50,'APPLE');assert(C.submit(g,function()return true end)=='Short')
for c in ('ALLEY'):gmatch('.') do assert(C.type(g,c)) end
assert(C.submit(g,function()return false end)=='NotAWord');assert(g.typing=='ALLEY')
assert(C.submit(g,function()return true end)=='Scored');assert(g.typing=='')
assert(g.keys.A==4 and g.keys.L==3 and g.keys.E==3)
for c in ('APPLE'):gmatch('.') do assert(C.type(g,c)) end
assert(C.submit(g,function()return true end)=='Won');assert(C.submit(g,function()return true end)=='Over');assert(not C.type(g,'X'))
local t=C.save(g);local restored=C.load(50,t);assert(restored and restored.status=='Won' and #restored.rows==2)
assert(not C.load(50,t..' APPLE'));assert(not C.load(50,'APP1E'));assert(not C.load(50,'APPLE XXX'))
local loss=C.start(1,'APPLE');for i=1,6 do for c in ('CRANE'):gmatch('.') do C.type(loss,c) end;assert(C.submit(loss,function()return true end)==(i==6 and 'Lost' or 'Scored')) end
assert(C.load(1,C.save(loss)).status=='Lost')
local day,answer=C.parseAnswerLine('2021-06-19 apple \r\n');assert(day==0 and answer=='APPLE')
assert(not C.parseAnswerLine('2021-06-19 APP1E'));assert(not C.parseAnswerLine('2021-13-19 APPLE'))
local jsonDay,jsonAnswer=C.parseDailyJson('{"solution":"apple","print_date":"2021-06-19","unused":17}')
assert(jsonDay==0 and jsonAnswer=='APPLE');assert(not C.parseDailyJson('{"solution":"apple"}'))
local archive=C.putAnswer('',4,'APPLE');assert(#archive==25 and C.lastKnownDay(archive)==4 and C.answerFor(archive,4)=='APPLE');assert(not C.answerFor(archive,0));assert(not C.answerFor(archive,-1))
archive=C.putAnswer(archive,0,'CRANE');assert(C.answerFor(archive,0)=='CRANE' and C.lastKnownDay(archive)==4)
local list='ALLEYAPPLECRANESTONE';for _,w in ipairs({'ALLEY','APPLE','CRANE','STONE'}) do assert(C.isWord(list,w)) end
for _,w in ipairs({'AAAAA','APPLY','ZZZZZ','STONY'}) do assert(not C.isWord(list,w)) end
assert(not C.isWord('', 'APPLE'))
for r=0,255 do local results=C.putResult('',5,r);assert(#results==6 and C.resultOf(results,5)==r and C.resultOf(results,4)==0 and C.resultOf(results,-1)==0) end
print('Wordle parser/archive/dictionary/result checks passed')
