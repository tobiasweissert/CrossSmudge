-- The graphical board adapts to existing validated actions and journal semantics.
local M,S,V,R=dofile('core.lua'),dofile('storage.lua'),dofile('board.lua'),dofile('records.lua')
local g,cursor,notice,savedAt=nil,1,'',0
local closed=false
local function save()
 R.observe(M,g);local ok=S.save(M.encode(g));local recordOk=R.save();notice=not ok and'SAVE FAILED'or not recordOk and'RECORD SAVE FAILED'or'';return ok
end
local function act(i)
 local result
 if type(i)=='table'then if not M.apply(g,{5,math.min(i[1],i[2]),math.max(i[1],i[2])})then return end
 else result=M.act(g,i)end;cursor=1
 R.observe(M,g);if not R.save()then notice='RECORD SAVE FAILED'end
 if result=='exit' then M.pause(g);if save()then closed=true;smudge.exit()end
 elseif smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end
 smudge.request_update()
end
function on_init()closed=false;g=S.load(M.decode);local saved=g~=nil;g=g or M.new(smudge.millis());M.pause(g);R.load();R.observe(M,g);R.save();V.init(saved,R);smudge.request_update()end
function on_draw()if not closed then V.draw(M,g,cursor,notice)end end
function on_button(b,p)
 if not p or closed then return end
 if b=='back' then M.pause(g);if save()then closed=true;smudge.exit()end
 elseif b=='confirm'then local index=V.focused();if index then act(index)end
 else V.button(b)end
 smudge.request_update()
end
function on_touch(x,y,event)
 if event~='tap'or closed then return end
 local index=V.touch(x,y);if index then act(index)else smudge.request_update()end
end
function on_swipe(dir)if closed then return end;V.pageBy((dir=='up'or dir=='left')and 1 or -1);smudge.request_update()end
function on_update()if not closed and g and V.playing()and g.ui==0 and g.turn==2 and g.phase<2 then act(1)end end
function on_exit()M.pause(g);save();closed=true end
return {state=function()return g end,view=V}
