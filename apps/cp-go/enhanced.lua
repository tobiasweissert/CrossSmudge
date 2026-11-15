local M=dofile('core.lua');local S=dofile('storage.lua');local F=dofile('graphics.lua');local B=dofile('board.lua')
local Front=dofile('front.lua');local hasSave=false
local g,aim,L;local menu=true;local cursor=1;local savedAt=0;local notice='';local boxes={}
local function redraw()smudge.request_update()end
local function save()local ok=S.save(M.encode(g));notice=ok and '' or 'SAVE FAILED';if ok then hasSave=true end;return ok end
local function changed()aim=nil;if smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end;redraw()end
local function options()
 if g.ui==9 then local _,o=M.view(g);return o end
 return {hasSave and 'RESUME GAME' or 'PLAY','SETTINGS','SAVE AND EXIT'}
end
local function activate()
 if g.ui==9 then M.act(g,cursor);menu=g.ui==9;cursor=1;changed()
 elseif cursor==1 then menu=false;redraw()elseif cursor==2 then g.ui=9;cursor=1;redraw()elseif save()then smudge.exit()end
end
local function play(p)
 if g.phase==1 then M.mark(g,p);changed()
 elseif g.phase==0 and not(g.solo==1 and g.turn==2)then if aim==p then if M.play(g,p)then changed()end else aim=p;redraw()end end
end
local function action(other)
 if g.phase==0 then if not(g.solo==1 and g.turn==2)then M.play(g,0)end
 elseif g.phase==1 then if other then g.phase=0;g.passes=0;g.dead=string.rep('0',#g.b);g.accepted=0 else M.accept(g,(g.accepted&2)==0 and 1 or 2)end
 elseif other then menu=true else local solo=g.solo;g=M.new(g.seed,g.n,g.handicap);g.solo=solo end;changed()
end
function on_init()F.init();g=S.load(M.decode);hasSave=g~=nil;g=g or M.new(smudge.millis());menu=true;M.pause(g);redraw()end
function on_draw()
 local w,h=smudge.get_bounds();smudge.clear();boxes={}
 if menu and g.ui==0 then boxes=Front(M,g,F,w,h,options(),cursor,B.preview,(hasSave and 'SAVED GAME' or 'NEW GAME')..'   '..g.n..'X'..g.n..'   MOVE '..g.moves)
 elseif menu then F.header('SETTINGS');local o=options();local rows=math.max(1,(h-126)//56);local start=((cursor-1)//rows)*rows+1
  for i=start,math.min(#o,start+rows-1)do local b={x=16,y=105+(i-start)*56,w=w-32,h=52};boxes[i]=b;F.pill(F.fit(string.upper(o[i]),w-52,20),b,i==cursor)end
 else L=B.draw(M,g,aim,F,w,h)end
 if notice~=''then F.center(notice,16,h-106,w-32,24,10)end
end
function on_button(b,p)
 if not p then return end
 if menu then if b=='back'then if g.ui==9 then g.ui=0;cursor=1 else if save()then smudge.exit()end end
  elseif b=='confirm'then activate()elseif b=='up'or b=='left'or b=='page_back'then cursor=(cursor-2)%#options()+1 else cursor=cursor%#options()+1 end
 elseif b=='back'then M.pause(g);save();menu=true;cursor=1;aim=nil
 elseif b=='confirm'then if aim then play(aim)else action(false)end
 else local p=aim or(g.n*g.n//2+1);local delta=(b=='up'or b=='page_back')and -g.n or(b=='down'or b=='page_forward')and g.n or b=='left'and -1 or 1;aim=(p-1+delta)%(g.n*g.n)+1 end;redraw()
end
function on_touch(x,y,e)
 if e~='tap'then return end;x,y=math.floor(x),math.floor(y);local w,h=smudge.get_bounds();if x<0 or y<0 or x>=w or y>=h then return end
 if menu then for i,b in pairs(boxes)do if F.inside(b,x,y)then cursor=i;activate();return end end
 elseif y<76 then on_button('back',true)
 elseif F.inside(L.pass,x,y)then action(false)
 elseif L.other and F.inside(L.other,x,y)then action(true)
 else local p=B.hit(L,g.n,x,y);if p then play(p)end end
end
function on_update()if not menu and g.phase==0 and g.solo==1 and g.turn==2 then M.play(g,M.ai(g));changed()end end
function on_swipe(d)on_button(d,true)end
function on_exit()M.pause(g);save()end
