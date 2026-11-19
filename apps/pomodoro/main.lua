local U,M,S=dofile('ui.lua'),dofile('core.lua'),dofile('storage.lua')
local state,settings,lastMinute
local function save()M.tick(state,smudge.millis());return U.save(S,M.encode(state))end
local function awake()smudge.prevent_sleep(state.running)end
local function pause()
 if not M.tick(state,smudge.millis())then state.running=not state.running end;state.tick=smudge.millis();awake();save();U.redraw()
end
local function closeSettings()settings=false;U.focus=2;U.redraw()end
local function skip()M.next(state,false);state.carry=0;awake();save();U.redraw()end
local function reset()state.phase=1;state.session=1;state.remaining=state.focus*60;state.running=false;state.carry=0;awake();save();U.redraw()end
function on_init()
 state=S.load(M.decode)or{focus=25,short=5,long=15,phase=1,session=1,remaining=1500,done=0,day=U.day(smudge.get_date()),label='Write project notes',running=false}
 state.tick=smudge.millis();state.carry=0;lastMinute=-1;U.focus=2;awake();U.redraw()
end
function on_draw()
 if U.editor then U.editDraw();return end
 U.begin(settings and 'Timer settings' or 'Pomodoro','Session '..state.session..' of 4')
 local p,w,t,b=U.pad,U.w,U.top,U.bottom
 if settings then
  local fields={{'Focus', 'focus',90},{'Short break','short',30},{'Long break','long',60}}
  for i,f in ipairs(fields)do
   local y=t+(i-1)*58;U.text(p,y+12,f[1]..': '..state[f[2]]..' min','ui12',true)
   U.button('-',w-p-100,y,44,44,function()state[f[2]]=math.max(1,state[f[2]]-1);reset()end)
   U.button('+',w-p-48,y,44,44,function()state[f[2]]=math.min(f[3],state[f[2]]+1);reset()end)
  end
  U.button('Task: '..U.fit(state.label,w-2*p-80),p,t+186,w-2*p,44,function()U.edit('Focus task',state.label,48,function(s)if not s:find('[%z\1-\31]')then state.label=s;save()end end)end)
  U.button('Return',p,t+240,w-2*p,44,closeSettings)
 else
  U.button(state.phase==1 and 'Focus' or state.phase==2 and 'Break' or 'Long break',p,t,math.min(145,w//3),40,function()settings=true;U.focus=1 end,true)
  U.text(w-p,t+12,U.fit(state.label,w*0.57,'small'),'small',false,'right')
  local landscape=w>U.h
  local timerText=tostring(math.ceil(state.remaining/60))
  local nh=landscape and math.min(150,b-t-120)or math.min(180,(b-t)//3)
  local nx=landscape and w//4 or w//2
  local ny=landscape and t+65 or t+(b-t-nh)//2-28
  U.number(ny,timerText,nh,nx-nh*1.25/2)
  U.text(nx,ny+nh+12,'MIN LEFT','ui10',true,'center')
  local sx=landscape and w//2+24 or p
  local sw=w-p-sx;local barY=b-112;local total=M.duration(state);local bw=sw//25
  for i=1,25 do U.style.panel({x=sx+(i-1)*bw,y=barY,w=math.max(2,bw-3),h=12},i<=math.floor((total-state.remaining)*25/total))end
  U.text(sx,barY+20,state.running and 'Timer running' or 'Paused - Select to start','small')
  for i=1,4 do U.style.panel({x=sx+(i-1)*26,y=barY+52,w=18,h=18},i<state.session)end
  U.text(sx+120,barY+48,state.done..' done today','small')
  U.text(sx,barY+84,'Long break after session 4','small')
  U.actions({{state.running and 'Pause' or 'Start',pause},{'Skip',skip},{'Reset',reset}},1)
 end
 lastMinute=math.ceil(state.remaining/60)
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if key=='back'then U.closeEditor();U.redraw()else U.key(key)end;return end
 if key=='back'then if settings then closeSettings() else if save()then smudge.exit()end end
 elseif key=='left' and not settings then settings=true;U.focus=1

 else U.key(key)end;U.redraw()
end
function on_tap(x,y)U.tap(x,y)end
function on_update()
 local day=U.day(smudge.get_date());if day~=state.day then state.day=day;state.done=0;save();U.redraw()end
 if M.tick(state,smudge.millis())then awake();save();smudge.full_refresh();U.redraw()end
 if math.ceil(state.remaining/60)~=lastMinute then lastMinute=math.ceil(state.remaining/60);if not settings and not U.editor then U.redraw()end end
end
function on_exit()save();smudge.prevent_sleep(false)end
