local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
if not ok or device~='sticky'then return dofile('legacy_front.lua')end
-- Original Screens menu: record, last-match ornament, and three bottom doors.
local Q={selected=1,wins=0,losses=0,draws=0};local boxes,nearby={},nil
local seq,slot,dirty,last,observed,armed=0,0,false,nil,nil,false
local paths={'native-record-a.dat','native-record-b.dat'}
local function sum(s)local v=7;for i=1,#s do v=(v*31+s:byte(i))%65521 end;return v end
local function decode(M,raw)
 if type(raw)~='string'or #raw>1200 then return end
 local n,check,body=raw:match('^UI3:(%d+):(%d+)\n(.*)$');n,check=tonumber(n),tonumber(check)
 if not n or math.type(n)~='integer'or n<1 or n>1000000 or sum(body or'')~=check then return end
 local w,l,d,board=body:match('^(%d+) (%d+) (%d+)\n(.*)$');w,l,d=tonumber(w),tonumber(l),tonumber(d)
 if not w or not l or not d then return end
 for _,v in ipairs({w,l,d})do if math.type(v)~='integer'or v>65535 then return end end
 if w+l+d>65535 then return end
 local value=M.decode(board);if not value or value.status==0 then return end
 return n,w,l,d,board
end
function Q.observe(M,g)
 if observed~=g then observed=g;armed=g.status==0 end
 if armed and g.status~=0 then
  armed=false;if Q.wins+Q.losses+Q.draws<65535 then
   if g.status==1 then Q.wins=Q.wins+1 elseif g.status==2 then Q.losses=Q.losses+1 else Q.draws=Q.draws+1 end
  end
  last=M.encode(g);dirty=true
 end
end
function Q.persist()
 if not dirty then return true end
 if seq>=1000000 then smudge.log('UI record sequence exhausted');return false end
 local body=Q.wins..' '..Q.losses..' '..Q.draws..'\n'..last;local raw='UI3:'..(seq+1)..':'..sum(body)..'\n'..body
 local nextSlot=slot==1 and 2 or 1
 if not smudge.write_file(paths[nextSlot],raw)or smudge.read_file(paths[nextSlot],1201)~=raw then smudge.log('UI record save failed');return false end
 seq,slot,dirty=seq+1,nextSlot,false;return true
end
function Q.stats()return {wins=Q.wins,losses=Q.losses,draws=Q.draws}end
-- Decode the fixed-size last-board snapshot only when it changes.
local cached,cachedData
function Q.history(M)if last~=cachedData then cached=last and M.decode(last);cachedData=last end;return cached end
function Q.init(M)
 nearby=assert(smudge.read_file('art/nearby32.raw',128))
 for at,path in ipairs(paths)do local n,w,l,d,board=decode(M,smudge.read_file(path,1201))
  if n and n>seq then seq,slot,Q.wins,Q.losses,Q.draws,last=n,at,w,l,d,board end
 end
end
function Q.home()Q.selected=1 end
function Q.draw(M,g,mode,F,w,h,items,preview)
 F.text((Q.wins+Q.losses+Q.draws)..' PLAYED   '..Q.wins..' WON',16,125,10);smudge.rect(16,151,w-32,3,true,true)
 local history=Q.history(M);if history and preview then preview(M,history,F,{x=16,y=154,w=w-32,h=h-380},Q)end
 local labels={'PLAY','PLAY NEARBY','HOW TO PLAY'};boxes={};local top=h-226
 for i,label in ipairs(labels)do local b={x=16,y=top+(i-1)*66,w=w-32,h=62};boxes[i]=b;local selected=i==Q.selected
  smudge.rect(b.x,b.y,b.w,b.h,true,selected);if not selected then smudge.rect(b.x,b.y,b.w,b.h,false,1,true)end
  F.text(label,b.x+12,b.y+19,20,selected)
  if i==2 then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,nearby,selected)end
 end
end
local function activate()if Q.selected==2 then return 'nearby'end;return 'activate',Q.selected==1 and 1 or 4 end
function Q.button(button)
 if button=='confirm'then return activate()end
 if button=='up'or button=='left'or button=='page_back'then Q.selected=(Q.selected-2)%3+1
 elseif button=='down'or button=='right'or button=='page_forward'then Q.selected=Q.selected%3+1
 else return false end;return true
end
function Q.touch(x,y)
 for i,b in ipairs(boxes)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then Q.selected=i;local kind,id=activate();return kind=='activate'and'menu'or kind,id end end
end
return Q
