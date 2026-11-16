local identityOk,identity=pcall(smudge.get_device)
if not identityOk or identity~='sticky'then return dofile('legacy_enhanced.lua')end
-- Original market canvas; the existing core validates every committed move.
local M,S,V,R=dofile('core.lua'),dofile('storage.lua'),dofile('board.lua'),dofile('records.lua')
local g,closed,savedAt,notice,orientation=nil,false,0,'',nil
local function restore()if orientation then smudge.set_orientation(orientation);orientation=nil end end
local function save()
 R.observe(M,g);local ok=S.save(M.encode(g));local r=R.save();notice=not ok and'SAVE FAILED'or not r and'RECORD SAVE FAILED'or'';return ok
end
local function act(a)
 local result
 if a=='new'then g=M.new(smudge.millis());V.menu=false;V.report=nil;V.clear()
 elseif a=='exit'then result='exit'
 elseif type(a)=='table'then local s=g.turn;local before={seat=s,card=a[1]==1 and g.market[a[2]],herd=g.herd[s],points=g.points[s],bonus=g.nb[s]};if not M.apply(g,a)then return end;V.describe(a,before,g);V.clear()
 elseif type(a)=='number'then result=M.act(g,a);V.report=nil;V.clear()end
 R.observe(M,g);if not R.save()then notice='RECORD SAVE FAILED'end
 if result=='exit'then if save()then closed=true;restore();smudge.exit()end
 elseif smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end
 smudge.request_update()
end
function on_init()
 closed=false;local ok,value=pcall(smudge.get_orientation);if ok and type(value)=='string'then orientation=value;smudge.set_orientation('portrait')end
 g=S.load(M.decode);local saved=g~=nil;g=g or M.new(smudge.millis());M.pause(g);R.load();R.observe(M,g);R.save();V.init(saved,R);smudge.request_update()
end
function on_draw()if not closed then V.draw(M,g,notice)end end
function on_touch(x,y,e)if closed or e~='tap'then return end;local a=V.touch(x,y);if a then act(a)else smudge.request_update()end end
function on_button(b,p)
 if closed or not p then return end
 if b=='back'then act('exit')elseif b=='confirm'then local a=V.activate();if a then act(a)end else V.button(b)end;smudge.request_update()
end
function on_update()if not closed and V.playing()and g.phase==0 and g.turn==2 then act(M.ai(g))end end
function on_exit()if g then M.pause(g);save()end;closed=true;restore()end
return{state=function()return g end,view=V}
