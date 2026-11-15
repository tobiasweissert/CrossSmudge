local U,M,S=dofile('ui.lua'),dofile('core.lua'),dofile('storage.lua')
local state,settings,lastMinute
local function save()M.tick(state,smudge.millis());return U.save(S,M.encode(state))end
local function awake()smudge.prevent_sleep(state.running)end
local function pause()
 if not M.tick(state,smudge.millis())then state.running=not state.running end;state.tick=smudge.millis();awake();save();U.redraw()
end
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
  U.button('Return',p,t+240,w-2*p,44,function()settings=false end)
 else
  U.button(state.phase==1 and 'Focus' or state.phase==2 and 'Break' or 'Long break',p,t,math.min(145,w//3),40,function()settings=true;U.focus=1 end,true)
  U.text(w-p,t+12,U.fit(state.label,w*0.57,'small'),'small',false,'right')
  local minutes=math.ceil(state.remaining/60)
  local numberH=math.min(180,(b-t)*0.36);local numberW=numberH*1.25
  U.number(t+65,tostring(minutes),numberH,w/2-numberW/2)
  U.text(w/2,t+75+numberH,'min left','ui12',true,'center')
  local barY=t+110+numberH;local total=M.duration(state);local bw=(w-2*p)/25
  for i=1,25 do smudge.rounded_rect(math.floor(p+(i-1)*bw),barY,math.max(2,math.floor(bw)-3),20,2,i<=math.floor((total-state.remaining)*25/total),1)end
  U.text(p,barY+30,state.running and 'Timer running' or 'Paused - Select to start','small')
  local y=barY+65;local bw=(w-2*p-16)//3
  U.button(state.running and 'Pause' or 'Start',p,y,bw,50,pause,true)
  U.button('Skip',p+bw+8,y,bw,50,skip)
  U.button('Reset',p+2*(bw+8),y,bw,50,reset)
  if y+130<b then
   for i=1,4 do smudge.circle(p+12+(i-1)*36,y+100,12,i<=((state.session-1)%4),2)end
   U.text(p+160,y+90,state.done..' done today','small')
   U.text(p,y+130,'Long break after session 4','small')
  end
 end
 U.footer()
end
function on_button(key,pressed)
 if not pressed then return end
 if U.editor then if key=='back'then U.closeEditor();U.redraw()else U.key(key)end;return end
 if key=='back'then if settings then settings=false else if save()then smudge.exit()end end
 elseif key=='left' and not settings then settings=true;U.focus=1

 else U.key(key)end;U.redraw()
end
function on_tap(x,y)U.tap(x,y)end
function on_update()
 local day=U.day(smudge.get_date());if day~=state.day then state.day=day;state.done=0;save();U.redraw()end
 if M.tick(state,smudge.millis())then awake();save();smudge.full_refresh();U.redraw()end
 local minute=math.ceil(state.remaining/60)
 if minute~=lastMinute then lastMinute=minute;U.redraw()end
end
function on_exit()save();smudge.prevent_sleep(false)end
