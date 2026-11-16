-- Stock v1.6.5.3 UI adapter. Tap signature is (x,y,event), per LuaRunner.cpp.
local U={}
local V=dofile('presentation.lua')
function U.install(M)
 local S=dofile('storage.lua')
 local g,mode,screen,cursor,menu,flag,search=nil,1,'menu',1,1,false,nil
 local w,h,left,top,cell,bw,bh=480,800,0,0,0,0,0
 local notice='';local pending=1;local margin=24;local focus=false
 local items={'Resume','Rotate placement','Toggle own / target grid','New game','How to play','Save and exit'}
 local function geometry()
  w,h=smudge.get_bounds();margin=w>h and 64 or 24
  local metrics=smudge.get_metrics()
  local pad=math.max(20,metrics.content_side_padding or 20)
  top=math.max(100,(metrics.top_padding or 0)+(metrics.header_height or 50)+40)
  cell=math.floor(math.min((w-2*pad)/M.cols,(h-top-100)/M.rows))
  bw=cell*M.cols;bh=cell*M.rows;left=math.floor((w-bw)/2)
 end
 local function save()
  local ok=S.save(tostring(mode)..'|'..M.encode(g))
  if not ok then notice='SAVE FAILED - try Save and exit again' end
  if ok and not V.persist(M,g)then notice='RECORD SAVE FAILED - RETRY BEFORE EXIT';return false end
  if ok then V.saved=true end;return ok
 end
 local function refresh()smudge.request_update()end
 local function act()
  if screen~='board' or (not M.solo and mode==1 and g.turn==2) then return end
  if V.place(M,g,cursor)then if g.phase~=0 then cursor=1 end;refresh();return end
  local x=(cursor-1)%M.cols+1;local y=(cursor-1)//M.cols+1
  if M.action(g,x,y,flag) then V.played(cursor);notice='';search=nil;if V.nativePlacement and g.status>0 then save()end;refresh() end
 end
 local function select_menu()
  if menu==1 then if not V.saved then V.begin()end;screen='board'
  elseif menu==2 then g.rotate=1-g.rotate;screen='board'
  elseif menu==3 then g.show=1-g.show;screen='board'
  elseif menu==4 then
   if V.nativePlacement then focus=false;V.begin();g=M.new(smudge.millis());mode=1;cursor=1;flag=false;screen='board';notice='';search=nil
   else pending=1;screen='confirm';menu=1 end
  elseif menu==5 then screen='help';V.helpPage=1
  else if save() then smudge.exit() end end
  refresh()
 end
 function on_init()
  V.init();geometry()
  local loaded=S.load(function(p)
   local m,data=p:match('^([12])|(.*)$');if not m then return nil end
   local value=M.decode(data);if not value then return nil end
   return {g=value,mode=tonumber(m)}
  end)
  V.saved=loaded~=nil;if loaded then g=loaded.g;mode=loaded.mode else g=M.new(smudge.millis());mode=1 end
  -- Older journals could keep the enlarged own-fleet page open. The original
  -- composition always shows that fleet below the target; resume on targets.
  if V.nativePlacement and g.phase==1 then g.show=0 end
  refresh()
 end
 function on_draw()
  V.draw(M,g,mode,screen,cursor,menu,notice,search,items,focus)
 end
 function on_button(btn,pressed)
  if not pressed then return end
  local nav,id=V.button(screen,btn);if nav then if nav=='activate'then menu=id;select_menu()end; if nav=='done'then screen='menu';menu=1 elseif nav=='nearby'then screen='nearby'end;refresh();return end
  if btn=='page_back' then btn='up' elseif btn=='page_forward' then btn='down' end
  if btn=='back' then
   search=nil
   if screen=='menu' then if save() then smudge.exit() end
   elseif screen=='board' then save();screen='menu';menu=1
   else screen='menu';menu=1 end
  elseif screen=='menu' then
   if btn=='up' or btn=='left' then menu=(menu-2)%#items+1
   elseif btn=='down' or btn=='right' then menu=menu%#items+1
   elseif btn=='confirm' then select_menu() end
  elseif screen=='confirm' then
   if btn=='up' or btn=='down' or btn=='left' or btn=='right' then menu=3-menu
   elseif btn=='confirm' then
    if menu==2 then focus=false;V.begin();g=M.new(smudge.millis());mode=pending;cursor=1;flag=false;screen='board';notice=''
    else screen='menu' end
    menu=1
   end
  elseif screen=='board' then
   if g.phase==0 and V.nativePlacement then
    if btn~='confirm'then focus=true end
    if btn=='confirm'then act()
    elseif cursor>100 then
     if btn=='up'then cursor=cursor==101 and 91 or 96
     elseif btn=='left'or btn=='right'then cursor=203-cursor
     elseif btn=='down'then cursor=1 end
    elseif btn=='down'and cursor>90 then cursor=(cursor-1)%10<5 and 101 or 102
    elseif btn=='left'then cursor=(cursor-2)%100+1
    elseif btn=='right'then cursor=cursor%100+1
    elseif btn=='up'then cursor=(cursor-11)%100+1
    elseif btn=='down'then cursor=(cursor+9)%100+1 end
   elseif V.nativePlacement and btn=='confirm'then
    if g.status>0 then menu=4;select_menu()
    elseif V.confirmTarget(g,cursor)then act()end
   elseif btn=='left' then cursor=(cursor-2)%(M.cols*M.rows)+1
   elseif btn=='right' then cursor=cursor%(M.cols*M.rows)+1
   elseif btn=='up' then cursor=(cursor-1-M.cols)%(M.cols*M.rows)+1
   elseif btn=='down' then cursor=(cursor-1+M.cols)%(M.cols*M.rows)+1
   elseif btn=='confirm' then act()
    end
   if V.nativePlacement and g.phase==1 and(btn=='left'or btn=='right'or btn=='up'or btn=='down')then V.focusTarget(g,cursor)end
  end
  refresh()
 end
 function on_touch(x,y,event)
  if event~='tap' then return end
  focus=false
  local kind,id=V.touch(x,y)
  if kind=='back' then on_button('back',true)
  elseif kind=='menu' then menu=id;select_menu()
  elseif kind=='confirm' then menu=id;on_button('confirm',true)
  elseif kind=='cell' then cursor=id;act()
  elseif kind=='aim' then cursor=id;refresh()
  elseif kind=='new' then if V.nativePlacement then menu=4;select_menu()else pending=mode;screen='confirm';menu=1;refresh()end
  elseif kind=='nearby'then screen='nearby';refresh()
  elseif kind=='help'then screen='help';V.helpPage=1;refresh()
  elseif kind=='action' then menu=id;select_menu()
  else refresh() end
 end
 function on_update()
  if screen~='board' or M.solo or mode~=1 or g.turn~=2 or g.status~=0 then return end
  if M.begin then
   if not search then search=M.begin(g);refresh();return end
   local done,col=M.step(search,24)
   if not done then return end
   search=nil;if col then M.action(g,(col-1)%10+1,(col-1)//10+1) end
  else local col=M.choose(g);if col then M.action(g,(col-1)%10+1,(col-1)//10+1) end end
  if V.nativePlacement and g.status>0 then save()end
  refresh()
 end
 function on_swipe(direction)on_button(direction,true)end
 function on_exit()save();search=nil end
 if V.nativePlacement then return {state=function()return g end,layout=V.layout,records=V.records}end
end
return U
