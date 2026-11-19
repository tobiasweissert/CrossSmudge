local ok,device=pcall(function()return smudge.get_device and smudge.get_device()end)
if not ok or device~='sticky'then return dofile('legacy_front.lua')end
-- Exact native StartModel row/record/miniature composition. Nearby reports the
-- missing runtime service explicitly; it never fabricates discovery or peers.
local R=dofile('records.lua');local Q={selected=1};local rows,boxes,nearby={},{},nil
function Q.init()R.load();nearby=assert(smudge.read_file('art/nearby32.raw',128))end
function Q.home()Q.selected=1 end
function Q.observe(g)R.observe(g)end
function Q.begin()R.begin()end
function Q.persist()return R.save()end
function Q.records()return R end
function Q.draw(M,g,mode,F,w,h,items,preview)
 rows=Q.hasSave and{{'CONTINUE',1},{'NEW GAME',4},{'PLAY NEARBY',0}}or{{'NEW GAME',4},{'PLAY NEARBY',0}}
 Q.selected=math.min(Q.selected,#rows);boxes={}
 F.text(R.played..' PLAYED   '..R.won..' WON   STREAK '..R.streak,16,125,10)
 smudge.rect(16,151,w-32,3,true,true)
 local listHeight=#rows*62+(#rows-1)*6+12;local top=h-16-listHeight
 if preview then preview(M,g,F,{x=16,y=145,w=w-32,h=top-145})end
 for i,data in ipairs(rows)do local b={x=16,y=top+(i-1)*66,w=w-32,h=62};boxes[i]=b;local selected=i==Q.selected
  smudge.rect(b.x,b.y,b.w,b.h,selected,1,true);F.text(data[1],b.x+12,b.y+19,20,selected)
  if data[2]==0 then smudge.bitmap(b.x+b.w-56,b.y+15,32,32,nearby,selected)end
 end
end
local function activate()
 local id=rows[Q.selected]and rows[Q.selected][2]
 if id==0 then return 'nearby'end
 return 'activate',id or 1
end
function Q.button(button)
 if button=='confirm'then return activate()end
 if button=='up'or button=='left'or button=='page_back'then Q.selected=(Q.selected-2)%math.max(1,#rows)+1
 elseif button=='down'or button=='right'or button=='page_forward'then Q.selected=Q.selected%math.max(1,#rows)+1
 else return false end
 return true
end
function Q.touch(x,y)
 for i,b in ipairs(boxes)do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then Q.selected=i;local kind,id=activate();return kind=='activate'and'menu'or kind,id end end
end
return Q
