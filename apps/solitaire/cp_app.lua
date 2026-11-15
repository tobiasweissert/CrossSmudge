local C=dofile('cp_core.lua')
local J=dofile('cp_journal.lua')
dofile('cp_suits.lua')
local g,journal,notice,selected,confirm,closed
local cursor,index,action=0,0,1
local dirty=false
local layout={}
local ranks={'A','2','3','4','5','6','7','8','9','10','J','Q','K'}
local suitMap={3,4,1,2}
local function refresh() smudge.request_update() end
local function save() if dirty and g then if journal:write(C.save(g)) then dirty=false else notice='Save failed; previous journal retained' end end end
local function metrics()
 local w,h=smudge.get_bounds();local m=smudge.get_metrics();local top=m.top_padding+m.header_height+38
 local cw=math.min(70,(w-24)//7-6);local ch=cw*4//3;local x=(w-(cw+6)*7+6)//2
 layout={w=w,h=h,cw=cw,ch=ch,x=x,top=top,tab=top+ch+16,bottom=h-m.button_hints_height-4,buttons={}}
 return layout
end
local function mutate(ok) if ok then dirty=true;selected=nil;notice=C.won(g) and 'Completed! Undo or start another deal' or '';refresh() end end
local function new(three)
 g=C.deal(smudge.millis(),three);dirty=true;selected=nil;confirm=nil;cursor=0;index=0;notice='';save();refresh()
end
local function choose(p,i,direct)
 if p==0 then mutate(C.drawFromStock(g));return end
 if selected then local s=selected;selected=nil
  if s.p==p then notice='';refresh();return end
  if s.p~=p and C.move(g,s.p,p,s.count) then mutate(true);return end
 end
 local pile=g.piles[p];i=i or #pile-1
 local count=C.runLength(g,p,i)
 if count>0 then
  if not direct and count==1 and not (p>=2 and p<=5) then local f=C.foundationFor(g,p);if f>=0 then mutate(C.move(g,p,f,1));return end end
  selected={p=p,count=count};notice='Select a destination'
 end
 refresh()
end
local function toolbar(n)
 if n==1 then confirm='one' elseif n==2 then confirm='three'
 elseif n==3 then mutate(C.undo(g))
 elseif n==4 then if C.canAutoFinish(g) then mutate(C.autoPlay(g)>0) else notice='Finish unlocks when no cards are hidden' end
 else save();closed=true;smudge.exit() end
 refresh()
end
function on_init()
 journal,notice=J.open('solitaire',160,function(s) return C.restore(s)~=nil end)
 local saved=notice;notice='';if saved then g=C.restore(saved) else g=C.deal(smudge.millis(),false);dirty=true end
 local old,msg=journal:preserve('game_state',4096);journal:preserve('games_won',32)
 if old and not saved then notice=msg end
 closed=false;metrics();J=nil;journal.preserve=nil;on_init=nil;collectgarbage()
end
local function card(x,y,c,sel,focus)
 local l=layout
 smudge.rect(x,y,l.cw,l.ch,true,false);smudge.rect(x,y,l.cw,l.ch,false,1,true)
 if sel or focus then smudge.rect(x+2,y+2,l.cw-4,l.ch-4,false,2,true) end
 if not c then return end
 if not C.isFaceUp(c) then for yy=y+6,y+l.ch-6,5 do smudge.line(x+5,yy,x+l.cw-5,yy,true) end;return end
 smudge.text(x+5,y+3,ranks[C.rankOf(c)+1],10,true,'left',true)
 draw_suit_14(x+l.cw-19,y+4,suitMap[C.suitOf(c)+1],true)
 if l.ch>=65 then draw_suit_22(x+(l.cw-22)//2,y+l.ch-30,suitMap[C.suitOf(c)+1],true) end
end
function on_draw()
 if not g then return end;local l=metrics();smudge.clear();smudge.header('Solitaire',g.drawThree and 'Draw three' or 'Draw one')
 local labels={'New 1','New 3','Undo','Finish','Exit'};local bw=(l.w-24)//5
 for n=1,5 do local x=12+(n-1)*bw;local y=l.top-36;l.buttons[n]={x=x,y=y,w=bw-4,h=30};smudge.rect(x,y,bw-4,30,false,cursor==-1 and action==n and 2 or 1,true);smudge.text(x+(bw-4)//2,y+5,labels[n],10,false,'center',true) end
 local slots={[0]=0,[1]=1,[2]=3,[3]=4,[4]=5,[5]=6}
 for p=0,5 do local a=g.piles[p];card(l.x+slots[p]*(l.cw+6),l.top,a[#a],selected and selected.p==p,cursor==p) end
 l.pileSteps={}
 for p=6,12 do local a=g.piles[p];local step=math.max(3,math.min(24,(l.bottom-34-l.tab-l.ch)//math.max(1,#a-1)));l.pileSteps[p]=step
  local x=l.x+(p-6)*(l.cw+6)
  if #a==0 then card(x,l.tab,nil,false,cursor==p) end
  for i,c in ipairs(a) do card(x,l.tab+(i-1)*step,c,selected and selected.p==p and i>#a-selected.count,cursor==p and i==index+1) end
 end
 smudge.text(12,l.bottom-27,notice~='' and notice or ('Moves '..g.moves..'  |  Top row: stock, waste, foundations'),10,false,'left',true)
 smudge.button_hints('Back','Select','Up','Down')
 if confirm then
  local x,y=24,l.h//2-55;smudge.rect(x,y,l.w-48,110,true,false);smudge.rect(x,y,l.w-48,110,false,2,true)
  smudge.text(l.w//2,y+12,'New draw-'..(confirm=='one' and 'one' or 'three')..' deal?',12,true,'center',true)
  smudge.text(l.w//2,y+54,'Back: cancel   Confirm: deal',10,false,'center',true)
 end
end
function on_button(b,pressed)
 if not pressed or closed then return end
 if confirm then if b=='back' then confirm=nil elseif b=='confirm' then new(confirm=='three') end;refresh();return end
 if b=='back' then if selected then selected=nil else save();closed=true;smudge.exit() end
 elseif b=='confirm' then if cursor==-1 then toolbar(action) else choose(cursor,index) end
 elseif b=='up' or b=='page_back' then if cursor==-1 then action=(action-2)%5+1 else cursor=(cursor+12)%13;index=math.max(0,#g.piles[cursor]-1) end
 elseif b=='down' or b=='page_forward' then if cursor==-1 then action=action%5+1 else cursor=(cursor+1)%13;index=math.max(0,#g.piles[cursor]-1) end
 elseif b=='left' then if cursor>=6 and index>0 then index=index-1 else cursor=-1 end
 elseif b=='right' then if cursor==-1 then cursor=0 elseif cursor>=6 and index<#g.piles[cursor]-1 then index=index+1 else cursor=6;index=math.max(0,#g.piles[6]-1) end end
 refresh()
end
function on_touch(x,y,event)
 if event~='tap' or closed then return end
 local l=layout
 if confirm then if y>=l.h//2-55 and y<=l.h//2+55 then if x<l.w//2 then confirm=nil else new(confirm=='three') end;refresh() end;return end
 for n,b in ipairs(l.buttons) do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then toolbar(n);return end end
 if y>=l.h-smudge.get_metrics().button_hints_height then on_button(({'back','confirm','left','right'})[math.min(4,x*4//l.w+1)],true);return end
 local col=(x-l.x)//(l.cw+6);if col<0 or col>6 or (x-l.x)%(l.cw+6)>=l.cw then return end
 if y>=l.top and y<l.top+l.ch then local p=col<=1 and col or col>=3 and col-1 or nil;if p then cursor=p;index=math.max(0,#g.piles[p]-1);choose(p,index,true) end
 elseif y>=l.tab then local p=col+6;local a=g.piles[p];local step=l.pileSteps[p]
  if y<=l.tab+math.max(0,#a-1)*step+l.ch then cursor=p;index=math.min(math.max(0,#a-1),(y-l.tab)//step);choose(p,index,true) end
 end
end
function on_update() if not closed then save() end end
function on_exit() save();closed=true;selected=nil end
return {state=function() return g end,layout=function() return layout end}
