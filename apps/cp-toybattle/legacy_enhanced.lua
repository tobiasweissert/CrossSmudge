local M=dofile('core.lua');local S=dofile('storage.lua');local F=dofile('graphics.lua');local B=dofile('board.lua')
local Front=dofile('front.lua');local Help=dofile('lesson.lua');local home=true;local settings=false;local helpPage;local hasSave=false
local g,L;local menu=true;local cursor=1;local chosen,slot;local boxes={};local savedAt=0;local notice=''
local function redraw()smudge.request_update()end
local function save()local ok=S.save(M.encode(g));notice=ok and ''or 'SAVE FAILED';if ok then hasSave=true end;return ok end
local function changed()if smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end;redraw()end
local function activate()
 if home then
  if cursor==1 then menu=false;home=false
  elseif cursor==2 then M.act(g,g.phase==1 and 2 or g.turn==2 and 3 or 4);home=false;cursor=1
  elseif cursor==3 then helpPage=1;home=false;cursor=1
  else home=false;settings=true;cursor=1 end;redraw();return
 end
 if settings then
  if cursor==1 then M.act(g,g.phase==1 and 1 or g.turn==2 and 2 or 3);settings=false;cursor=1
  elseif cursor==2 then M.act(g,g.phase==1 and 2 or g.turn==2 and 3 or 4);settings=false;cursor=1
  elseif cursor==3 then if save()then smudge.exit()end
  else settings=false;home=true;cursor=1 end;redraw();return
 end
 local result=M.act(g,cursor);cursor=1;chosen=nil;slot=nil;menu=g.ui~=0
 if result=='exit'then M.pause(g);if save()then smudge.exit()end else changed()end
end
local function plan()
 if g.phase~=0 or g.turn~=1 then return end
 M.act(g,2);g.edit[1]=slot or 1;g.edit[2]=chosen or 1;menu=true;cursor=1;redraw()
end
function on_init()F.init();g=S.load(M.decode);hasSave=g~=nil;g=g or M.new(smudge.millis());home=true;settings=false;menu=true;helpPage=nil;M.pause(g);redraw()end
function on_draw()
 local w,h=smudge.get_bounds();smudge.clear();boxes={}
 if helpPage then boxes=Help.draw(helpPage,F,w,h,B)
 elseif menu and home then boxes=Front(M,g,F,w,h,{hasSave and 'CONTINUE'or'PLAY','NEW GAME','HOW TO PLAY','SETTINGS'},cursor,B.preview,M.board(g).name..'   '..g.medals[1]..' - '..g.medals[2])
 elseif menu then
  local lines,o=M.view(g);if settings then lines={'CURRENT GAME'};o={'MAP / EFFECTS','NEW GAME','SAVE AND EXIT','BACK'}end;F.header(settings and 'SETTINGS'or'TOY BATTLE');F.center(F.fit(string.upper(lines[#lines]),w-32,10),16,95,w-32,22,10)
  local rows=math.max(1,(h-143)//52);local start=((cursor-1)//rows)*rows+1
  for i=start,math.min(#o,start+rows-1)do local b={x=16,y=125+(i-start)*52,w=w-32,h=48};boxes[i]=b;F.pill(F.fit(string.upper(o[i]),w-52,20),b,i==cursor)end
 else L=B.draw(M,g,chosen,slot,F,w,h)end
 if notice~=''then F.center(notice,16,h-108,w-32,24,10)end
end
function on_button(b,p)
 if not p then return end
 if helpPage then
  if b=='back'then helpPage=nil;home=true;cursor=1
  elseif b=='left'or b=='up'or b=='page_back'then if helpPage==1 then helpPage=nil;home=true else helpPage=helpPage-1 end
  elseif b=='confirm'or b=='right'or b=='down'or b=='page_forward'then if helpPage==8 then helpPage=nil;menu=false;home=false else helpPage=helpPage+1 end end;redraw();return
 end
 if b=='back'then if menu then if home then if save()then smudge.exit()end else M.pause(g);settings=false;home=true;cursor=1 end elseif chosen then chosen=nil;slot=nil else save();home=true;menu=true;cursor=1 end elseif menu then
  local _,o=M.view(g);if home or settings then o={1,2,3,4}end;if b=='confirm'then activate()elseif b=='up'or b=='left'or b=='page_back'then cursor=(cursor-2)%#o+1 else cursor=cursor%#o+1 end
 elseif b=='confirm'then if chosen then plan()else menu=true;cursor=1 end
 else local kinds=L.kinds;if #kinds>0 then local index=1;for i,k in ipairs(kinds)do if k==chosen then index=i end end;local delta=(b=='up'or b=='left'or b=='page_back')and -1 or 1;chosen=kinds[(index-1+delta)%#kinds+1] end
 end;redraw()
end
function on_touch(x,y,e)
 if e~='tap'then return end;x,y=math.floor(x),math.floor(y);local w,h=smudge.get_bounds();if x<0 or y<0 or x>=w or y>=h then return end
 if helpPage then for i,b in ipairs(boxes)do if F.inside(b,x,y)then on_button(i==1 and 'left'or'right',true);return end end
 elseif menu then for i,b in pairs(boxes)do if F.inside(b,x,y)then cursor=i;activate();return end end
 elseif y<76 then on_button('back',true)
 elseif F.inside(L.brief,x,y)then g.ui=1;menu=true;cursor=1;redraw()
 elseif F.inside(L.action,x,y)then if g.phase==1 then menu=true;cursor=1 elseif chosen then chosen=nil;slot=nil elseif g.turn==1 then M.act(g,1);changed()end;redraw()
 else local kind,id=B.hit(L,x,y)
  if kind=='troop'and id and g.turn==1 and g.phase==0 then chosen=id;slot=nil;redraw()
  elseif kind=='slot'then slot=id;if chosen then plan()else g.ui=1;menu=true;cursor=1;redraw()end end
 end
end
function on_update()if not menu and g.phase==0 and g.turn==2 then M.act(g,1);changed()end end
function on_swipe(d)on_button(d,true)end
function on_exit()M.pause(g);save()end
