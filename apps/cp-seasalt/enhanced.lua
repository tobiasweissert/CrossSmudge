local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_enhanced.lua')end
-- The graphical board adapts to existing validated actions and journal semantics.
local previousOrientation
local M,S,V,R=dofile('core.lua'),dofile('storage.lua'),dofile('board.lua'),dofile('records.lua')
local g,cursor,notice,savedAt=nil,1,'',0
local closed=false
local function save()
 R.observe(M,g);local ok=S.save(M.encode(g));local recordOk=R.save();notice=not ok and'SAVE FAILED'or not recordOk and'RECORD SAVE FAILED'or'';return ok
end
local function act(i)
 if g.turn==1 then V.report=''end;local seed,round=g.seed,g.round
 local result
 if type(i)=='table'then if not M.apply(g,{5,math.min(i[1],i[2]),math.max(i[1],i[2])})then return end
 else result=M.act(g,i)end;if g.seed~=seed or g.round~=round then V.report=''end;cursor=1
 R.observe(M,g);if not R.save()then notice='RECORD SAVE FAILED'end
 if result=='exit' then M.pause(g);if save()then closed=true;smudge.exit()end
 elseif smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end
 smudge.request_update()
end
function on_init()local orientOk,value=pcall(smudge.get_orientation);if orientOk then previousOrientation=value;smudge.set_orientation('portrait')end;closed=false;g=S.load(M.decode);local saved=g~=nil;g=g or M.new(smudge.millis());M.pause(g);R.load();R.observe(M,g);R.save();V.init(saved,R);smudge.request_update()end
function on_draw()if not closed then V.draw(M,g,cursor,notice)end end
function on_button(b,p)
 if not p or closed then return end
 if b=='back'then
  if V.help>0 then V.help=0
  elseif V.call then V.call=false
  elseif V.unavailable then V.unavailable=false;V.menu=true
  elseif not V.menu then M.pause(g);V.selected=nil;V.second=nil;V.menu=true;save()
  else M.pause(g);if save()then closed=true;if previousOrientation then smudge.set_orientation(previousOrientation);previousOrientation=nil end;smudge.exit()end end
 elseif b=='confirm'then local index=V.focused();if index then act(index)end
 else V.button(b)end
 smudge.request_update()
end
function on_touch(x,y,event)
 if event~='tap'or closed then return end
 local index=V.touch(x,y);if index then act(index)else smudge.request_update()end
end
function on_swipe(dir)if closed then return end;local delta=(dir=='up'or dir=='left')and 1 or -1;if V.help>0 then local page=V.help+delta;V.help=page>6 and 0 or math.max(1,page)else V.pageBy(delta)end;smudge.request_update()end
function on_update()
 if closed or not g or not V.playing()or g.ui~=0 or g.turn~=2 or g.phase>=2 then return end
 -- Same old AI/actions, shown as one opponent turn as in the original activity.
 local kinds=dofile('cards.lua')[1];local laid={0,0,0};local stole=false;local called=false;local took=false
 for guard=1,200 do
  if g.turn~=2 or g.phase>=2 then break end
  local move=M.ai(g);if not move then break end
  local held=0;for _,p in ipairs(g.place)do if p==1 then held=held+1 end end
  local before=M.encode(g);act(1);if M.encode(g)==before then break end
  if move[1]==5 then local kind=kinds[move[2]];if kind<=2 then laid[kind+1]=laid[kind+1]+1 end;local after=0;for _,p in ipairs(g.place)do if p==1 then after=after+1 end end;stole=stole or after<held
  elseif move[1]==9 then called=true
  elseif move[1]==1 or move[1]==2 or move[1]==3 or move[1]==7 then took=true end
 end
 if called then V.report=g.bet==1 and'THEY BET LAST CHANCE. ONE MORE TURN, MAKE IT COUNT.'or'THEY CALLED STOP.'
 elseif stole then V.report='SWIMMER AND SHARK. THEY STOLE FROM YOUR HAND.'
 elseif laid[2]>0 then V.report='THEY PLAYED BOATS FOR EXTRA TURNS. YOUR MOVE NOW.'
 elseif laid[1]>0 then V.report='THEY PLAYED TWO CRABS AND DUG THROUGH A PILE.'
 elseif laid[3]>0 then V.report='THEY PLAYED TWO FISH AND DREW A CARD.'
 elseif took then V.report='THEY TOOK A CARD. YOUR MOVE.'else V.report=''end
 smudge.request_update()
end
function on_exit()if previousOrientation then smudge.set_orientation(previousOrientation);previousOrientation=nil end;M.pause(g);save();closed=true end
return {state=function()return g end,view=V}
