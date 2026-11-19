local identityOk,identity=pcall(smudge.get_device)
if not identityOk or identity~='sticky'then return dofile('legacy_enhanced.lua')end
-- Original card table over the existing validated game core.
local M,S,V,R=dofile('core.lua'),dofile('storage.lua'),dofile('board.lua'),dofile('records.lua')
local g,closed,savedAt,notice,orientation=nil,false,0,'',nil
local trickShownAt
local function restore()if orientation then smudge.set_orientation(orientation);orientation=nil end end
local function save()
 R.observe(M,g);local ok=S.save(M.encode(g));local r=R.save();notice=not ok and'SAVE FAILED'or not r and'RECORD SAVE FAILED'or'';return ok
end
local function act(action)
 local result
 if action=='new'then g=M.new(smudge.millis());V.menu=false
 elseif action=='exit'then result='exit'
 elseif type(action)=='number'then result=M.act(g,action)end
 R.observe(M,g);if not R.save()then notice='RECORD SAVE FAILED'end
 if result=='exit'then if save()then closed=true;restore();smudge.exit()end
 elseif smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end
 smudge.request_update()
end
function on_init()
 closed=false;local ok,value=pcall(smudge.get_orientation);if ok and type(value)=='string'then orientation=value end
 g=S.load(M.decode);local saved=g~=nil;g=g or M.new(smudge.millis());R.load();R.observe(M,g);R.save();V.init(saved,R);smudge.request_update()
end
function on_draw()if not closed then V.draw(M,g,notice)end end
function on_touch(x,y,e)if closed or e~='tap'then return end;local a=V.touch(x,y);if a then act(a)else smudge.request_update()end end
function on_button(b,p)
 if closed or not p then return end
 if b=='back'then act('exit')elseif b=='confirm'then local a=V.activate();if a then act(a)end else V.button(b)end;smudge.request_update()
end
function on_update()
 if closed or not V.playing()then trickShownAt=nil;return end
 if g.phase==2 then trickShownAt=trickShownAt or smudge.millis();if smudge.millis()-trickShownAt>=900 then trickShownAt=nil;act(1)end
 else trickShownAt=nil;if g.phase==1 and g.turn~=1 then act(1)end end
end
function on_exit()if g then save()end;closed=true;restore()end
return{state=function()return g end,view=V}
