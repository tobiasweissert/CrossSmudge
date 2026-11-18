local U,M,S=dofile('ui.lua'),dofile('core.lua'),dofile('storage.lua')
local state,today,selected,menu= nil,nil,1,false
local confirmDelete=false
local function save()return U.save(S,M.encode(state,today))end
local function add()
 if #state.names>=8 then U.notice='Limit: eight habits';return end
 U.edit('New habit','',40,function(name)
  if name:find('%S') and not name:find('[%z\1-\31]') then state.names[#state.names+1]=name;selected=#state.names;save()end
 end)
end
function on_init()
 today=U.day(smudge.get_date())
 state=S.load(M.decode) or {names={'Exercise','Read 20 min','Take vitamins','Water plants'},history={}}
 U.redraw()
end
function on_draw()
 if U.editor then U.editDraw();return end
 local done=0;for i=1,#state.names do if M.checked(state,today,i)then done=done+1 end end
 U.begin(menu and 'Habit options' or 'Habits',done..' of '..#state.names)
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 if confirmDelete then
  U.button('Cancel',p,t,w-2*p,44,function()confirmDelete=false end)
  U.button('Delete habit',p,t+56,w-2*p,44,function()if state.names[selected]then M.remove(state,selected);selected=math.max(1,math.min(selected,#state.names));save()end;confirmDelete=false;menu=false end)
 elseif menu then
  local actions={{'Add habit',add},{'Rename selected',function()if state.names[selected] then U.edit('Habit name',state.names[selected],40,function(s)if s:find('%S') and not s:find('[%z\1-\31]') then state.names[selected]=s;save()end end)end end},
   {'Remove selected',function()confirmDelete=true;U.focus=1 end},
   {'Retry save',save},{'Return',function()menu=false end}}
  for i,a in ipairs(actions)do U.button(a[1],p,t+(i-1)*50,w-2*p,42,a[2])end
 else
  local landscape=w>U.h;local historyW=landscape and (w-3*p)//2 or w-2*p
  U.text(p,t,'LAST 16 WEEKS','ui10',true)
  local step=historyW//16;local cell=math.min(18,step-6);local gy=t+34
  local d=smudge.get_date();local monday=today-((d.wday+5)%7);local start=monday-15*7
  for col=0,15 do for row=0,6 do
   local day=start+col*7+row;local mask=state.history[day]or 0;local n=0
   for i=1,#state.names do if (mask&(1<<(i-1)))~=0 then n=n+1 end end
   local x,y=p+col*step,gy+row*24
   U.style.panel({x=x,y=y,w=cell,h=18},n>0 and n==#state.names)
   if n>0 and n<#state.names then smudge.rect_dither(x+2,y+2,cell-4,14)end
  end end
  local lx=landscape and p+historyW+p or p;local lw=landscape and w-p-lx or w-2*p
  local ly=landscape and t or gy+7*24+16
  U.text(lx,ly,'TODAY '..d.day..'/'..d.month,'ui10',true);ly=ly+36
  local rows=math.max(1,(b-ly)//60);local first=((selected-1)//rows)*rows+1
  for i=first,math.min(#state.names,first+rows-1)do
   local y=ly+(i-first)*60;local focus=i==selected
   U.button('',lx,y,lw,52,function()selected=i;M.toggle(state,today,i);save()end,focus)
   smudge.rect(lx+12,y+15,20,20,false,2,not focus)
   if M.checked(state,today,i)then smudge.rect(lx+16,y+19,12,12,true,not focus)end
   U.text(lx+42,y+13,U.style.fit(state.names[i],lw-145,'ui10',true),'ui10',true,'left',not focus)
   U.text(lx+lw-10,y+15,M.streak(state,today,i)..' days','small',false,'right',not focus)
  end
  if #state.names==0 then U.text(lx,ly,'Add your first habit','ui10')end
  U.actions({{'Option',function()menu=true;U.focus=1 end},{'+ Add',add},{'Done',function()if state.names[selected]then M.toggle(state,today,selected);save()end end}},3)
 end
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if confirmDelete then if key=='back'then confirmDelete=false else U.key(key)end;U.redraw();return end
 if U.editor then
 if key=='back'then U.closeEditor();U.redraw()else U.key(key)end;return end
 if key=='back'then if menu then menu=false else smudge.exit()end
 elseif key=='left'then menu=not menu;U.focus=1
 elseif not menu and (key=='up' or key=='page_back')then selected=(selected-2)%math.max(1,#state.names)+1
 elseif not menu and (key=='down' or key=='page_forward')then selected=selected%math.max(1,#state.names)+1
 elseif not menu and key=='confirm'then if state.names[selected]then M.toggle(state,today,selected);save()end
 elseif key=='right' and not menu then add()
 else U.key(key)end
 U.redraw()
end
function on_tap(x,y)U.tap(x,y)end
function on_swipe(dir)on_button(dir=='left' and 'down' or 'up',true)end
function on_update()
 local day=U.day(smudge.get_date());if day~=today then today=day;U.redraw()end
 -- Keep only the visible sixteen-week window, without daily SD writes.
 for day in pairs(state.history)do if day<today-111 then state.history[day]=nil end end
end
function on_exit()end
