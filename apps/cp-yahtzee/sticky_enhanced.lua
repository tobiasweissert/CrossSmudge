local Y=dofile('core.lua');local S=dofile('storage.lua');local V=dofile('sticky_presentation.lua')
local g=Y.new(1);local mode,screen,selection,menu=1,'menu',6,1
local search=nil;local notice='';local pending=1
local w,h,left,dieW,rowH,cardTop,rows=480,800,20,88,32,240,13
local menuItems={'Resume','New solo game','New two-player game','How to play','Save and exit'}
local function geometry()
 w,h=smudge.get_bounds();left=w>h and 64 or 20;dieW=(w-left-20)//5
 rows=w>h and 7 or 13;cardTop=210;rowH=math.max(16,math.min(32,(h-cardTop-55)//rows))
end
local function save()
 if not S.save(mode..'|'..Y.encode(g)) then notice='Save failed - retry before exit';return false end;V.saved=true;if not V.persist(Y,g)then notice='Record save failed - retry';return false end;return true
end
local function human()return mode==2 or g.turn==1 end
local function redraw()smudge.request_update()end
local function play()
 if not human() or Y.over(g) then return end
 if selection<=5 then Y.hold(g,selection)
 elseif selection==6 then Y.roll(g)
 elseif g.rolls>0 and Y.allowed(g.cards[g.turn],g.d,selection-6)then
  local face=g.d[1];for i=2,5 do if g.d[i]~=face then face=0 end end
  if Y.take(g,selection-6)then V.records.note(face);selection=6;search=nil end
 end
 redraw()
end
local function activate()
 if menu==1 then if Y.over(g)then if not save()then return end;g=Y.new(smudge.millis());V.begin();selection=6;search=nil end;screen='board'
 elseif menu==4 then screen='help';V.page=1
 elseif menu==5 then if save()then smudge.exit()end
 else pending=menu==2 and 1 or 2;screen='new';menu=1 end
 redraw()
end
function on_init()
 V.init(Y);geometry()
 local old=S.load(function(s)
  local m,p=s:match('^([12])|(.*)$');local state=p and Y.decode(p)
  if state then return {mode=tonumber(m),g=state}end
 end)
 V.saved=old~=nil;if old then mode=old.mode;g=old.g else g=Y.new(smudge.millis())end;redraw()
end
function on_draw()
 V.draw(Y,g,mode,screen,selection,menu,notice,search,menuItems)
end
function on_button(btn,pressed)
 if not pressed then return end
 if screen=='board'and(btn=='left'or btn=='right'or btn=='up'or btn=='down')then V.keyboard=true end
 local nav,id=V.button(screen,btn);if nav then if nav=='activate'then menu=id;activate()elseif nav=='nearby'then screen='nearby'end; if nav=='done' then screen='menu';menu=1 end;redraw();return end
 if btn=='page_back' then btn='up' elseif btn=='page_forward' then btn='down' end
 if btn=='back' then
  search=nil
  if screen=='menu' then if save()then smudge.exit()end
  elseif screen=='board' then save();screen='menu';menu=1
  elseif screen=='take' then screen='board'
  else screen='menu';menu=1 end
 elseif screen=='menu' then
  if btn=='up' or btn=='left' then menu=(menu-2)%5+1 elseif btn=='down' or btn=='right' then menu=menu%5+1
  elseif btn=='confirm' then activate()end
 elseif screen=='new' or screen=='take' then
  if btn=='up' or btn=='down' or btn=='left' or btn=='right' then menu=3-menu
  elseif btn=='confirm' then
   if menu==2 then
    if screen=='new' then g=Y.new(smudge.millis());V.begin();mode=pending;selection=6;search=nil
    else Y.take(g,selection-6);selection=6 end
    screen='board'
   else screen=screen=='new' and 'menu' or 'board' end;menu=1
  end
 elseif screen=='board' then
  if Y.over(g)and btn=='confirm'then if save()then g=Y.new(smudge.millis());V.begin();selection=6;search=nil;notice=''end
  elseif btn=='up' or btn=='left' then selection=(selection-2)%19+1
  elseif btn=='down' or btn=='right' then selection=selection%19+1
  elseif btn=='confirm' then play()end
 end
 redraw()
end
function on_touch(x,y,event)
 if event~='tap' then return end;V.keyboard=false
 local kind,id=V.touch(x,y)
 if kind=='back' then on_button('back',true)
 elseif kind=='select' then selection=id;play()
 elseif kind=='new'then if save()then g=Y.new(smudge.millis());V.begin();selection=6;search=nil;notice=''end;redraw()
 elseif kind=='nearby'then screen='nearby';redraw()
 elseif kind=='action'then menu=id;activate()
 elseif kind=='menu' then menu=id;activate()
 elseif kind=='confirm' then menu=id;on_button('confirm',true)
 else redraw()end
end
function on_update()
 if screen~='board' or human() or Y.over(g)then return end
 if g.rolls==0 then Y.roll(g);redraw()
 elseif g.rolls<3 then
  if not search then search=Y.begin(g);redraw();return end
  local done,mask=Y.step(g,search)
  if done then search=nil;g.held=mask;Y.roll(g);redraw()end
 else local c=Y.choose_box(g.cards[g.turn],g.d);if c then Y.take(g,c)end;redraw()end
end
function on_swipe(dir)on_button(dir,true)end
function on_exit()save();search=nil end

return {state=function()return g end,layout=V.layout,records=V.records,screen=function()return screen end}
