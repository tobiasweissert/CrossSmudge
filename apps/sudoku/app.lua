local C=dofile('cp_core.lua')
local J=dofile('cp_journal.lua')
local V=dofile('board.lua')
local G={cell=1,digit=1,notesMode=false,level=0,menu=true,menuIndex=1,notice='',history={},closed=false,elapsed=0,hinted=false}
local L={};local journal,dirty,lastTime,lastSave
local levels={'Easy','Medium','Hard','Expert'}
local function digits(a) local t={};for i=1,81 do t[i]=tostring(a[i]) end;return table.concat(t) end
local function array(s) if #s~=81 or s:find('[^0-9]') then return nil end;local a={};for i=1,81 do a[i]=tonumber(s:sub(i,i)) end;return a end
local function decode(s)
 local lev,elapsed,hinted,given,solution,value,notes=s:match('^([0-3]) (%d+) ([01])\n(%d+)\n(%d+)\n(%d+)\n([%d,]+)$')
 elapsed=tonumber(elapsed);if not lev or not elapsed or elapsed>31536000 then return nil end
 local a,b,c=array(given),array(solution),array(value);if not a or not b or not c then return nil end
 local r=C.solve(a,10);if not r.solved or r.broken or C.levelOf(r.hardest)~=tonumber(lev) then return nil end
 for i=1,81 do if b[i]==0 or b[i]~=r.grid[i] or a[i]~=0 and c[i]~=a[i] then return nil end end
 local n={};for v in notes:gmatch('[^,]+') do local x=tonumber(v);if #n>=81 or not x or x>511 or math.type(x)~='integer' then return nil end;n[#n+1]=x end
 if #n~=81 or table.concat(n,',')~=notes then return nil end
 return {level=tonumber(lev),elapsed=elapsed,hinted=hinted=='1',given=a,solution=b,value=c,notes=n}
end
local function encode() return G.level..' '..G.elapsed..' '..(G.hinted and '1' or '0')..'\n'..digits(G.given)..'\n'..digits(G.solution)..'\n'..digits(G.value)..'\n'..table.concat(G.notes,',') end
local function refresh() smudge.request_update() end
local function save() if dirty and G.given then if journal:write(encode()) then dirty=false;lastSave=smudge.millis() else G.notice='Save failed; earlier copy retained' end end end
local function solved() if not G.value then return false end;for i=1,81 do if G.value[i]~=G.solution[i] then return false end end;return true end
local function remember(i) if #G.history==24 then table.remove(G.history,1) end;G.history[#G.history+1]={i,G.value[i],G.notes[i]} end
local function enter(i,note)
 if not G.given or G.job or G.solved then return end
 if G.given[i]~=0 then G.digit=G.given[i];refresh();return end
 remember(i)
 if note then G.notes[i]=G.notes[i]~(1<<(G.digit-1));G.value[i]=0
 else G.value[i]=G.value[i]==G.digit and 0 or G.digit;G.notes[i]=0 end
 G.solved=solved();dirty=true;G.notice=G.solved and 'Solved!' or '';refresh()
end
local function hint()
 if not G.given or G.job or G.solved then return end
 for i=1,81 do if G.value[i]~=0 and G.value[i]~=G.solution[i] then G.cell=i;G.notice='Check this entry first';refresh();return end end
 local h=C.next_hint(G.value,10)
 if h.found then remember(h.cell);G.cell=h.cell;G.value[h.cell]=h.digit;G.notes[h.cell]=0;G.hinted=true;G.notice=C.names[h.technique];G.solved=solved();dirty=true else G.notice='No logical placement found' end
 refresh()
end
local function start(level)
 lastTime=smudge.millis();G.pendingLevel=level;G.job=C.newGenerator(level,smudge.millis());G.attempts=1;G.menu=false;G.notice='Generating '..levels[level+1]..' (Back cancels)';refresh()
end
local function geometry()
 local w,h=smudge.get_bounds();L=V.layout(w,h);return L
end
local function action(n)
 if n==1 then G.notesMode=not G.notesMode elseif n==2 then hint()
 elseif n==3 then local u=table.remove(G.history);if u then G.value[u[1]]=u[2];G.notes[u[1]]=u[3];G.solved=solved();dirty=true end
 else G.menu=true;G.menuIndex=1 end;refresh()
end
function on_init()
 local payload;journal,payload=J.open('sudoku',1024,function(s) return decode(s)~=nil end)
 if payload then for k,v in pairs(decode(payload)) do G[k]=v end;G.menu=false;G.solved=solved() end
 local old,msg=journal:preserve('state',4096);if old and not payload then G.notice=msg end
 lastTime=smudge.millis();lastSave=lastTime;V.init(G);geometry()
end
function on_draw()
 if not G.closed then V.draw(geometry(),G) end
end
local function menuSelect()
 lastTime=smudge.millis()
 if G.menuIndex==1 then if G.given then G.menu=false;lastTime=smudge.millis() end
 elseif G.menuIndex==6 then save();G.closed=true;smudge.exit()
 else start(G.menuIndex-2) end;refresh()
end
function on_button(b,pressed)
 if not pressed or G.closed then return end
 if G.job then if b=='back' then G.job=nil;G.notice='Generation canceled';G.menu=not G.given end;refresh();return end
 if V.button(G,b,menuSelect,refresh) then return end
 if G.menu then
  if b=='confirm' then menuSelect() elseif b=='back' then if G.given then G.menu=false else G.closed=true;smudge.exit() end
  elseif b=='left' or b=='up' or b=='page_back' then G.menuIndex=(G.menuIndex-2)%6+1
  elseif b=='right' or b=='down' or b=='page_forward' then G.menuIndex=G.menuIndex%6+1 end;refresh();return
 end
 if b=='back' then G.menu=true;G.menuIndex=1
 elseif b=='confirm' then enter(G.cell,G.notesMode)
 elseif b=='left' then G.cell=(G.cell-10)%81+1 elseif b=='right' then G.cell=(G.cell+8)%81+1
 elseif b=='up' or b=='page_back' then G.cell=(G.cell-2)%81+1 elseif b=='down' or b=='page_forward' then G.cell=G.cell%81+1 end
 refresh()
end
function on_touch(x,y,event)
 if G.closed then return end
 if G.job then if event=='tap' then on_button('back',true) end;return end
 V.touch(L,G,x,y,event,enter,action,menuSelect,refresh)
end
function on_long_press(x,y)on_touch(x,y,'hold')end
function on_update()
 if G.closed then return end
 if G.job then
  lastTime=smudge.millis()
  if C.stepGenerator(G.job) then local j=G.job
   if j.accepted then G.level=G.pendingLevel;G.given=j.given;G.solution=j.v;G.value={};G.notes={};for i=1,81 do G.value[i]=G.given[i];G.notes[i]=0 end
    G.history={};G.elapsed=0;G.hinted=false;G.solved=false;G.cell=1;G.job=nil;G.notice='';dirty=true
   elseif G.attempts<200 then G.attempts=G.attempts+1;G.job=C.newGenerator(G.pendingLevel,j.rng)
   else G.job=nil;G.notice='No matching puzzle in 200 attempts; try again';G.menu=true end
   refresh()
  end
 else
  local now=smudge.millis();local dt=now-lastTime
  if dt>=1000 and dt<60000 and G.given and not G.solved and not G.menu then G.elapsed=math.min(31536000,G.elapsed+dt//1000);if now-lastSave>=30000 then dirty=true end end
  if dt>=1000 or dt<0 then lastTime=now end
  save()
 end
end
function on_exit()
 local dt=smudge.millis()-lastTime
 if not G.job and not G.menu and G.given and not G.solved and dt>=1000 and dt<60000 then G.elapsed=math.min(31536000,G.elapsed+dt//1000);dirty=true end
 G.job=nil;if G.given then dirty=true end;save();G.closed=true
end
return {core=C,state=function() return G end,layout=function() return L end}
