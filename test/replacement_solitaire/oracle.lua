assert(math.maxinteger==2147483647,'test must use firmware signed int32 Lua')
assert(16777216.0==16777217.0,'test must use firmware float32 Lua')
local C=dofile('cp_core.lua')
local function snapshot(g)
 local parts={tostring(g.moves)..','..(C.won(g) and '1' or '0')..','..(C.canAutoFinish(g) and '1' or '0')}
 for p=0,12 do parts[#parts+1]=table.concat(g.piles[p],',') end
 return table.concat(parts,'|')
end
local g,n=nil,0
local function line(s)
 local cmd,expected=s:match('^(.-)\t(.*)$');assert(cmd,'bad oracle record')
 local a={};for x in cmd:gmatch('[^,]+') do a[#a+1]=x end
 if a[1]=='D' then g=C.deal(tonumber(a[2]),a[3]=='1')
 elseif a[1]=='U' then assert(C.undo(g))
 elseif a[1]=='M' then assert(C.move(g,tonumber(a[2]),tonumber(a[3]),tonumber(a[4])))
 elseif a[1]=='S' then C.drawFromStock(g) end
 n=n+1;assert(snapshot(g)==expected,'source state mismatch record '..n..' '..cmd..'\n'..snapshot(g)..'\n'..expected)
 local seen={};local total=0
 for p=0,12 do for _,card in ipairs(g.piles[p]) do local id=card&127;assert(not seen[id]);seen[id]=true;total=total+1 end end
 assert(total==52)
 if n%47==0 then local text=C.save(g);local restored=C.restore(text);assert(restored and snapshot(restored)==expected,'save roundtrip');assert(not C.restore(text..'BAD'),'trailing garbage accepted') end
end
local pending,offset='',0
while true do
 local block=assert(smudge.read_file('oracle.txt',1024,offset));if #block==0 then break end;offset=offset+#block;pending=pending..block
 while true do local at=pending:find('\n',1,true);if not at then break end;line(pending:sub(1,at-1));pending=pending:sub(at+1) end
end
assert(pending=='' and n==2652);print('Solitaire source differential states '..n)
