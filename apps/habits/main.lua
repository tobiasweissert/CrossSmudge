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
 U.begin(menu and 'Habit options' or 'Habits','')
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
  U.button('+',w-p-44,t,44,32,add)
  U.text(p,t+5,'Last 16 weeks','ui12',true)
  local cell=math.max(5,math.min((w-2*p-36-45)//16,(b-t-170)//14))
  local gx=p+36;local gy=t+46
  local d=smudge.get_date();local monday=today-((d.wday+5)%7);local start=monday-15*7
  for row,label in pairs({[0]='M',[2]='W',[4]='F',[6]='S'}) do U.text(p,gy+row*(cell+3),label,'small')end
  for col=0,15 do for row=0,6 do
   local day=start+col*7+row;local mask=state.history[day] or 0;local n=0
   for i=1,#state.names do if (mask&(1<<(i-1)))~=0 then n=n+1 end end
   local x,y=gx+col*(cell+3),gy+row*(cell+3)
   smudge.rounded_rect(x,y,cell,cell,2,false,day==today and 2 or 1)
   if n>0 then if n==#state.names then smudge.rect(x+1,y+1,cell-2,cell-2,true)else smudge.rect_dither(x+1,y+1,cell-2,cell-2)end end
  end end
  local ly=gy+7*(cell+3)+15;local done=0
  for i=1,#state.names do if M.checked(state,today,i)then done=done+1 end end
  U.text(p,ly,'Today '..d.day..'/'..d.month..'   '..done..' of '..#state.names,'ui12',true)
  ly=ly+34;local rows=math.max(1,(b-ly-30)//52);local first=((selected-1)//rows)*rows+1
  for i=first,math.min(#state.names,first+rows-1)do
   local label=(i==selected and '> 'or '')..(M.checked(state,today,i) and '[x] ' or '[ ] ')..state.names[i]
   U.button('',p,ly+(i-first)*52,w-2*p,46,function()selected=i;M.toggle(state,today,i);save()end)
   U.text(p+12,ly+(i-first)*52+12,U.fit(label,w-2*p-110),'ui12')
   U.text(w-p-8,ly+(i-first)*52+14,M.streak(state,today,i)..' days','small',false,'right')
  end
  if #state.names==0 then U.text(p,ly,'Add your first habit','ui12')end
 end
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if confirmDelete then if key=='back'then confirmDelete=false else U.key(key)end;U.redraw();return end
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
