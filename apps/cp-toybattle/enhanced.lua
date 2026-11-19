local ok,device=pcall(smudge.get_device)
if not ok or device~='sticky'then return dofile('legacy_enhanced.lua')end
-- Original UI shell and move questions; the existing core applies every real move.
local M,S,F,B=dofile('core.lua'),dofile('storage.lua'),dofile('graphics.lua'),dofile('board.lua')
local Front,Help,UI,D=dofile('front.lua'),dofile('lesson.lua'),dofile('ui.lua'),dofile('ui_draft.lua')
local g,L,mode,previousOrientation;local cursor,helpPage,mapPage=1,1,0;local boxes={};local draft;local options;local hasSave=false;local dealt=false;local loaded=false;local savedAt=0;local notice='';local closed=false
local function redraw()smudge.request_update()end
local function save()local saved=S.save(M.encode(g));notice=saved and''or'SAVE FAILED';if saved then hasSave=true end;return saved end
local function changed()if smudge.millis()-savedAt>=5000 then save();savedAt=smudge.millis()end;redraw()end
local function choices()local rows={};if(hasSave or dealt)and g.phase==0 then rows[#rows+1]='CONTINUE'end;rows[#rows+1]='PLAY';rows[#rows+1]='PLAY NEARBY';rows[#rows+1]='HOW TO PLAY';return rows end
local function start()
 if hasSave or dealt or options.map~=g.map then local index=g.phase==1 and 2 or g.turn==2 and 3 or 4;M.act(g,index);M.act(g,options.map==0 and 2 or options.map+1)end
 g.special=options.special and 1 or 0;dealt=true;M.pause(g);draft=D.clear();notice='';mode='board';changed()
end
local function afterAnswer()
 if D.pending(M,g,draft)=='ready'and #draft.steps>0 then
  if M.apply(g,draft.steps)then draft=D.clear();notice='';changed()else notice='THE RULES REFUSE THAT MOVE';redraw()end
 else notice='';redraw()end
end
local function action(op,value)
 if op=='home'then local row=choices()[value]
  if row=='CONTINUE'then mode='board';draft=D.clear()
  elseif row=='PLAY'then options={map=g.map,special=g.special==1};mode='setup';cursor=1
  elseif row=='PLAY NEARBY'then mode='unavailable';cursor=1
  else helpPage=1;mode='help'end
 elseif op=='maps'then mapPage=0;mode='maps';cursor=1
 elseif op=='map'then options.map=value;mode='setup';cursor=1
 elseif op=='bases'then options.special=not options.special
 elseif op=='start'then start();return
 elseif op=='local'then notice='LOCAL OPPONENT: ONE STRENGTH'
 elseif op=='first'then notice='LOCAL GAME: YOU MOVE FIRST'
 elseif op=='done'then mode='menu';cursor=1
 elseif op=='again'then options={map=g.map,special=g.special==1};mode='setup';cursor=1
 elseif op=='skip'or op=='take'then if D.offer(M,g,draft,op=='take')then afterAnswer();return end
 elseif op=='cancel'then draft=D.clear();notice=''
 elseif op=='draw'then if g.turn==1 and g.phase==0 and M.apply(g,{})then draft=D.clear();changed();return end
 elseif op=='result'then mode='result';cursor=1
 elseif op=='brief'then mode='brief'
 end;redraw()
end
function on_init()
 closed=false;F.init();local orientOk,value=pcall(smudge.get_orientation);if orientOk then previousOrientation=value;smudge.set_orientation('portrait')end
 g=S.load(M.decode);hasSave=g~=nil;loaded=hasSave;dealt=hasSave;g=g or M.new(smudge.millis());M.pause(g);draft=D.clear();mode='menu';cursor=1;notice='';redraw()
end
function on_draw()
 if closed then return end;smudge.clear();local w,h=smudge.get_bounds();boxes={}
 if mode=='menu'then local labels=choices();local menuBoxes=Front(M,g,F,w,h,labels,cursor,B.preview,g.phase==1 and'LOCAL GAME COMPLETE'or loaded and'LOCAL GAME'or'NO BATTLES YET');for i,b in ipairs(menuBoxes)do boxes[i]={box=b,op='home',value=i}end
 elseif mode=='setup'then boxes=UI.setup(M,g,F,B,w,h,options)
 elseif mode=='maps'then boxes=UI.maps(M,g,F,B,w,h,mapPage)
 elseif mode=='help'then local drawn=Help.draw(helpPage,F,w,h,B);for i,b in ipairs(drawn)do boxes[i]={box=b,op=i==1 and'prev'or'next'}end
 elseif mode=='brief'then boxes=UI.brief(M,g,F,w,h)
 elseif mode=='result'then boxes=UI.result(M,g,F,w,h)
 elseif mode=='unavailable'then boxes=UI.unavailable(F,w,h)
 else
  local ask=D.pending(M,g,draft);local shown=D.project(M,g,draft);local st=draft.steps[draft.step];local chosen=st and st[2]
  local prompt=notice~=''and notice or g.phase==1 and(g.winner==0 and'A DRAW: NOBODY COULD MOVE'or g.ending==1 and(g.winner==1 and'YOU WIN: THEIR H.Q. IS TAKEN'or'YOU LOSE: YOUR H.Q. IS TAKEN')or g.ending==2 and(g.winner==1 and'YOU WIN ON MEDALS'or'THEY WIN ON MEDALS')or(g.winner==1 and'YOU WIN: NOBODY COULD MOVE'or'YOU LOSE: NOBODY COULD MOVE'))or g.turn~=1 and'THEY ARE THINKING'or D.prompt(M,g,draft)
  L=B.draw(M,shown,chosen,nil,F,w,h,{prompt=prompt,candidates=D.candidates(M,g,draft),ask=ask,original=g,step=draft.step,steps=draft.steps})
  boxes[#boxes+1]={box=L.brief,op='brief'}
  if g.phase==1 then boxes[#boxes+1]={box=L.action,op='result'}
  elseif g.turn==1 then
   if ask=='troop'and #draft.steps==0 then boxes[#boxes+1]={box=L.action,op='draw'}
   elseif ask=='troop'or ask=='slot'then boxes[#boxes+1]={box=L.action,op='cancel'}
   else boxes[#boxes+1]={box=L.skip,op='skip'};boxes[#boxes+1]={box=L.take,op=L.targeted and'cancel'or'take'}end
  end
 end
 if notice~=''and mode~='board'then F.native(notice,{x=16,y=h-128,w=w-32,h=22},10,'center')end
end
local function activate(c)
 if not c then return end
 if c.op=='prev'then if helpPage==1 then mode='menu'else helpPage=helpPage-1 end
 elseif c.op=='next'then if helpPage==8 then mode='setup';options={map=g.map,special=g.special==1};cursor=1 else helpPage=helpPage+1 end
 else action(c.op,c.value);return end;redraw()
end
function on_touch(x,y,e)
 if closed or e~='tap'then return end
 if y<83 then on_button('back',true);return end
 for _,c in ipairs(boxes)do if F.inside(c.box,x,y)then activate(c);return end end
 if mode=='board'and g.turn==1 and g.phase==0 then
  local kind,value=B.hit(L,x,y)
  if kind=='troop'and value then if D.troop(M,g,draft,value)then afterAnswer()else notice=D.whyTroop(M,g,draft,value);redraw()end
  elseif kind=='slot'then if D.slot(M,g,draft,value)then afterAnswer()else notice=D.whySlot(M,g,draft,value);redraw()end end
 end
end
function on_button(b,p)
 if closed or not p then return end
 if b=='back'then
  if mode=='menu'then if save()then closed=true;if previousOrientation then smudge.set_orientation(previousOrientation);previousOrientation=nil end;smudge.exit()end
  elseif mode=='brief'then mode='board'
  elseif mode=='board'and #draft.steps>0 then draft=D.clear();notice=''
  elseif mode=='board'then save();mode='menu';cursor=1
  elseif mode=='maps'then mode='setup'
  elseif mode=='help'and helpPage>1 then helpPage=helpPage-1
  else mode='menu';cursor=1 end
 elseif mode=='maps'and(b=='page_back'or b=='page_forward'or b=='left'or b=='right')then mapPage=1-mapPage;cursor=1
 elseif mode=='help'then activate({op=(b=='left'or b=='up'or b=='page_back')and'prev'or'next'})
 elseif b=='confirm'then activate(boxes[cursor])
 else cursor=(cursor-1+((b=='up'or b=='left'or b=='page_back')and -1 or 1))%math.max(1,#boxes)+1 end;redraw()
end
function on_update()if not closed and mode=='board'and g.phase==0 and g.turn==2 then if M.apply(g,M.ai(g))then draft=D.clear();changed()end end end
function on_swipe(d)on_button(d=='left'and'page_forward'or d=='right'and'page_back'or d,true)end
function on_exit()if g then M.pause(g);save()end;closed=true;if previousOrientation then smudge.set_orientation(previousOrientation);previousOrientation=nil end end
return{state=function()return g end,layout=function()return L end,view=function()return{mode=mode,controls=boxes,draft=draft,options=options,help=helpPage,mapPage=mapPage}end}
