-- Original game/save controller with a phase-specific CrossPlay presentation.
local M,S,V=dofile('core.lua'),dofile('storage.lua'),dofile('presentation.lua')
local g,last,lastSave,closed,notice=nil,0,0,false,''
local function refresh()smudge.request_update()end
local function save()local ok=S.save(M.encode(g));if ok and V.persist and not V.persist(g)then notice='CATEGORY RECORD SAVE FAILED';return false end;notice=ok and ''or'SAVE FAILED';return ok end
local function tick()
 local before=g.phase;local now=smudge.millis();local delta=now-last;last=now
 if delta>=0 and delta<3600000 and M.tick then M.tick(g,delta) end
 if M.awake then smudge.prevent_sleep(M.awake(g)) end
 return before~=g.phase
end
local function activate(hit)
 if not hit then return end
 -- A failed reset-sidecar clear must become durable before new rounds can
 -- catch up to its old counter and make stale category bits look current.
 if V.mustPersistBeforeAction and V.mustPersistBeforeAction(hit.action)and not save()then refresh();return end
 local result=V.act(g,M,hit.action,hit.value)
 if type(result)=='table' then g=result;last=smudge.millis()
 elseif result=='exit' then M.pause(g);if save() then closed=true;smudge.exit() end;return end
 V.focus=1
 if smudge.millis()-lastSave>=5000 then save();lastSave=smudge.millis() end
 refresh()
end
function on_init()
 g=S.load(M.decode) or M.new(smudge.millis());M.pause(g);last=smudge.millis();closed=false;V.init(g);refresh()
end
function on_draw()if not closed then V.draw(g,M,notice) end end
function on_button(button,pressed)
 if not pressed or closed then return end
 if button~='confirm' and button~='back' then V.keyboard=true end
 if tick() then refresh();return end
 if button=='back' then
  if V.back(g) then refresh();return end
  M.pause(g);if save() then closed=true;V.exit();smudge.exit() end
 else activate(V.key(g,button));refresh() end
end
function on_touch(x,y,event)
 if closed or event~='tap' then return end
 V.keyboard=false
 if tick() then refresh();return end
 activate(V.at(x,y))
end
function on_swipe(direction)
 if closed then return end
 on_button((direction=='up' or direction=='left') and 'down' or 'up',true)
end
function on_update()
 if closed then return end
 local before=V.clock(g);local phase=g.phase;tick()
 if V.update then V.update(g) end
 if phase~=g.phase then save();refresh() elseif before~=V.clock(g) then refresh() end
end
function on_exit()
 if g then tick();M.pause(g);if M.awake then smudge.prevent_sleep(false) end;save() end
 V.exit();closed=true
end
return {state=function()return g end,view=V,core=M}
