-- Minesweeper graphics and controls; rules and journal remain separate.
local U={}
function U.install(M)
 local S=dofile('storage.lua')
 collectgarbage('collect')
 local G=dofile('graphics.lua')
 local keyboard=false
 local lesson=1
 local g,mode,screen,cursor,menu,flag=nil,1,'menu',1,1,false
 local w,h,left,top,cell,bw,bh=480,800,0,0,0,0,0
 local notice=''
 local items={'RESUME','TOGGLE DIG / FLAG','NEW GAME','HOW TO PLAY','SAVE AND EXIT'}
 local function geometry()
  w,h=smudge.get_bounds()
  top=84
  cell=math.floor(math.min(w/M.cols,(h-top-116)/M.rows))
  bw=cell*M.cols;bh=cell*M.rows;left=math.floor((w-bw)/2)
 end
 local function save()
  local ok=S.save(tostring(mode)..'|'..M.encode(g))
  if not ok then notice='SAVE FAILED - try Save and exit again' end
  return ok
 end
 local function refresh()smudge.request_update()end
 local function act()
  if screen~='board' then return end
  local x=(cursor-1)%M.cols+1;local y=(cursor-1)//M.cols+1
  if M.action(g,x,y,flag) then notice='';refresh() end
 end
 local function select_menu()
  if menu==1 then screen='board'
  elseif menu==2 then flag=not flag;screen='board'
  elseif menu==#items then if save() then smudge.exit() end
  elseif menu==#items-1 then screen='help'
  else screen='confirm';menu=1 end
  refresh()
 end
 function on_init()
  geometry();collectgarbage('collect');G.init()
  local loaded=S.load(function(p)
   local m,data=p:match('^([12])|(.*)$');if not m then return nil end
   local value=M.decode(data);if not value then return nil end
   return {g=value,mode=tonumber(m)}
  end)
  if loaded then g=loaded.g;mode=loaded.mode else g=M.new(smudge.millis());mode=1 end
  refresh()
 end
 function on_draw()
  geometry();smudge.clear()
  smudge.rect(0,0,w,76,true)
  G.text('MINESWEEPER',16,19,30,true)
  if screen=='menu' then
   for i,item in ipairs(items) do
    local y=top+(i-1)*56
    smudge.rect(16,y,w-32,48,i==menu)
    G.center(item,16,y,w-32,48,20,i==menu)
   end
  elseif screen=='confirm' then
   G.center('REPLACE CURRENT BOARD?',0,top,w,50,20)
   G.center(menu==1 and '> CANCEL' or 'CANCEL',0,top+55,w,45,20)
   G.center(menu==2 and '> START NEW GAME' or 'START NEW GAME',0,top+103,w,45,20)
  elseif screen=='help' then
   G.lesson(lesson,w,h,top)
  else
   smudge.rect(left,top-3,bw,3,true);smudge.rect(left,top+bh,bw,3,true)
   for i=1,M.cols*M.rows do
    local x=left+((i-1)%M.cols)*cell;local y=top+((i-1)//M.cols)*cell
    local v=g.b[i];local revealed=v&2~=0
    if not revealed then smudge.rect_dither(x,y,cell,cell,false) end
    smudge.rect(x,y,cell,cell,false)
    if v&4~=0 then G.flag(x,y,cell)
    elseif revealed and v&1~=0 then smudge.rect(x,y,cell,cell,true);G.mine(x,y,cell,true)
    elseif g.status>=2 and v&1~=0 then G.mine(x,y,cell,false)
    elseif revealed then
     local n=M.adjacent(g,i,1)
     if n>0 then G.center(tostring(n),x,y+(cell>=50 and 2 or 0),cell,cell,cell>=50 and 30 or 20) end
    end
    if keyboard and i==cursor then smudge.rect(x+2,y+2,cell-4,cell-4,false,2) end
   end
   local y=h-68
   if g.status>=2 then
    smudge.rect(16,y,w-32,52,true)
    G.center(g.status==2 and 'CLEARED' or 'BOOM',16,y,w-32,52,20,true)
   else
    local remaining=10;for i=1,80 do if g.b[i]&4~=0 then remaining=remaining-1 end end
    local mw=(w-32)//3;local cw=w-32-mw-52-12
    G.center(tostring(remaining)..' OF 10',16,y+6,cw,52,30)
    G.flag(16+cw,y,52)
    smudge.rect(w-16-mw,y,mw,52,flag)
    G.center(flag and 'FLAG' or 'DIG',w-16-mw,y,mw,52,20,flag)
   end
  end
  if notice~='' then G.center('SAVE FAILED - RETRY EXIT',0,h-108,w,30,20) end
 end
 function on_button(btn,pressed)
  if not pressed then return end
  keyboard=true
  if btn=='page_back' then btn='up' elseif btn=='page_forward' then btn='down' end
  if btn=='back' then
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
    if menu==2 then g=M.new(smudge.millis());mode=1;cursor=1;flag=false;screen='board';notice=''
    else screen='menu' end
    menu=1
   end
  elseif screen=='help' then
   if btn=='confirm' or btn=='right' or btn=='down' then
    if lesson==5 then screen='menu';lesson=1 else lesson=lesson+1 end
   elseif btn=='left' or btn=='up' then lesson=math.max(1,lesson-1) end
  elseif screen=='board' then
   if btn=='left' then cursor=(cursor-2)%(M.cols*M.rows)+1
   elseif btn=='right' then cursor=cursor%(M.cols*M.rows)+1
   elseif btn=='up' then cursor=(cursor-1-M.cols)%(M.cols*M.rows)+1
   elseif btn=='down' then cursor=(cursor-1+M.cols)%(M.cols*M.rows)+1
   elseif btn=='confirm' then act()
    end
  end
  refresh()
 end
 function on_touch(x,y,event)
  if event~='tap' then return end
  geometry()
  if x<0 or y<0 or x>=w or y>=h then return end
  keyboard=false
  if y<76 then on_button('back',true);return end
  if screen=='menu' then
   local i=math.floor((y-top)/56)+1
   if x>=20 and x<w-20 and i>=1 and i<=#items then menu=i;select_menu() end
  elseif screen=='confirm' then
   if y>=top+48 and y<top+144 then menu=y<top+96 and 1 or 2;on_button('confirm',true) end
  elseif screen=='help' then
   if x>=16 and x<w-16 and y>=h-68 and y<h-16 then on_button('confirm',true) end
  elseif screen=='board' then
   if y>=h-68 and y<h-16 then
    if g.status>=2 then screen='menu';menu=1;refresh();return end
    if x>=w-16-(w-32)//3 and x<w-16 then flag=not flag;refresh();return end
   end
   if x>=left and x<left+bw and y>=top and y<top+bh then
    cursor=math.floor((y-top)/cell)*M.cols+math.floor((x-left)/cell)+1;act()
   end
  end

 end
 -- Callback presence is the firmware opt-in: old hosts keep the mode-button fallback.
 function on_long_press(x,y)
  geometry()
  if screen~='board' or x<left or x>=left+bw or y<top or y>=top+bh then return end
  keyboard=false
  local col=(x-left)//cell+1;local row=(y-top)//cell+1
  if M.action(g,col,row,true) then notice='';refresh() end
 end
 function on_update()end
 function on_swipe(direction)on_button(direction,true)end
 function on_exit()save() end
end
return U
