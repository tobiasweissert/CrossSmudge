local C=dofile('cp_core.lua')
local J=dofile('cp_journal.lua')
dofile('cp_suits.lua')
local g,journal,notice,selected,confirm,closed
local cursor,index,action=0,0,1
local dirty=false
local layout={}
local view,optionsOpen,records,front,home,hasGame,drawMode,wonPage
local ranks={'A','2','3','4','5','6','7','8','9','10','J','Q','K'}
local suitMap={3,4,1,2}
local function refresh() smudge.request_update() end
local function save()
 if dirty and g then
  if not journal:write(C.save(g))then notice='Save failed; previous journal retained';return false end
  dirty=false
  if records and C.won(g)then records.observe(C,g,true);records.save()end
 end
 return true
end
local function metrics()
 local w,h=smudge.get_bounds();local m=smudge.get_metrics();local top=m.top_padding+m.header_height+38
 if view then layout=view.layout(w,h,g,C);return layout end
 local cw=math.min(70,(w-24)//7-6);local ch=cw*4//3;local x=(w-(cw+6)*7+6)//2
 layout={w=w,h=h,cw=cw,ch=ch,x=x,top=top,tab=top+ch+16,bottom=h-m.button_hints_height-4,buttons={}}
 return layout
end
local function mutate(ok) if ok then dirty=true;selected=nil;notice=C.won(g) and 'Completed! Undo or start another deal' or '';if view and C.won(g)then wonPage=true;save()end;refresh() end end
local function new(three)
 local old=g
 wonPage=false;home=false;hasGame=true;drawMode=three
 g=C.deal(smudge.millis(),three);dirty=true;selected=nil;confirm=nil;cursor=0;index=0;notice='';if save()and records then if old and old.moves>0 and not C.won(old)then records.observe(C,old,false)end;records.begin();records.save()end;refresh()
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
 if smudge.get_memory().lua_max_kb>=512 then view=dofile('board.lua');view.init();front=dofile('front.lua');front.init(view.graphics);records=dofile('records.lua');records.init(J,C);records.pip=view.pip end
 journal,notice=J.open('solitaire',160,function(s) return C.restore(s)~=nil end)
 local saved=notice;notice='';if saved then g=C.restore(saved) else g=C.deal(smudge.millis(),false);dirty=not view end
 hasGame=saved~=nil and not C.won(g);drawMode=g.drawThree;home=view~=nil;wonPage=view and C.won(g)
 if wonPage then home=false;records.observe(C,g,true);records.save()end
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
 if not g then return end
 if wonPage then local w,h=smudge.get_bounds();layout=front.win(w,h,g.moves,records);return end
 if home then local w,h=smudge.get_bounds();layout=front.draw(w,h,hasGame,g.moves,drawMode,records);return end
 local l=metrics()
 if view then view.draw(l,C,g,selected,cursor,cursor==-1 and action or index,notice,optionsOpen or cursor==-1)
 else
 smudge.clear();smudge.header('Solitaire',g.drawThree and 'Draw three' or 'Draw one')
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
 end
 if confirm and view then view.confirm(l.w,l.h,confirm=='three')
 elseif confirm then
  local x,y=24,l.h//2-55;smudge.rect(x,y,l.w-48,110,true,false);smudge.rect(x,y,l.w-48,110,false,2,true)
  smudge.text(l.w//2,y+12,'New draw-'..(confirm=='one' and 'one' or 'three')..' deal?',12,true,'center',true)
  smudge.text(l.w//2,y+54,'Back: cancel   Confirm: deal',10,false,'center',true)
 end
end
local function menuAction(n)
 if n==(hasGame and 3 or 2)then drawMode=not drawMode
 elseif hasGame and n==1 then home=false;selected=nil;cursor=0;index=0
 elseif hasGame then confirm=drawMode and 'three'or'one';home=false;optionsOpen=false
 else new(drawMode)end
 refresh()
end
function on_button(b,pressed)
 if not pressed or closed then return end
 if wonPage then if b=='confirm'then new(drawMode)elseif b=='back'then wonPage=false;home=true;hasGame=false;refresh()end;return end
 if home then if b=='back'then save();closed=true;smudge.exit()elseif b=='confirm'then menuAction(1)elseif b=='right'or b=='page_forward'then menuAction(hasGame and 3 or 2)end;return end
 if confirm then if b=='back' then confirm=nil elseif b=='confirm' then new(confirm=='three') end;refresh();return end
 if optionsOpen then
  if b=='back' then optionsOpen=false;cursor=0
  elseif b=='confirm' then optionsOpen=false;toolbar(action);cursor=0
  elseif b=='up' or b=='left' or b=='page_back' then action=(action-2)%5+1
  elseif b=='down' or b=='right' or b=='page_forward' then action=action%5+1 end
  refresh();return
 end
 if b=='back' then if selected then selected=nil elseif view then save();home=true;hasGame=true;optionsOpen=false else save();closed=true;smudge.exit() end
 elseif b=='confirm' then if cursor==-1 then toolbar(action) else choose(cursor,index) end
 elseif b=='up' or b=='page_back' then if cursor==-1 then action=(action-2)%5+1 else cursor=(cursor+12)%13;index=math.max(0,#g.piles[cursor]-1) end
 elseif b=='down' or b=='page_forward' then if cursor==-1 then action=action%5+1 else cursor=(cursor+1)%13;index=math.max(0,#g.piles[cursor]-1) end
 elseif b=='left' then if cursor>=6 and index>0 then index=index-1 elseif view then save();home=true;hasGame=not C.won(g);cursor=0 else cursor=-1 end
 elseif b=='right' then if cursor==-1 then cursor=0 elseif cursor>=6 and index<#g.piles[cursor]-1 then index=index+1 else cursor=6;index=math.max(0,#g.piles[6]-1) end end
 refresh()
end
function on_touch(x,y,event)
 if event~='tap' or closed then return end
 local l=layout
 if wonPage then for i,b in ipairs(l.won or{})do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then if i==1 then new(drawMode)else wonPage=false;home=true;hasGame=false;refresh()end;return end end;return end
 if home then for i,b in ipairs(l.menu or{})do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then menuAction(i);return end end;return end
 -- A second queued contact can arrive before the requested board repaint.
 if view and(l.menu or l.won)then return end
 if confirm then if y>=l.h//2-55 and y<=l.h//2+55 then if x<l.w//2 then confirm=nil else new(confirm=='three') end;refresh() end;return end
 for n,b in pairs(l.buttons) do if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then optionsOpen=false;cursor=0;toolbar(n);return end end
 if view then
  if optionsOpen or cursor==-1 then return end
  local b=l.menuButton
  if x>=b.x and x<b.x+b.w and y>=b.y and y<b.y+b.h then save();home=true;hasGame=not C.won(g);optionsOpen=false;cursor=0;refresh();return end
 end
 if y>=l.h-smudge.get_metrics().button_hints_height then on_button(({'back','confirm','left','right'})[math.min(4,x*4//l.w+1)],true);return end
 if view and l.waste and x>=l.waste.x and x<l.waste.x+l.waste.w and y>=l.waste.y and y<l.waste.y+l.waste.h then cursor=1;index=math.max(0,#g.piles[1]-1);choose(1,index,true);return end
 local pitch=l.cw+(l.gap or 6)
 local col=(x-l.x)//pitch;if col<0 or col>6 or (x-l.x)%pitch>=l.cw then return end
 if y>=l.top and y<l.top+l.ch then
  local p=col<=1 and col or col>=3 and col-1 or nil
  if view then p=nil;for pile,column in pairs(l.pileColumns) do if column==col then p=pile;break end end end
  if p then cursor=p;index=math.max(0,#g.piles[p]-1);choose(p,index,true) end
 elseif y>=l.tab then local p=col+6;local a=g.piles[p];local step=l.pileSteps[p]
  local lastOffset=view and l.pileOffsets[p][#a]or math.max(0,#a-1)*step
  if y<=l.tab+(lastOffset or 0)+l.ch then
   cursor=p;index=math.min(math.max(0,#a-1),(y-l.tab)//step)
   if view then index=0;for at,offset in ipairs(l.pileOffsets[p])do if y>=l.tab+offset then index=at-1 end end end
   choose(p,index,true)
  end
 end
end
function on_update() if not closed then if save()and records then records.save()end end end
function on_exit() if save()and records then records.save()end;closed=true;selected=nil end
return {state=function() return g end,layout=function() return layout end,records=function()return records end}
